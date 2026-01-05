// Benchmark program to test COW fork and Swapping performance
// Measures various memory management statistics

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STATS_SIZE 12

// Structure to hold statistics
struct mem_stats {
  uint64 cow_faults;
  uint64 cow_pages_shared;
  uint64 swap_outs;
  uint64 swap_ins;
  uint64 swap_blocks_used;
  uint64 page_faults;
  uint64 load_faults;
  uint64 store_faults;
  uint64 lazy_alloc_faults;
  uint64 pages_allocated;
  uint64 pages_freed;
  uint64 current_pages;
};

void print_stats(struct mem_stats *before, struct mem_stats *after, const char *test_name)
{
  struct mem_stats diff;
  
  diff.cow_faults = after->cow_faults - before->cow_faults;
  diff.cow_pages_shared = after->cow_pages_shared - before->cow_pages_shared;
  diff.swap_outs = after->swap_outs - before->swap_outs;
  diff.swap_ins = after->swap_ins - before->swap_ins;
  diff.swap_blocks_used = after->swap_blocks_used;
  diff.page_faults = after->page_faults - before->page_faults;
  diff.load_faults = after->load_faults - before->load_faults;
  diff.store_faults = after->store_faults - before->store_faults;
  diff.lazy_alloc_faults = after->lazy_alloc_faults - before->lazy_alloc_faults;
  diff.pages_allocated = after->pages_allocated - before->pages_allocated;
  diff.pages_freed = after->pages_freed - before->pages_freed;
  diff.current_pages = after->current_pages;
  
  printf("\n=== %s ===\n", test_name);
  printf("COW Statistics:\n");
  printf("  COW page faults:     %d\n", diff.cow_faults);
  printf("  Pages shared (COW):  %d\n", diff.cow_pages_shared);
  printf("\nSwap Statistics:\n");
  printf("  Pages swapped out:   %d\n", diff.swap_outs);
  printf("  Pages swapped in:    %d\n", diff.swap_ins);
  printf("  Swap blocks in use:  %d\n", diff.swap_blocks_used);
  printf("\nPage Fault Statistics:\n");
  printf("  Total page faults:   %d\n", diff.page_faults);
  printf("  Load faults:         %d\n", diff.load_faults);
  printf("  Store faults:        %d\n", diff.store_faults);
  printf("  Lazy alloc faults:   %d\n", diff.lazy_alloc_faults);
  printf("\nMemory Statistics:\n");
  printf("  Pages allocated:     %d\n", diff.pages_allocated);
  printf("  Pages freed:         %d\n", diff.pages_freed);
  printf("  Current pages:       %d\n", diff.current_pages);
  printf("========================\n\n");
}

void get_stats(struct mem_stats *stats)
{
  uint64 raw_stats[STATS_SIZE];
  if(memstats(raw_stats) < 0) {
    printf("Error getting statistics\n");
    exit(1);
  }
  
  stats->cow_faults = raw_stats[0];
  stats->cow_pages_shared = raw_stats[1];
  stats->swap_outs = raw_stats[2];
  stats->swap_ins = raw_stats[3];
  stats->swap_blocks_used = raw_stats[4];
  stats->page_faults = raw_stats[5];
  stats->load_faults = raw_stats[6];
  stats->store_faults = raw_stats[7];
  stats->lazy_alloc_faults = raw_stats[8];
  stats->pages_allocated = raw_stats[9];
  stats->pages_freed = raw_stats[10];
  stats->current_pages = raw_stats[11];
}

