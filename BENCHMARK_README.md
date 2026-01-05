# Benchmark Suite cho COW Fork và Swapping

## Tổng quan

Benchmark suite này được thiết kế để đo lường và đánh giá hiệu năng của các cơ chế:
- **Copy-on-Write (COW) Fork**: Đo số lượng COW page faults và pages được chia sẻ
- **Swapping (Demand Paging)**: Đo số lượng swap operations và memory pressure
- **Page Faults**: Theo dõi các loại page faults (load, store, lazy allocation)

## Cấu trúc

### Kernel Components

1. **`kernel/memstats.c`**: Module thống kê memory management
   - Track COW faults, swap operations, page faults
   - Thread-safe với spinlock
   - Cung cấp API để lấy và reset statistics

2. **System Call `memstats`**: 
   - Số hiệu: `SYS_memstats` (22)
   - Trả về 12 thống kê về memory management

### User Program

**`user/benchmark.c`**: Chương trình benchmark chính

## Các Test Cases

### Test 1: COW Fork Performance
- Tạo nhiều processes và đo COW page faults
- Đo thời gian fork với COW
- Kiểm tra số lượng pages được chia sẻ

### Test 2: Swapping Under Memory Pressure
- Allocate lượng memory lớn để trigger swapping
- Đo số lượng swap operations
- Kiểm tra swap blocks usage

### Test 3: COW Write Performance
- Fork và write vào shared pages
- Đo số lượng COW faults khi write
- So sánh performance với/không có COW

### Test 4: Overall System Statistics
- Hiển thị tổng quan tất cả statistics
- Useful để monitor system state

## Statistics Tracked

1. **COW Statistics**:
   - `cow_faults`: Số lượng COW page faults
   - `cow_pages_shared`: Số lượng pages được chia sẻ qua COW

2. **Swap Statistics**:
   - `swap_outs`: Số lượng pages swapped out
   - `swap_ins`: Số lượng pages swapped in
   - `swap_blocks_used`: Số swap blocks đang sử dụng

3. **Page Fault Statistics**:
   - `page_faults`: Tổng số page faults
   - `load_faults`: Load page faults (scause=13)
   - `store_faults`: Store page faults (scause=15)
   - `lazy_alloc_faults`: Lazy allocation page faults

4. **Memory Statistics**:
   - `pages_allocated`: Tổng số pages đã allocate
   - `pages_freed`: Tổng số pages đã free
   - `current_pages`: Số pages hiện tại đang sử dụng

## Cách sử dụng

### Build

```bash
make clean
make
```

### Chạy Benchmark

```bash
make qemu
# Trong xv6 shell:
benchmark
```

### Output Format

Benchmark sẽ in ra:
- Thời gian thực thi (ticks)
- Statistics trước và sau mỗi test
- Diff statistics cho mỗi test
- Overall system statistics

## Ví dụ Output

```
========================================
xv6 Memory Management Benchmark Suite
========================================
Testing COW Fork and Swapping
========================================

Test 1: COW Fork Performance
Creating 10 processes, each with 5 pages
Time elapsed: 150 ticks

=== COW Fork Test ===
COW Statistics:
  COW page faults:     5
  Pages shared (COW):  50
...
```

## Tùy chỉnh Tests

Có thể chỉnh sửa parameters trong `benchmark.c`:

```c
// Test 1: Số processes và pages
test_cow_fork(10, 5);  // 10 processes, 5 pages each

// Test 2: Số pages để trigger swapping
test_swapping(100);    // Allocate 100 pages

// Test 3: Số lượng COW writes
test_cow_writes(20);   // 20 write operations
```

## So sánh với Page Replacement Algorithms

Khi implement các thuật toán page replacement (LRU, Clock, FIFO), có thể so sánh:
- Số lượng swap operations
- Thời gian thực thi
- Memory efficiency

## Lưu ý

- Statistics được track globally (toàn hệ thống)
- Reset statistics bằng cách restart kernel
- Một số statistics có thể không chính xác 100% do race conditions (nhưng đủ để benchmark)

## Future Enhancements

- Per-process statistics
- Real-time monitoring
- CSV export for analysis
- Comparison mode giữa các algorithms

