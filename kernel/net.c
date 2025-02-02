#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;



// Structure to track packets queued for a bound port
struct udp_sock {
  int port;               // Port in host byte order
  struct spinlock lock;   // Protects the queue
  char *pkts[16];        // Queue of packet buffers
  uint len[16];          // Length of each packet
  int head;              // Index for next packet to receive
  int tail;              // Index for next packet to queue
  int count;             // Number of packets currently queued
  int bound;             // Whether this port is bound
};

#define NSOCK 32  // Maximum number of bound ports
struct {
  struct spinlock lock;      // Protects socks array
  struct udp_sock socks[NSOCK];
} udp_table;

// Find a socket for the given port
static struct udp_sock*
get_sock(int port)
{
  for(int i = 0; i < NSOCK; i++) {
    if(udp_table.socks[i].bound && udp_table.socks[i].port == port)
      return &udp_table.socks[i];
  }
  return 0;
}

uint64
sys_bind(void)
{
  int port;
  argint(0, &port);

  acquire(&udp_table.lock);
  
  // Find free socket
  struct udp_sock *sock = 0;
  for(int i = 0; i < NSOCK; i++) {
    if(!udp_table.socks[i].bound) {
      sock = &udp_table.socks[i];
      break;
    }
  }

  if(!sock) {
    release(&udp_table.lock);
    return -1;
  }

  // Initialize socket
  sock->port = port;
  sock->head = 0;
  sock->tail = 0;
  sock->count = 0;
  sock->bound = 1;
  
  release(&udp_table.lock);
  return 0;
}






void
netinit(void)
{
  initlock(&netlock, "netlock");

  initlock(&udp_table.lock, "udp_table");
  for (int i = 0; i < NSOCK; i++) {
    initlock(&udp_table.socks[i].lock, "udp_socket");
    udp_table.socks[i].bound = 0;
  }
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  int dport;
  uint64 src_addr, sport_addr, buf;
  int maxlen;

  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf);
  argint(4, &maxlen);

  acquire(&udp_table.lock);
  struct udp_sock *sock = get_sock(dport);
  if(!sock) {
    release(&udp_table.lock);
    return -1;
  }

  acquire(&sock->lock);
  release(&udp_table.lock);

  // Wait for packet
  while(sock->count == 0) {
    sleep(sock, &sock->lock);
  }

  // Get packet from queue
  char *pkt = sock->pkts[sock->head];
  sock->head = (sock->head + 1) % 16;
  sock->count--;

  // Extract headers
  struct ip *iphdr = (struct ip*)(pkt + sizeof(struct eth));
  struct udp *udphdr = (struct udp*)(pkt + sizeof(struct eth) + sizeof(struct ip));

  // Convert to host byte order for user
  int src = ntohl(iphdr->ip_src);
  short sport = ntohs(udphdr->sport);

  // Copy out data to user space
  if(copyout(myproc()->pagetable, src_addr, (char*)&src, sizeof(int)) < 0 ||
     copyout(myproc()->pagetable, sport_addr, (char*)&sport, sizeof(short)) < 0) {
    kfree(pkt);
    release(&sock->lock);
    return -1;
  }

  // Copy packet payload
  char *payload = pkt + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  int len = ntohs(udphdr->ulen) - sizeof(struct udp);
  if(len > maxlen)
    len = maxlen;
  if(copyout(myproc()->pagetable, buf, payload, len) < 0) {
    kfree(pkt);
    release(&sock->lock);
    return -1;
  }

  kfree(pkt);
  release(&sock->lock);
  return len;
}


// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  if(len < sizeof(struct eth) + sizeof(struct ip))
    return;

  struct ip *iphdr = (struct ip*)(buf + sizeof(struct eth));
  if(iphdr->ip_p != IPPROTO_UDP)
    return;

  if(len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp))
    return;

  struct udp *udphdr = (struct udp*)(buf + sizeof(struct eth) + sizeof(struct ip));
  int dport = ntohs(udphdr->dport);  // Convert to host byte order to match bound ports

  acquire(&udp_table.lock);
  struct udp_sock *sock = get_sock(dport);
  if(!sock) {
    release(&udp_table.lock);
    return;
  }

  acquire(&sock->lock);
  release(&udp_table.lock);

  if(sock->count >= 16) {
    release(&sock->lock);
    return;
  }

  // Allocate and copy packet
  char *pkt = kalloc();
  if(!pkt) {
    release(&sock->lock);
    return;
  }
  memmove(pkt, buf, len);

  // Queue packet
  sock->pkts[sock->tail] = pkt;
  sock->len[sock->tail] = len;
  sock->tail = (sock->tail + 1) % 16;
  sock->count++;

  wakeup(sock);
  release(&sock->lock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
