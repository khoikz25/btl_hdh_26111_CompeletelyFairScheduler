# TÀI LIỆU THIẾT KẾ BỘ LẬP LỊCH CFS (COMPLETELY FAIR SCHEDULER)

## 1.1. Các trường bổ sung trong struct proc

Bảng định nghĩa các trường bổ sung vào cấu trúc `struct proc` tại file `kernel/proc.h`:

|   Tên trường    |   Kiểu dữ liệu   | Mặc định |                      Ý nghĩa                       |
| :-------------- | :--------------- | :------- | :------------------------------------------------- |
| vruntime        | uint64           | 0        | Thời gian chạy ảo tích lũy (khóa chính trên cây)   |
| nice            | int              | 0        | Mức độ ưu tiên của tiến trình (-20 đến +19)        |
| weight          | uint64           | 1024     | Trọng số CPU tương ứng với mức nice                |
| start_tick      | uint64           | 0        | Thời điểm (ticks) bắt đầu lượt chạy hiện tại       |
| runtime_ticks   | uint64           | 0        | Tổng số ticks thực tế đã chạy trên CPU             |
| time_slice      | uint64           | 0        | Hạn ngạch ticks tối đa cho mỗi lượt chạy           |
| cfs_node        | struct rb_node   | 0        | Nút cây đỏ-đen nhúng trực tiếp vào tiến trình      |

## 1.2. Công thức tính vruntime và Ví dụ số

### a. Công thức cập nhật vruntime
Sau mỗi khoảng thời gian thực thi thực tế $\Delta t$ (tính bằng ticks: $\Delta t = \text{ticks}_{\text{hiện tại}} - \text{start\_tick}$), thời gian chạy ảo `vruntime` của tiến trình được cộng dồn theo công thức:

$$\Delta \text{vruntime} = \left\lfloor \frac{\Delta t \times W_0 \times S}{\text{weight}} \right\rfloor$$

$$\text{vruntime}_{\text{mới}} = \text{vruntime}_{\text{cũ}} + \Delta \text{vruntime}$$

Trong đó:
- $W_0 = 1024$: Trọng số chuẩn của mức ưu tiên mặc định (`nice = 0`).
- $S = 1024$: Hệ số phóng đại (scale factor) giúp tránh sai số phần thập phân khi thực hiện phép chia số nguyên `uint64` trong nhân hệ điều hành.
- $\text{weight}$: Trọng số của tiến trình tra từ bảng `nice_to_weight[40]` (ứng với `nice` từ -20 đến +19).

### b. Ví dụ số cho 2 tiến trình có mức nice khác nhau
Giả sử tiến trình P1 (`nice = 0`, $\text{weight} = 1024$) và tiến trình P2 (`nice = -5`, $\text{weight} = 3121$) cùng khởi đầu với `vruntime = 0` và cùng chạy thực tế một khoảng $\Delta t = 10\text{ ticks}$:
- **P1 (`nice = 0`):** $\Delta \text{vruntime} = \frac{10 \times 1024 \times 1024}{1024} = 10240 \implies \text{vruntime} = 10240$
- **P2 (`nice = -5`):** $\Delta \text{vruntime} = \frac{10 \times 1024 \times 1024}{3121} = 3359 \implies \text{vruntime} = 3359$

|   Tiến trình   |   Mức nice   |   Trọng số (weight)   |   Ticks thực tế ($\Delta t$)   |   vruntime tích lũy   |
| :------------- | :----------- | :-------------------- | :----------------------------- | :-------------------- |
| P1             | 0            | 1024                  | 10                             | 10240                 |
| P2             | -5           | 3121                  | 10                             | 3359                  |

**Nhận xét:** Dù cùng chạy 10 ticks vật lý như nhau, nhưng do P2 có độ ưu tiên cao hơn (`nice = -5`), `vruntime` của P2 tăng chậm hơn khoảng 3.05 lần so với P1. Nhờ đó, P2 giữ vị trí `leftmost` trên cây đỏ-đen lâu hơn và được hệ thống ưu tiên cấp phát thời gian CPU nhiều hơn.

