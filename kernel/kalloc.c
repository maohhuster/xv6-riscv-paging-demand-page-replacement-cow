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
} kmem;

// Reference count for each physical page
// Indexed by physical page number: (pa >> 12)
struct {
  struct spinlock lock;
  int count[(PHYSTOP - KERNBASE) / PGSIZE];
} refcount;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcount.lock, "refcount");
  memset(refcount.count, 0, sizeof(refcount.count));
  freerange(end, (void*)PHYSTOP);
}

// Get page index from physical address
static int
pageindex(void *pa)
{
  if((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("pageindex: invalid pa");
  return ((uint64)pa - KERNBASE) / PGSIZE;
}

// Increment reference count for a physical page
void
krefinc(void *pa)
{
  if((uint64)pa % PGSIZE != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("krefinc: invalid pa");
  
  acquire(&refcount.lock);
  refcount.count[pageindex(pa)]++;
  release(&refcount.lock);
}

// Decrement reference count for a physical page
// Returns the new reference count
int
krefdec(void *pa)
{
  if((uint64)pa % PGSIZE != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("krefdec: invalid pa");
  
  acquire(&refcount.lock);
  if(refcount.count[pageindex(pa)] <= 0)
    panic("krefdec: refcount <= 0");
  refcount.count[pageindex(pa)]--;
  int count = refcount.count[pageindex(pa)];
  release(&refcount.lock);
  return count;
}

// Get reference count for a physical page
int
krefget(void *pa)
{
  if((uint64)pa % PGSIZE != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("krefget: invalid pa");
  
  acquire(&refcount.lock);
  int count = refcount.count[pageindex(pa)];
  release(&refcount.lock);
  return count;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // Initialize ref_count to 0 for pages in freerange
    // They will be set to 1 when allocated
    acquire(&refcount.lock);
    refcount.count[pageindex(p)] = 0;
    release(&refcount.lock);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Decrement reference count
  int count = krefdec(pa);
  
  // Only free the page if reference count reaches 0
  if(count > 0)
    return;

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
// If out of memory, tries to swap out a page.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  // If no free page, try to swap out a page
  if(r == 0) {
    if(select_victim_page() == 0) {
      // Successfully swapped out a page, try again
      acquire(&kmem.lock);
      r = kmem.freelist;
      if(r)
        kmem.freelist = r->next;
      release(&kmem.lock);
    }
  }

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // Initialize reference count to 1
    acquire(&refcount.lock);
    refcount.count[pageindex(r)] = 1;
    release(&refcount.lock);
  }
  return (void*)r;
}
