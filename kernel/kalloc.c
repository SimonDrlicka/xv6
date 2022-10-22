// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define MAXPAGE (PHYSTOP >> 12)

int page_reference[MAXPAGE];
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 *cntref;
} kmem;

void
kinit()
{
  //int frames = 0;
  //uint64 addr = PGROUNDUP((uint64)end);

  //kmem.cntref = (uint64*)addr;
  //while(addr < PHYSTOP){
  //  kmem.cntref[PA2IND(addr)] = 1;
  //  addr += PGSIZE;
  //  frames++;
  //}
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  //if(dec_ref(pa)!=0) return;

  if(page_reference[(uint64) pa >>12] > 0)
    page_reference[(uint64) pa >>12]--;

  if(page_reference[(uint64) pa >>12] !=0) return;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    page_reference[(uint64)r >>12] =1;
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
    //kmem.cntref[PA2IND((uint64)r)] = 1;
}

int dec_ref(void* pa){
  //acquire(&kmem.lock);
  //uint64 n = kmem.cntref[PA2IND(pa)];
  //if(n == 0) return n;
  //else{
  //  kmem.cntref[PA2IND(pa)]--;
  //}
  // release(&kmem.lock);

  int n = page_reference[(uint64) pa >>12];
  if(n >=1){
    --n;
    page_reference[(uint64) pa >>12]--;
  }
  return n;
}

void inc_ref(uint64 pa){ 
   page_reference[(uint64) pa >>12]++;
}
