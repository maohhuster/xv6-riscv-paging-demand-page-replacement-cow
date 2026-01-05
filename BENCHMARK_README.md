# XV6-RISCV Memory Management Benchmark Suite

## Tổng quan

Chương trình benchmark này đánh giá toàn diện 3 cơ chế quản lý bộ nhớ trong xv6-riscv:
1. **Paging (Phân trang)**: Lazy allocation và demand paging
2. **Page Replacement (Thay thế trang)**: Thuật toán FIFO
3. **Copy-on-Write (COW)**: Chia sẻ trang giữa các tiến trình

## Cấu trúc

### System Calls mới

1. **`get_mem_stats(void *stats)`**: Lấy thống kê bộ nhớ
   - Trả về số lượng trang vật lý đang sử dụng
   - Số lượng Page Faults (tổng, lazy allocation, COW)
   - Số lần Swap-in và Swap-out
   - Tổng số Disk I/O operations

2. **`reset_mem_stats(void)`**: Đặt lại tất cả các bộ đếm về 0

### Các bài test

#### Test 1: Copy-on-Write (COW) Efficiency
- **Mục đích**: Chứng minh hiệu quả của COW so với copy toàn bộ
- **Thực hiện**:
  - Fork 20 tiến trình con
  - Mỗi tiến trình đọc một mảng lớn (1MB)
  - Đo lượng RAM thực tế tiêu thụ
- **Kết quả mong đợi**: 
  - Với COW: ~1MB (1 bản copy + overhead)
  - Không có COW: ~21MB (21 bản copy)

#### Test 2: Sequential Memory Access Pattern
- **Mục đích**: Kiểm tra tốc độ swapping cơ bản với FIFO
- **Thực hiện**:
  - Cấp phát vùng nhớ lớn gấp 2 lần dung lượng RAM (~128MB)
  - Truy cập tuần tự từ đầu đến cuối
  - Mỗi trang được đọc và ghi một lần
- **Kết quả mong đợi**: 
  - FIFO hoạt động tốt với pattern này
  - Mỗi trang được swap-out sau khi sử dụng xong

#### Test 3: Locality-Based Memory Access Pattern
- **Mục đích**: Thấy sự khác biệt về hiệu suất giữa FIFO và LRU
- **Thực hiện**:
  - Cấp phát vùng nhớ lớn gấp 2 lần RAM
  - Truy cập theo nhóm: lặp lại 5 lần trên 10 trang, rồi chuyển sang nhóm tiếp theo
- **Kết quả mong đợi**:
  - FIFO có thể swap-out các trang sẽ được truy cập lại sớm
  - LRU sẽ hoạt động tốt hơn với pattern này

## Cách sử dụng

### Biên dịch

```bash
make clean
make
```

### Chạy benchmark

```bash
make qemu
# Trong xv6 shell:
$ benchmark
```

### Kết quả đầu ra

Chương trình sẽ in ra:
- **Execution Time**: Thời gian thực thi tính bằng ticks
- **Physical Pages Used**: Số trang vật lý đang sử dụng (và MB tương ứng)
- **Total Page Faults**: Tổng số page faults
  - Lazy Allocations: Số lần lazy allocation
  - COW Faults: Số lần COW page fault
  - Swap-in Operations: Số lần swap-in
- **Swap-out Operations**: Số lần swap-out
- **Total Disk I/O**: Tổng số thao tác đĩa (swap-in + swap-out)
- **Page Faults/sec**: Tốc độ page faults
- **Disk I/O/sec**: Tốc độ disk I/O

## Các chỉ số được đo

1. **Physical Pages in Use**: Số trang vật lý đang được sử dụng
2. **Page Faults**: 
   - Lazy allocation faults
   - COW faults
   - Swap-in faults
3. **Swap Operations**: 
   - Swap-in: Đọc trang từ disk vào RAM
   - Swap-out: Ghi trang từ RAM ra disk
4. **Disk I/O**: Tổng số thao tác đĩa (swap-in + swap-out)
5. **Execution Time**: Thời gian thực thi (ticks)

## Phân tích kết quả

### COW Test
- **Hiệu quả bộ nhớ**: So sánh RAM thực tế với RAM lý thuyết (không có COW)
- **COW Faults**: Số lần trang được copy khi có write

### Sequential Access
- **Swap Pattern**: FIFO swap-out các trang theo thứ tự
- **Disk I/O**: Mỗi trang được swap một lần (swap-out khi cần, swap-in khi truy cập)

### Locality Access
- **FIFO Limitation**: Có thể swap-out các trang sẽ được truy cập lại
- **Performance Impact**: Nhiều swap-in/out hơn so với LRU

## Cấu trúc mã nguồn

### Kernel
- `kernel/memstats.c`: Tracking thống kê bộ nhớ
- `kernel/sysproc.c`: System call handlers
- `kernel/trap.c`: Đếm page faults
- `kernel/swap.c`: Đếm swap operations

### User
- `user/benchmark.c`: Chương trình benchmark chính

## Lưu ý

1. Benchmark có thể mất nhiều thời gian, đặc biệt là test locality
2. Kết quả có thể khác nhau tùy vào cấu hình QEMU và RAM
3. Để có kết quả chính xác, nên chạy nhiều lần và lấy trung bình

## Mở rộng

Có thể thêm các test sau:
- **Random Access Pattern**: Truy cập ngẫu nhiên để test FIFO
- **Working Set Size**: Đo working set size của các pattern khác nhau
- **Memory Pressure**: Test dưới áp lực bộ nhớ cao
- **Multi-process**: Test với nhiều tiến trình đồng thời

