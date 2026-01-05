// Memory management statistics
// Tracks COW, swapping, and page fault statistics

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "defs.h"

// Global statistics
struct {
  struct spinlock lock;
  
  // COW statistics
  uint64 cow_faults;        // Number of COW page faults
  uint64 cow_pages_shared;  // Number of pages shared via COW
  
  // Swap statistics
  uint64 swap_outs;         // Number of pages swapped out
  uint64 swap_ins;          // Number of pages swapped in
  uint64 swap_blocks_used;  // Current number of swap blocks in use
  
  // Page fault statistics
  uint64 page_faults;       // Total page faults (load + store)
  uint64 load_faults;       // Load page faults
  uint64 store_faults;      // Store page faults
  uint64 lazy_alloc_faults; // Lazy allocation page faults
  
  // Memory statistics
  uint64 pages_allocated;   // Total pages allocated via kalloc
  uint64 pages_freed;       // Total pages freed via kfree
  uint64 current_pages;     // Current number of allocated pages
  
} memstats;

void
memstats_init(void)
{
  initlock(&memstats.lock, "memstats");
  memstats.cow_faults = 0;
  memstats.cow_pages_shared = 0;
  memstats.swap_outs = 0;
  memstats.swap_ins = 0;
  memstats.swap_blocks_used = 0;
  memstats.page_faults = 0;
  memstats.load_faults = 0;
  memstats.store_faults = 0;
  memstats.lazy_alloc_faults = 0;
  memstats.pages_allocated = 0;
  memstats.pages_freed = 0;
  memstats.current_pages = 0;
}

void
memstats_inc_cow_fault(void)
{
  acquire(&memstats.lock);
  memstats.cow_faults++;
  release(&memstats.lock);
}

void
memstats_inc_cow_shared(void)
{
  acquire(&memstats.lock);
  memstats.cow_pages_shared++;
  release(&memstats.lock);
}

void
memstats_inc_swap_out(void)
{
  acquire(&memstats.lock);
  memstats.swap_outs++;
  memstats.swap_blocks_used++;
  release(&memstats.lock);
}

void
memstats_inc_swap_in(void)
{
  acquire(&memstats.lock);
  memstats.swap_ins++;
  if(memstats.swap_blocks_used > 0)
    memstats.swap_blocks_used--;
  release(&memstats.lock);
}

void
memstats_inc_page_fault(int is_load)
{
  acquire(&memstats.lock);
  memstats.page_faults++;
  if(is_load)
    memstats.load_faults++;
  else
    memstats.store_faults++;
  release(&memstats.lock);
}

void
memstats_inc_lazy_alloc(void)
{
  acquire(&memstats.lock);
  memstats.lazy_alloc_faults++;
  release(&memstats.lock);
}

void
memstats_inc_pages_allocated(void)
{
  acquire(&memstats.lock);
  memstats.pages_allocated++;
  memstats.current_pages++;
  release(&memstats.lock);
}

void
memstats_inc_pages_freed(void)
{
  acquire(&memstats.lock);
  memstats.pages_freed++;
  if(memstats.current_pages > 0)
    memstats.current_pages--;
  release(&memstats.lock);
}

// Get all statistics
void
memstats_get(uint64 *stats)
{
  acquire(&memstats.lock);
  stats[0] = memstats.cow_faults;
  stats[1] = memstats.cow_pages_shared;
  stats[2] = memstats.swap_outs;
  stats[3] = memstats.swap_ins;
  stats[4] = memstats.swap_blocks_used;
  stats[5] = memstats.page_faults;
  stats[6] = memstats.load_faults;
  stats[7] = memstats.store_faults;
  stats[8] = memstats.lazy_alloc_faults;
  stats[9] = memstats.pages_allocated;
  stats[10] = memstats.pages_freed;
  stats[11] = memstats.current_pages;
  release(&memstats.lock);
}

// Reset all statistics
void
memstats_reset(void)
{
  acquire(&memstats.lock);
  memstats.cow_faults = 0;
  memstats.cow_pages_shared = 0;
  memstats.swap_outs = 0;
  memstats.swap_ins = 0;
  memstats.swap_blocks_used = 0;
  memstats.page_faults = 0;
  memstats.load_faults = 0;
  memstats.store_faults = 0;
  memstats.lazy_alloc_faults = 0;
  memstats.pages_allocated = 0;
  memstats.pages_freed = 0;
  memstats.current_pages = 0;
  release(&memstats.lock);
}

