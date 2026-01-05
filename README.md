# xv6-riscv-mit-o1

This repository contains implementations of **Copy-on-Write (COW) fork** and **Swapping (Demand Paging)** framework for xv6-riscv, along with implementations of different page replacement algorithms.

## Features

### ✅ Implemented
- **Copy-on-Write (COW) Fork**: Efficient fork implementation that shares pages between parent and child processes, copying only when needed
- **Swapping Framework**: Demand paging system that swaps pages to/from disk when memory is low
- **Reference Counting**: Tracks physical page usage to prevent premature deallocation

### 🔄 In Progress
- **Page Replacement Algorithms**: Three branches for implementing different algorithms:
  - `page-replacement-lru`: Least Recently Used algorithm
  - `page-replacement-clock`: Clock/Second-Chance algorithm  
  - `page-replacement-fifo`: First In First Out algorithm

## Repository Structure

- **`main` branch**: Base implementation with COW fork and basic swapping framework
- **`page-replacement-lru`**: Branch for LRU algorithm implementation
- **`page-replacement-clock`**: Branch for Clock algorithm implementation
- **`page-replacement-fifo`**: Branch for FIFO algorithm implementation

## Building and Running

Follow the standard xv6 build instructions:

```bash
make clean
make
make qemu
```

## Implementation Details

### Copy-on-Write (COW) Fork
- Pages are shared between parent and child processes
- Marked with `PTE_COW` flag (bit 8)
- Copied on first write access
- Reference counting ensures pages are freed only when no longer referenced

### Swapping Framework
- Swap space located after filesystem (starting from block `FSSIZE`)
- Pages swapped out are marked with `PTE_SWAPPED` flag (bit 9)
- Swap block number stored in PTE (bits 10-41)
- Automatic swap-out when `kalloc()` runs out of memory
- Page fault handler swaps pages back in when accessed

## Files Modified/Added

- `kernel/riscv.h`: Added `PTE_COW` and `PTE_SWAPPED` flags
- `kernel/kalloc.c`: Added reference counting and swap-out trigger
- `kernel/vm.c`: Modified `uvmcopy()` for COW, added `cowfault()` and swap handling
- `kernel/trap.c`: Added handlers for COW and swap page faults
- `kernel/swap.c`: **New file** - Swap block management and page replacement framework
- `kernel/proc.c`: Initialize swap system

## License

This project is based on xv6-riscv from MIT. See [LICENSE](LICENSE) file for details.

Copyright (c) 2006-2024 Frans Kaashoek, Robert Morris, Russ Cox, Massachusetts Institute of Technology

## Acknowledgments

This implementation extends the xv6-riscv operating system with advanced memory management features. Original xv6-riscv acknowledgments are in the [README](README) file.

