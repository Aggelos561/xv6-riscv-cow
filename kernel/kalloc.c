// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"


//Struct fot a spinlock and the reference counter array
struct pageInfo{
  struct spinlock lock;
  int ref_counter[PHYSTOP / PGSIZE];
}reference_counter;


void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    //Initialize reference counter array for every page
    reference_counter.ref_counter[(uint64) p / PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  r = (struct run *)pa;
  acquire(&reference_counter.lock);
  int pn = (uint64) r / PGSIZE;

  //Decrease Reference count
  reference_counter.ref_counter[pn] -= 1;
  int counter = reference_counter.ref_counter[pn];
  release(&reference_counter.lock);

  //If reference counter is not 0 simply return 
  if (counter > 0)
    return;

  //If reference counter is 0 then add the memory page to the free list (Do what the kfree does)

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

  if(r){

    //Getting page number
    int pn = (uint64) r / PGSIZE;

    reference_counter.ref_counter[pn] = 1;
    kmem.freelist = r->next;

  }
  
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


void increase_ref_counter(uint64 pa) {

  //acquire reference counter lock
  acquire(&reference_counter.lock);

  if (pa > PHYSTOP) {
    panic("PHYSTOP Error in increase_ref_counter");
  }

  //Finding page number
  int pn = pa / PGSIZE;

  //Increase reference counter
  reference_counter.ref_counter[pn]++;

  // release reference counter lock
  release(&reference_counter.lock);

}