// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKNUM 17
struct bucket{
  struct spinlock lock;
  struct buf start;
};

struct {
  struct spinlock lock;
  struct bucket buck[BUCKNUM];
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;




uint64
hash(uint64 dev, uint64 blockno) {
    uint64 hash_value = dev;
    hash_value ^= blockno + 0x9e3779b97f4a7c15ULL + (hash_value << 6) + (hash_value >> 2);
    return hash_value % BUCKNUM;
}



void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0 ; i < BUCKNUM;i++){
    char lock_name[24];
    snprintf(lock_name, sizeof(lock_name), "bcache-lock-%d", i);
    initlock(&bcache.buck[i].lock, lock_name); 
    bcache.buck[i].start.next = &bcache.buck[i].start;
    bcache.buck[i].start.prev = &bcache.buck[i].start;

  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int buck = (b-bcache.buf)%BUCKNUM;
    b->prev = &bcache.buck[buck].start;
    b->next = bcache.buck[buck].start.next;
    initsleeplock(&b->lock, "buffer");

    bcache.buck[buck].start.next->prev = b;
    bcache.buck[buck].start.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint64 bucket = hash(dev,blockno);
  acquire(&bcache.buck[bucket].lock);

  // Is the block already cached?
  for(b = bcache.buck[bucket].start.next; b != &bcache.buck[bucket].start; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buck[bucket].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.buck[bucket].start.prev; b != &bcache.buck[bucket].start; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.buck[bucket].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buck[bucket].lock);
  acquire(&bcache.lock);
  for(int i=1;i < BUCKNUM;i++){
    int bu = (i+bucket)%BUCKNUM;
    acquire(&bcache.buck[bu].lock);
    for(b = bcache.buck[bu].start.prev;b!=&bcache.buck[bu].start;b=b->prev){
      if(b->refcnt==0){
        b->prev->next = b->next;
        b->next->prev = b->prev;
        
        release(&bcache.buck[bu].lock);
        acquire(&bcache.buck[bucket].lock);

        b->prev=&bcache.buck[bucket].start;
        b->next=bcache.buck[bucket].start.next;
        bcache.buck[bucket].start.next->prev=b;

        bcache.buck[bucket].start.next=b;
        
        
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        release(&bcache.buck[bucket].lock);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.buck[bu].lock);
  }
  release(&bcache.lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  uint64 bucket = hash(b->dev,b->blockno);
  acquire(&bcache.buck[bucket].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->prev->next = b->next;
    b->next->prev = b->prev;
   
    b->next = bcache.buck[bucket].start.next;
    b->prev = &bcache.buck[bucket].start;
    bcache.buck[bucket].start.next->prev = b;
    bcache.buck[bucket].start.next = b;
  }
  
  release(&bcache.buck[bucket].lock);
}

void
bpin(struct buf *b) {
  uint64 bucket = hash(b->dev,b->blockno);
  acquire(&bcache.buck[bucket].lock);
  b->refcnt++;
  release(&bcache.buck[bucket].lock);
}

void
bunpin(struct buf *b) {
  uint64 bucket = hash(b->dev,b->blockno);
  acquire(&bcache.buck[bucket].lock);
  b->refcnt--;
  release(&bcache.buck[bucket].lock);
}