// Test 1: COW Fork Performance
void test_cow_fork(int num_forks, int pages_per_process)
{
  struct mem_stats before, after;
  int i, pid;
  char *mem;
  uint64 start_time, end_time;
  
  printf("Test 1: COW Fork Performance\n");
  printf("Creating %d processes, each with %d pages\n", num_forks, pages_per_process);
  
  get_stats(&before);
  start_time = uptime();
  
  // Allocate memory in parent
  mem = sbrk(pages_per_process * 4096);
  if(mem == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }
  
  // Write to memory to ensure pages are allocated
  for(i = 0; i < pages_per_process * 4096; i++) {
    mem[i] = i % 256;
  }
  
  // Fork multiple times
  for(i = 0; i < num_forks; i++) {
    pid = fork();
    if(pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if(pid == 0) {
      // Child: modify some pages to trigger COW
      if(i % 2 == 0) {
        mem[0] = 0xFF;
        mem[4096] = 0xAA;
      }
      exit(0);
    }
  }
  
  // Wait for all children
  for(i = 0; i < num_forks; i++) {
    wait(0);
  }
  
  end_time = uptime();
  get_stats(&after);
  
  printf("Time elapsed: %d ticks\n", end_time - start_time);
  print_stats(&before, &after, "COW Fork Test");
}

// Test 2: Memory Pressure and Swapping
void test_swapping(int target_pages)
{
  struct mem_stats before, after;
  char *mem;
  int i;
  uint64 start_time, end_time;
  
  printf("Test 2: Swapping Under Memory Pressure\n");
  printf("Allocating %d pages to trigger swapping\n", target_pages);
  
  get_stats(&before);
  start_time = uptime();
  
  // Allocate large amount of memory to trigger swapping
  mem = sbrk(target_pages * 4096);
  if(mem == (char*)-1) {
    printf("sbrk failed (memory exhausted)\n");
  } else {
    // Write to all pages
    for(i = 0; i < target_pages * 4096; i += 4096) {
      mem[i] = i / 4096;
    }
    
    // Read back to trigger swap-in if pages were swapped out
    for(i = 0; i < target_pages * 4096; i += 4096) {
      volatile char c = mem[i];
      (void)c; // prevent optimization
    }
  }
  
  end_time = uptime();
  get_stats(&after);
  
  printf("Time elapsed: %d ticks\n", end_time - start_time);
  print_stats(&before, &after, "Swapping Test");
}

// Test 3: COW Write Performance
void test_cow_writes(int num_writes)
{
  struct mem_stats before, after;
  char *mem;
  int i, pid;
  uint64 start_time, end_time;
  
  printf("Test 3: COW Write Performance\n");
  printf("Forking and performing %d COW writes\n", num_writes);
  
  // Allocate memory
  mem = sbrk(10 * 4096);
  if(mem == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }
  
  // Initialize memory
  for(i = 0; i < 10 * 4096; i++) {
    mem[i] = 0;
  }
  
  get_stats(&before);
  start_time = uptime();
  
  pid = fork();
  if(pid < 0) {
    printf("fork failed\n");
    exit(1);
  }
  
  if(pid == 0) {
    // Child: write to pages to trigger COW
    for(i = 0; i < num_writes; i++) {
      mem[(i % 10) * 4096] = i;
    }
    exit(0);
  } else {
    // Parent: also write to different pages
    for(i = 0; i < num_writes; i++) {
      mem[((i + 5) % 10) * 4096] = i + 100;
    }
    wait(0);
  }
  
  end_time = uptime();
  get_stats(&after);
  
  printf("Time elapsed: %d ticks\n", end_time - start_time);
  print_stats(&before, &after, "COW Write Test");
}

// Test 4: Overall System Statistics
void test_overall_stats(void)
{
  struct mem_stats stats;
  
  printf("Test 4: Overall System Statistics\n");
  get_stats(&stats);
  
  printf("\n=== Overall System Statistics ===\n");
  printf("COW Statistics:\n");
  printf("  Total COW faults:        %d\n", stats.cow_faults);
  printf("  Total pages shared:      %d\n", stats.cow_pages_shared);
  printf("\nSwap Statistics:\n");
  printf("  Total swap outs:         %d\n", stats.swap_outs);
  printf("  Total swap ins:          %d\n", stats.swap_ins);
  printf("  Current swap blocks:     %d\n", stats.swap_blocks_used);
  printf("\nPage Fault Statistics:\n");
  printf("  Total page faults:       %d\n", stats.page_faults);
  printf("  Load faults:             %d\n", stats.load_faults);
  printf("  Store faults:            %d\n", stats.store_faults);
  printf("  Lazy alloc faults:       %d\n", stats.lazy_alloc_faults);
  printf("\nMemory Statistics:\n");
  printf("  Total pages allocated:   %d\n", stats.pages_allocated);
  printf("  Total pages freed:       %d\n", stats.pages_freed);
  printf("  Current pages in use:    %d\n", stats.current_pages);
  printf("================================\n\n");
}

int main(int argc, char *argv[])
{
  printf("\n");
  printf("========================================\n");
  printf("xv6 Memory Management Benchmark Suite\n");
  printf("========================================\n");
  printf("Testing COW Fork and Swapping\n");
  printf("========================================\n\n");
  
  // Test 1: COW Fork
  test_cow_fork(10, 5);
  pause(10); // Small delay between tests
  
  // Test 2: Swapping
  test_swapping(100);
  pause(10);
  
  // Test 3: COW Writes
  test_cow_writes(20);
  pause(10);
  
  // Test 4: Overall Statistics
  test_overall_stats();
  
  printf("Benchmark completed!\n");
  exit(0);
}

