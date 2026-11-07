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
