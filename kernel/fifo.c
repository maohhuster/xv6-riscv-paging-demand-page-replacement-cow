// FIFO page replacement algorithm implementation
// Maintains a queue of user pages in the order they were allocated

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"

// FIFO queue entry: stores (pagetable, va) pair
struct fifo_entry {
  pagetable_t pagetable;
  uint64 va;
  struct fifo_entry *next;
};

// FIFO queue structure
struct {
  struct spinlock lock;
  struct fifo_entry *head;  // Oldest page (first to be swapped out)
  struct fifo_entry *tail;  // Newest page (last added)
} fifo_queue;

// Initialize FIFO queue
void
fifoinit(void)
{
  initlock(&fifo_queue.lock, "fifo");
  fifo_queue.head = 0;
  fifo_queue.tail = 0;
}

// Add a user page to the end of FIFO queue
// Only call this for user pages (pages with PTE_U flag)
// Returns 0 on success, -1 on failure (out of memory)
int
fifo_add(pagetable_t pagetable, uint64 va)
{
  struct fifo_entry *entry;
  
  // Allocate a new entry
  entry = (struct fifo_entry *)kalloc();
  if(entry == 0)
    return -1;
  
  entry->pagetable = pagetable;
  entry->va = PGROUNDDOWN(va);
  entry->next = 0;
  
  acquire(&fifo_queue.lock);
  
  if(fifo_queue.tail == 0) {
    // Queue is empty
    fifo_queue.head = entry;
    fifo_queue.tail = entry;
  } else {
    // Add to tail
    fifo_queue.tail->next = entry;
    fifo_queue.tail = entry;
  }
  
  release(&fifo_queue.lock);
  return 0;
}

// Remove a specific page from the FIFO queue
// Called when a page is freed (e.g., process exits)
// Returns 0 if found and removed, -1 if not found
int
fifo_remove(pagetable_t pagetable, uint64 va)
{
  struct fifo_entry *prev, *curr;
  uint64 va_aligned = PGROUNDDOWN(va);
  
  acquire(&fifo_queue.lock);
  
  prev = 0;
  curr = fifo_queue.head;
  
  while(curr != 0) {
    if(curr->pagetable == pagetable && curr->va == va_aligned) {
      // Found the entry, remove it
      if(prev == 0) {
        // Removing head
        fifo_queue.head = curr->next;
        if(fifo_queue.head == 0)
          fifo_queue.tail = 0;
      } else {
        prev->next = curr->next;
        if(curr == fifo_queue.tail)
          fifo_queue.tail = prev;
      }
      
      release(&fifo_queue.lock);
      kfree((void*)curr);
      return 0;
    }
    prev = curr;
    curr = curr->next;
  }
  
  release(&fifo_queue.lock);
  return -1;  // Not found
}

// Get the oldest page (head of queue) for swapping
// Returns 0 on success (victim found and swapped out), -1 on failure
// The page is removed from the queue if successfully swapped out
int
fifo_get_victim(void)
{
  struct fifo_entry *entry;
  pagetable_t pagetable;
  uint64 va;
  pte_t *pte;
  
  acquire(&fifo_queue.lock);
  
  // Try to find a valid entry
  while(fifo_queue.head != 0) {
    entry = fifo_queue.head;
    pagetable = entry->pagetable;
    va = entry->va;
    
    // Check if the page is still valid
    // We need to find which process owns this pagetable
    pte = walk(pagetable, va, 0);
    if(pte != 0 && (*pte & PTE_V) != 0 && (*pte & PTE_U) != 0 && (*pte & PTE_COW) == 0) {
      // Valid user page, try to swap it out
      release(&fifo_queue.lock);
      
      if(swapout(pagetable, va) == 0) {
        // Successfully swapped out, remove from queue
        acquire(&fifo_queue.lock);
        fifo_queue.head = entry->next;
        if(fifo_queue.head == 0)
          fifo_queue.tail = 0;
        release(&fifo_queue.lock);
        kfree((void*)entry);
        return 0;
      } else {
        // Swap failed, remove this entry and try next
        acquire(&fifo_queue.lock);
        fifo_queue.head = entry->next;
        if(fifo_queue.head == 0)
          fifo_queue.tail = 0;
        release(&fifo_queue.lock);
        kfree((void*)entry);
        // Continue to next entry
        acquire(&fifo_queue.lock);
        continue;
      }
    } else {
      // Page is no longer valid (already swapped, freed, or COW)
      // Remove from queue and try next
      fifo_queue.head = entry->next;
      if(fifo_queue.head == 0)
        fifo_queue.tail = 0;
      release(&fifo_queue.lock);
      kfree((void*)entry);
      acquire(&fifo_queue.lock);
    }
  }
  
  release(&fifo_queue.lock);
  return -1;  // No valid victim found
}

