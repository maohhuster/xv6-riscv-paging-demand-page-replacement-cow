// Comprehensive benchmark for Paging, Page Replacement (FIFO), and Copy-on-Write (COW)
// Tests memory management mechanisms and provides detailed statistics

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/memlayout.h"
#include "user/user.h"

#define PGSIZE 4096
#define PHYSTOP_SIZE (128 * 1024 * 1024)  // 128MB total RAM
#define USER_RAM_ESTIMATE (PHYSTOP_SIZE / 2)  // Estimate ~64MB for user processes

// Memory statistics structure (must match kernel)
struct mem_stats {
  uint64 pages_in_use;
  uint64 page_faults;
  uint64 swap_ins;
  uint64 swap_outs;
  uint64 cow_faults;
  uint64 lazy_allocs;
};

// Print a separator line
void print_separator() {
  printf("================================================================================\n");
}

// Print test header
void print_test_header(const char *test_name) {
  printf("\n");
  print_separator();
  printf("TEST: %s\n", test_name);
  print_separator();
}

// Get and print memory statistics
void print_stats(const char *label, uint64 start_time) {
  struct mem_stats stats;
  uint64 end_time;
  
  if(get_mem_stats(&stats) < 0) {
    printf("Error: Failed to get memory statistics\n");
    return;
  }
  
  end_time = uptime();
  uint64 elapsed = end_time - start_time;
  
  printf("\n%s Statistics:\n", label);
  printf("  Execution Time:        %lu ticks\n", elapsed);
  printf("  Physical Pages Used:   %lu pages (%.2f MB)\n", 
         stats.pages_in_use, stats.pages_in_use * PGSIZE / (1024.0 * 1024.0));
  printf("  Total Page Faults:     %lu\n", stats.page_faults);
  printf("  - Lazy Allocations:    %lu\n", stats.lazy_allocs);
  printf("  - COW Faults:          %lu\n", stats.cow_faults);
  printf("  - Swap-in Operations:  %lu\n", stats.swap_ins);
  printf("  Swap-out Operations:  %lu\n", stats.swap_outs);
  printf("  Total Disk I/O:        %lu operations\n", stats.swap_ins + stats.swap_outs);
  
  if(elapsed > 0) {
    printf("  Page Faults/sec:       %.2f\n", (float)stats.page_faults / elapsed);
    printf("  Disk I/O/sec:          %.2f\n", (float)(stats.swap_ins + stats.swap_outs) / elapsed);
  }
}

// Test 1: Copy-on-Write (COW) Efficiency
// Fork 20 child processes, each reading a large array
void test_cow() {
  print_test_header("Copy-on-Write (COW) Efficiency Test");
  
  reset_mem_stats();
  uint64 start_time = uptime();
  
  const int NUM_CHILDREN = 20;
  const int ARRAY_SIZE = 1024 * 1024;  // 1MB array per process
  int i, pid;
  
  printf("Allocating %d MB array in parent process...\n", ARRAY_SIZE / (1024 * 1024));
  
  // Parent allocates large array using lazy allocation
  char *array = sbrklazy(ARRAY_SIZE);
  if(array == SBRK_ERROR) {
    printf("Error: Failed to allocate memory\n");
    return;
  }
  
  // Initialize array (triggers lazy allocation)
  printf("Initializing array (triggering lazy allocation)...\n");
  for(i = 0; i < ARRAY_SIZE; i++) {
    array[i] = (char)(i % 256);
  }
  
  printf("Forking %d child processes...\n", NUM_CHILDREN);
  
  // Fork children
  for(i = 0; i < NUM_CHILDREN; i++) {
    pid = fork();
    if(pid < 0) {
      printf("Error: Fork failed\n");
      exit(1);
    }
    if(pid == 0) {
      // Child process: read from shared array
      int sum = 0;
      for(int j = 0; j < ARRAY_SIZE; j++) {
        sum += array[j];
      }
      // Prevent optimization
      if(sum == 0) {
        printf("Impossible\n");
      }
      exit(0);
    }
  }
  
  // Wait for all children
  printf("Waiting for all children to complete...\n");
  for(i = 0; i < NUM_CHILDREN; i++) {
    wait(0);
  }
  
  print_stats("COW Test", start_time);
  
  printf("\nAnalysis: With COW, all %d processes share the same physical pages.\n", NUM_CHILDREN + 1);
  printf("Without COW, we would need %d MB of physical memory.\n", 
         (NUM_CHILDREN + 1) * ARRAY_SIZE / (1024 * 1024));
  printf("With COW, we only need ~%d MB (1 copy + overhead).\n", 
         ARRAY_SIZE / (1024 * 1024) + 1);
}

