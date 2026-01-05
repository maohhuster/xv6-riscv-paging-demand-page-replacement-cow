// Swap (Demand Paging) implementation
// Uses virtio disk to store swapped-out pages

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "buf.h"
#include "sleeplock.h"

extern struct superblock sb;

// Swap space starts after filesystem
// We use blocks after FSSIZE for swap
#define SWAP_START_BLOCK (FSSIZE)
#define SWAP_MAX_BLOCKS  (1000)  // Maximum swap blocks (can be adjusted)
#define SWAP_END_BLOCK   (SWAP_START_BLOCK + SWAP_MAX_BLOCKS)

// Swap block management
struct {
  struct spinlock lock;
  char inuse[SWAP_MAX_BLOCKS / 8 + 1];  // Bitmap for swap blocks
  uint nfree;                            // Number of free swap blocks
} swap;

// Initialize swap system
void
swapinit(void)
{
  initlock(&swap.lock, "swap");
  memset(swap.inuse, 0, sizeof(swap.inuse));
  swap.nfree = SWAP_MAX_BLOCKS;
}

// Allocate a swap block
// Returns block number (relative to SWAP_START_BLOCK) or -1 if out of space
static int
swapalloc(void)
{
  acquire(&swap.lock);
  
  if(swap.nfree == 0) {
    release(&swap.lock);
    return -1;
  }
  
  // Find first free block
  for(int i = 0; i < SWAP_MAX_BLOCKS; i++) {
    int byte = i / 8;
    int bit = i % 8;
    if((swap.inuse[byte] & (1 << bit)) == 0) {
      swap.inuse[byte] |= (1 << bit);
      swap.nfree--;
      release(&swap.lock);
      return i;
    }
  }
  
  release(&swap.lock);
  return -1;
}

// Free a swap block
void
swapfree(int swapblock)
{
  if(swapblock < 0 || swapblock >= SWAP_MAX_BLOCKS)
    panic("swapfree: invalid swapblock");
  
  acquire(&swap.lock);
  int byte = swapblock / 8;
  int bit = swapblock % 8;
  if((swap.inuse[byte] & (1 << bit)) == 0)
    panic("swapfree: double free");
  swap.inuse[byte] &= ~(1 << bit);
  swap.nfree++;
  release(&swap.lock);
}

// Get absolute block number on disk
static uint
swapblock_to_diskblock(int swapblock)
{
  return SWAP_START_BLOCK + swapblock;
}

// Extract swap block number from PTE
// Swap block number is stored in bits 10-41 of PTE (32 bits)
int
pte_to_swapblock(pte_t pte)
{
  return (pte >> 10) & 0xFFFFFFFF;
}

// Create PTE with swap block number
// Store swap block in bits 10-41, set PTE_SWAPPED, clear PTE_V
static pte_t
swapblock_to_pte(int swapblock)
{
  return ((uint64)swapblock << 10) | PTE_SWAPPED;
}

// Swap out a page to disk
// Returns 0 on success, -1 on failure
int
swapout(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;
  struct buf *b;
  int swapblock;
  
  va = PGROUNDDOWN(va);
  
  if((pte = walk(pagetable, va, 0)) == 0)
    return -1;
  
  if((*pte & PTE_V) == 0)
    return -1;
  
  // Don't swap COW pages - they should be copied first
  if((*pte & PTE_COW) != 0)
    return -1;
  
  // Don't swap kernel pages
  if((*pte & PTE_U) == 0)
    return -1;
  
  // Allocate a swap block
  if((swapblock = swapalloc()) < 0)
    return -1;
  
  pa = PTE2PA(*pte);
  
  // Write page to swap block
  b = bread(ROOTDEV, swapblock_to_diskblock(swapblock));
  memmove(b->data, (char*)pa, PGSIZE);
  bwrite(b);
  brelse(b);
  
  // Update PTE: store swap block number, set PTE_SWAPPED, clear PTE_V
  *pte = swapblock_to_pte(swapblock);
  
  // Free the physical page
  kfree((void*)pa);
  
  // Increment swap-out counter
  memstats_inc_swapout();
  
  return 0;
}

// Swap in a page from disk
// Returns physical address on success, 0 on failure
uint64
swapin(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  struct buf *b;
  int swapblock;
  void *pa;
  
  va = PGROUNDDOWN(va);
  
  if((pte = walk(pagetable, va, 0)) == 0)
    return 0;
  
  if((*pte & PTE_SWAPPED) == 0)
    return 0;
  
  // Extract swap block number
  swapblock = pte_to_swapblock(*pte);
  
  // Allocate a physical page
  if((pa = kalloc()) == 0)
    return 0;
  
  // Read page from swap block
  b = bread(ROOTDEV, swapblock_to_diskblock(swapblock));
  memmove((char*)pa, b->data, PGSIZE);
  brelse(b);
  
  // Free the swap block
  swapfree(swapblock);
  
  // Restore original PTE flags (without PTE_SWAPPED)
  uint flags = (*pte & 0x3FF) & ~PTE_SWAPPED;
  *pte = PA2PTE((uint64)pa) | flags | PTE_V;
  
  // Add to FIFO queue (user page swapped back in)
  fifo_add(pagetable, va);
  
  // Increment swap-in counter
  memstats_inc_swapin();
  
  return (uint64)pa;
}

// Select a victim page to swap out
// Uses FIFO algorithm: selects the oldest page (head of FIFO queue)
// Returns 0 on success, -1 on failure
int
select_victim_page(void)
{
  // Use FIFO queue to get the oldest page
  return fifo_get_victim();
}

