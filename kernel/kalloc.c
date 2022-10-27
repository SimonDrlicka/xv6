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

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 *cntref;
} kmem;

uint64
dec_ref(uint64 pa){
  acquire(&kmem.lock);
  if(kmem.cntref[PA2IND(pa)] == 0) {
    release(&kmem.lock);
    panic("dec_ref");
  }
  kmem.cntref[PA2IND(pa)] -= 1;
  release(&kmem.lock);
  return (kmem.cntref[PA2IND(pa)]); 
}

void
inc_ref(uint64 pa){
  acquire(&kmem.lock);
  kmem.cntref[PA2IND(pa)] += 1;
  release(&kmem.lock);
}

void
kinit()
{
  int frames = 0;
  uint64 addr = PGROUNDUP((uint64)end);

  kmem.cntref = (uint64*)addr;
  while(addr < PHYSTOP) {
    kmem.cntref[PA2IND(addr)] = 1;
    addr += PGSIZE;
    frames++;
  }

  initlock(&kmem.lock, "kmem");
  freerange(kmem.cntref+frames, (void*)PHYSTOP);
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
  //printf("predKfree");
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  uint64 res = dec_ref((uint64)pa);
  if(res != 0)
    return;
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
  //printf("kfree");
}


static void
int_ref_internal(uint64 pa) {
  kmem.cntref[PA2IND(pa)]++;
}

int
get_pa(uint64 pa){
  return(kmem.cntref[PA2IND(pa)]);
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
  if(r){
    kmem.freelist = r->next;
    int_ref_internal((uint64)r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  //printf("work");
  return (void*)r;
}
