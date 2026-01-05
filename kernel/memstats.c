// Memory statistics tracking for benchmarking
// Tracks page faults, swap operations, and physical page usage

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "kalloc.h"

// Global memory statistics
struct {
  struct spinlock lock;
  uint64 page_faults;        // Total page faults (lazy allocation + swap-in)
  uint64 swap_ins;           // Number of swap-in operations
  uint64 swap_outs;          // Number of swap-out operations
  uint64 cow_faults;         // Number of COW page faults
  uint64 lazy_allocs;        // Number of lazy allocation page faults
} memstats;

// Initialize memory statistics
void
memstats_init(void)
{
  initlock(&memstats.lock, "memstats");
  memstats.page_faults = 0;
  memstats.swap_ins = 0;
  memstats.swap_outs = 0;
  memstats.cow_faults = 0;
  memstats.lazy_allocs = 0;
}

// Increment page fault counter
void
memstats_inc_pagefault(void)
{
  acquire(&memstats.lock);
  memstats.page_faults++;
  release(&memstats.lock);
}

// Increment swap-in counter
void
memstats_inc_swapin(void)
{
  acquire(&memstats.lock);
  memstats.swap_ins++;
  release(&memstats.lock);
}

// Increment swap-out counter
void
memstats_inc_swapout(void)
{
  acquire(&memstats.lock);
  memstats.swap_outs++;
  release(&memstats.lock);
}

// Increment COW fault counter
void
memstats_inc_cowfault(void)
{
  acquire(&memstats.lock);
  memstats.cow_faults++;
  release(&memstats.lock);
}

// Increment lazy allocation counter
void
memstats_inc_lazyalloc(void)
{
  acquire(&memstats.lock);
  memstats.lazy_allocs++;
  release(&memstats.lock);
}

// Get current memory statistics
// Returns number of physical pages in use
uint64
memstats_get(uint64 *page_faults, uint64 *swap_ins, uint64 *swap_outs, 
             uint64 *cow_faults, uint64 *lazy_allocs)
{
  uint64 pages_in_use = 0;
  
  // Count physical pages in use using kalloc function
  pages_in_use = kalloc_count_used();
  
  // Get statistics
  acquire(&memstats.lock);
  if(page_faults)
    *page_faults = memstats.page_faults;
  if(swap_ins)
    *swap_ins = memstats.swap_ins;
  if(swap_outs)
    *swap_outs = memstats.swap_outs;
  if(cow_faults)
    *cow_faults = memstats.cow_faults;
  if(lazy_allocs)
    *lazy_allocs = memstats.lazy_allocs;
  release(&memstats.lock);
  
  return pages_in_use;
}

// Reset all statistics counters
void
memstats_reset(void)
{
  acquire(&memstats.lock);
  memstats.page_faults = 0;
  memstats.swap_ins = 0;
  memstats.swap_outs = 0;
  memstats.cow_faults = 0;
  memstats.lazy_allocs = 0;
  release(&memstats.lock);
}