## 1.3. Lựa chọn mô hình Hàng đợi: Toàn cục (Global) hay Theo CPU (Per-CPU)

### Lựa chọn thiết kế
Nhóm lựa chọn sử dụng **Hàng đợi toàn cục (Global Runqueue)** — duy trì một cây đỏ-đen (`struct rb_root`) duy nhất chứa toàn bộ các tiến trình ở trạng thái `RUNNABLE` trên hệ thống, được bảo vệ bởi một khóa chung (`cfs_rq.lock`) đã định nghĩa sẵn trong `kernel/rbtree.h`.

### Lý do lựa chọn
1. **Tự động cân bằng tải (Load Balancing) trên đa nhân:** Khi hệ thống chạy ở cấu hình đa nhân (`CPUS=4`), bất kỳ lõi CPU nào rảnh rỗi cũng sẽ gọi hàm `rb_pop_min(&cfs_rq)` để lấy ra tiến trình có `vruntime` nhỏ nhất toàn hệ thống với độ phức tạp $O(1)$ thông qua con trỏ `leftmost`. Nhóm không cần phải viết thêm các thuật toán di trú tiến trình (migration/work-stealing) phức tạp giữa các CPU.
2. **Đảm bảo tính công bằng tuyệt đối:** Tránh được hiện tượng một lõi CPU bị dồn quá nhiều tiến trình nặng trong khi lõi khác rảnh rỗi nhưng không thể chia sẻ tiến trình như mô hình Per-CPU.
3. **Phù hợp với giới hạn của xv6:** Với quy mô của xv6 (tối đa 64 tiến trình và tối đa 4 lõi CPU), các thao tác chèn/xóa trên cây đỏ-đen chỉ mất tối đa $\approx 6$ bước duyệt, do đó thời gian giữ khóa toàn cục rất ngắn và không gây ra hiện tượng nghẽn cổ chai (bottleneck).

## 1.4. Thứ tự lấy Lock (Lock Ordering)

Để đảm bảo hệ thống đa nhân (`CPUS=4`) không xảy ra tình trạng khóa chết (deadlock) giữa khóa bảo vệ từng tiến trình (`p->lock`) và khóa bảo vệ hàng đợi toàn cục (`cfs_rq.lock`), hệ thống tuân thủ nghiêm ngặt nguyên tắc thứ tự khóa một chiều (không có chu trình).

### a. Quy tắc bất biến
* Luôn phải **lấy `p->lock` trước**, sau đó mới **lấy `cfs_rq.lock` sau**.
* **Tuyệt đối không** được phép giữ `cfs_rq.lock` rồi đi xin `p->lock`, nhằm tránh tạo ra vòng lặp chờ khóa chéo giữa các luồng.

### b. Sơ đồ thứ tự lấy và nhả Lock

1. **Luồng đưa tiến trình vào hàng đợi RUNNABLE** (được gọi từ `yield`, `wakeup`, `fork`, `userinit`):
   ```text
   acquire(&p->lock) 
     └──► p->state = RUNNABLE 
     └──► Đồng bộ p->cfs_node.vruntime = p->vruntime, p->cfs_node.pid = p->pid
     └──► acquire(&cfs_rq.lock)
            └──► rb_insert(&cfs_rq, &p->cfs_node)
          release(&cfs_rq.lock)
   release(&p->lock)



2. **Luồng bộ lập lịch chọn tiến trình để chạy** (được gọi từ `scheduler`):
     ```text
    acquire(&cfs_rq.lock)
    └──► p = rb_pop_min(&cfs_rq)
    release(&cfs_rq.lock)   <-- Nhả khóa cây TRƯỚC khi xin p->lock
    │
    ▼ (Nếu tìm thấy tiến trình p hợp lệ)
    acquire(&p->lock)
    └──► Kiểm tra p->state == RUNNABLE
            ├──► Đúng: p->state = RUNNING ──► swtch(&c->context, &p->context)
            └──► Sai (tiến trình đã bị kill hoặc đổi trạng thái): bỏ qua
    release(&p->lock)