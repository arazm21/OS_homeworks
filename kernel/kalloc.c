// // Physical memory allocator, for user processes,
// // kernel stacks, page-table pages,
// // and pipe buffers. Allocates whole 4096-byte pages.

// #include "types.h"
// #include "param.h"
// #include "memlayout.h"
// #include "spinlock.h"
// #include "riscv.h"
// #include "defs.h"
// #include "proc.h"
// void freerange(void *pa_start, void *pa_end);

// extern char end[]; // first address after kernel.
//                    // defined by kernel.ld.

// struct run {
//   struct run *next;
// };

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;
// struct {
//   struct spinlock cpuLOCK;
//   struct run* freelist;
//   char name[16];
// } allCPUs[NCPU];
// void
// kinit()
// {
//   for(int i = 0; i < NCPU;i++){
//     char lock_name[16]; // Sufficient size for "lock-" and a few digits
//     snprintf(lock_name, sizeof(lock_name), "lock-%d", i);
//     initlock(&allCPUs[i].cpuLOCK,lock_name);
//   }
//   initlock(&kmem.lock, "kmem");
//   freerange(end, (void*)PHYSTOP);
// }

// void
// freerange(void *pa_start, void *pa_end)
// {
//   char *p;
//   p = (char*)PGROUNDUP((uint64)pa_start);
//   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
//     kfree(p);
// }

// // Free the page of physical memory pointed at by pa,
// // which normally should have been returned by a
// // call to kalloc().  (The exception is when
// // initializing the allocator; see kinit above.)
// void
// kfree(void *pa)
// {
//   struct run *r;

//   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
//     panic("kfree");

//   // Fill with junk to catch dangling refs.
//   memset(pa, 1, PGSIZE);

//   r = (struct run*)pa;
//   push_off();
//   int id = cpuid();
//   pop_off();

//   acquire(&allCPUs[id].cpuLOCK);
//   r->next = allCPUs[id].freelist;
//   allCPUs[id].freelist = r;
//   release(&allCPUs[id].cpuLOCK);
// }

// // Allocate one 4096-byte page of physical memory.
// // Returns a pointer that the kernel can use.
// // Returns 0 if the memory cannot be allocated.

// void *
// kalloc(void)
// {
  
//   push_off();
//   int currentCPU = cpuid();
//   pop_off();
//   struct run *r;
//   r=allCPUs[currentCPU].freelist;

//   if(r)
//     allCPUs[currentCPU].freelist=r->next;

//   if(!r){
//     r = borrow(currentCPU);
//     if(r){
//       allCPUs[currentCPU].freelist=r->next;


//     }else{
//       return 0;
//     }
//   }

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }

// struct run* borrow(int OGCPU){
//   struct run *half, *full, *head;
//   for(int i = 1;i < NCPU;i++){
//     int newCPU = (OGCPU+i) % NCPU;
//     acquire(&allCPUs[newCPU].cpuLOCK);
    

//     if(allCPUs[newCPU].freelist){
//       head = allCPUs[newCPU].freelist;
//       half=head;
//       full=head;
//       while(1){
//         if(full==0)break;
//         full = full->next;
//         half = half->next;
//         if(full==0)break;
//         full = full->next;
//       }
//       allCPUs[newCPU].freelist = half->next;
//       half->next=0;
//       release(&allCPUs[newCPU].cpuLOCK);
//       return head;
//     }
//     release(&allCPUs[newCPU].cpuLOCK);

//   }
//   return 0;
// } 

    // Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem mems[NCPU];

void
kinit()
{
  int size = ((char*) PHYSTOP - (char*) end) / NCPU;

  for(int i = 0; i < NCPU; i++){
    char lock_name[16]; // Sufficient size for "lock-" and a few digits
    snprintf(lock_name, sizeof(lock_name), "lock-%d", i);
    initlock(&(mems[i].lock), lock_name); 
    void *pa_start = (void*)(end + i * size);
    void *pa_end = (void*)(end + (i + 1) * size);

    if (i == NCPU - 1)
      pa_end = (void*) PHYSTOP;

    freerange(pa_start, pa_end);
  }
}

void
kfree_all(void* pa, struct kmem* k){
  struct run *r = (struct run*) pa;
  memset(pa, 1, PGSIZE);

  acquire(&k->lock);
  r->next = k->freelist;
  k->freelist = r;
  release(&k->lock);
}



void
freerange(void *pa_start, void *pa_end)
{
  push_off();
  int id = cpuid();
  char *p = (char*)PGROUNDUP((uint64)pa_start);

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_all(p, &mems[id]);


  pop_off();
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);


  push_off();
  int id = cpuid(); 
  pop_off();
  r = (struct run*)pa;


  struct kmem *k = &mems[id];
  acquire(&k->lock);
  r->next = k->freelist;
  k->freelist = r;
  release(&k->lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int id = cpuid();
  //printf("%d",id);
  
  pop_off();

  struct kmem* k = &mems[id];
  struct run* r;
  acquire(&k->lock);
  r = k->freelist;
  if(r){
    k->freelist = r->next;
    release(&k->lock);
    memset((char*) r, 5, PGSIZE);
    return (void*) r;
  }
  release(&k->lock);

  
  for(int i = 1; i < NCPU; i++){
    int newInt = (i + id)%NCPU;

    struct kmem* kk = &mems[newInt];
    acquire(&kk->lock);
    
    r = kk->freelist;

    if(r){
      kk->freelist = r->next;
      release(&kk->lock);
      memset((char*)r, 5, PGSIZE);
      return (void*)r;
    }
    release(&kk->lock);
  }


  return 0; // no memory allocated
}

struct run* borrow(int OGCPU){
  struct run *half, *full, *head;
  for(int i = 1;i < NCPU;i++){
    int newCPU = (OGCPU+i) % NCPU;
    acquire(&mems[newCPU].lock);
    

    if(mems[newCPU].freelist){
      head = mems[newCPU].freelist;
      half=head;
      full=head;
      while(1){
        if(full==0)break;
        full = full->next;
        half = half->next;
        if(full==0)break;
        full = full->next;
      }
      mems[newCPU].freelist = half->next;
      half->next=0;
      release(&mems[newCPU].lock);
      return head;
    }
    release(&mems[newCPU].lock);

  }
  return 0;
} 