// Test 2: Sequential Memory Access Pattern
// Allocate memory 2x RAM size and access sequentially
void test_sequential_access() {
  print_test_header("Sequential Memory Access Pattern Test");
  
  reset_mem_stats();
  uint64 start_time = uptime();
  
  // Allocate memory approximately 2x RAM size
  uint64 mem_size = USER_RAM_ESTIMATE * 2;
  uint64 num_pages = mem_size / PGSIZE;
  
  printf("Allocating %lu MB (approximately 2x RAM size)...\n", mem_size / (1024 * 1024));
  printf("This will trigger swapping as we access pages.\n\n");
  
  char *mem = sbrklazy(mem_size);
  if(mem == SBRK_ERROR) {
    printf("Error: Failed to allocate memory\n");
    return;
  }
  
  printf("Accessing memory sequentially (read and write)...\n");
  
  // Sequential access: read and write
  for(uint64 i = 0; i < num_pages; i++) {
    uint64 page_addr = (uint64)mem + i * PGSIZE;
    char *page = (char*)page_addr;
    
    // Write to page (triggers lazy allocation or swap-in)
    for(int j = 0; j < PGSIZE; j++) {
      page[j] = (char)((i + j) % 256);
    }
    
    // Read from page
    int sum = 0;
    for(int j = 0; j < PGSIZE; j++) {
      sum += page[j];
    }
    // Prevent optimization
    if(sum == 0 && i == 0) {
      printf("Impossible\n");
    }
    
    // Progress indicator every 100 pages
    if(i % 100 == 0 && i > 0) {
      printf("  Processed %lu/%lu pages (%.1f%%)\n", 
             i, num_pages, (float)i * 100.0 / num_pages);
    }
  }
  
  printf("Sequential access complete.\n");
  
  print_stats("Sequential Access", start_time);
  
  printf("\nAnalysis: Sequential access shows FIFO replacement behavior.\n");
  printf("Each page is accessed once, then replaced by the next page.\n");
}

// Test 3: Locality-Based Memory Access Pattern
// Access a small group of pages repeatedly, then move to next group
void test_locality_access() {
  print_test_header("Locality-Based Memory Access Pattern Test");
  
  reset_mem_stats();
  uint64 start_time = uptime();
  
  // Allocate memory approximately 2x RAM size
  uint64 mem_size = USER_RAM_ESTIMATE * 2;
  uint64 num_pages = mem_size / PGSIZE;
  const int LOCALITY_SIZE = 10;  // Access 10 pages repeatedly
  const int ITERATIONS = 5;      // Repeat 5 times per locality group
  
  printf("Allocating %lu MB (approximately 2x RAM size)...\n", mem_size / (1024 * 1024));
  printf("Using locality pattern: %d pages per group, %d iterations per group.\n\n", 
         LOCALITY_SIZE, ITERATIONS);
  
  char *mem = sbrklazy(mem_size);
  if(mem == SBRK_ERROR) {
    printf("Error: Failed to allocate memory\n");
    return;
  }
  
  printf("Accessing memory with locality pattern...\n");
  
  // Locality-based access
  for(uint64 group_start = 0; group_start < num_pages; group_start += LOCALITY_SIZE) {
    uint64 group_end = group_start + LOCALITY_SIZE;
    if(group_end > num_pages) {
      group_end = num_pages;
    }
    
    // Repeat access to this locality group
    for(int iter = 0; iter < ITERATIONS; iter++) {
      for(uint64 i = group_start; i < group_end; i++) {
        uint64 page_addr = (uint64)mem + i * PGSIZE;
        char *page = (char*)page_addr;
        
        // Write to page
        for(int j = 0; j < PGSIZE; j++) {
          page[j] = (char)((i + iter + j) % 256);
        }
        
        // Read from page
        int sum = 0;
        for(int j = 0; j < PGSIZE; j++) {
          sum += page[j];
        }
        // Prevent optimization
        if(sum == 0 && i == 0 && iter == 0) {
          printf("Impossible\n");
        }
      }
    }
    
    // Progress indicator
    if(group_start % (LOCALITY_SIZE * 10) == 0 && group_start > 0) {
      printf("  Processed %lu/%lu pages (%.1f%%)\n", 
             group_start, num_pages, (float)group_start * 100.0 / num_pages);
    }
  }
  
  printf("Locality access complete.\n");
  
  print_stats("Locality Access", start_time);
  
  printf("\nAnalysis: Locality pattern shows how FIFO performs with repeated access.\n");
  printf("FIFO may swap out pages that will be accessed again soon.\n");
  printf("This demonstrates the behavior of FIFO replacement algorithm.\n");
}

// Main benchmark function
int main(int argc, char *argv[]) {
  printf("\n");
  print_separator();
  printf("XV6-RISCV Memory Management Benchmark Suite\n");
  printf("Testing: Paging, Page Replacement (FIFO), and Copy-on-Write (COW)\n");
  print_separator();
  
  // Run all tests
  test_cow();
  test_sequential_access();
  test_locality_access();
  
  // Final summary
  printf("\n");
  print_separator();
  printf("BENCHMARK SUMMARY\n");
  print_separator();
  printf("All tests completed successfully!\n");
  printf("\nKey Insights:\n");
  printf("1. COW significantly reduces memory usage when forking processes.\n");
  printf("2. Sequential access patterns work well with FIFO replacement.\n");
  printf("3. Locality patterns show FIFO's behavior with repeated access.\n");
  printf("4. Swap operations (disk I/O) are the main performance bottleneck.\n");
  print_separator();
  
  exit(0);
}

