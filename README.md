# btl_hdh_26111_CompeletelyFairScheduler
# btl_hdh_26111_CompeletelyFairScheduler
# CFS cho xv6-riscv

Bài tập lớn môn Hệ điều hành [mã lớp], đề #12: Completely Fair
Scheduler.

## Thành viên
| Họ tên | MSSV | Vai trò |
|---|---|---|
| Nguyễn Đình Sơn | 20233620 | ... |
| Hoàng Mạnh Cường | 20233292 | ... |
| Hoàng Tuấn Khôi | 20233468 | ... |

## Mục tiêu
Thay scheduler round-robin của xv6-riscv bằng CFS (chọn process
có vruntime nhỏ nhất, hàng đợi RUNNABLE dùng cây đỏ-đen).

## Liên kết
- Quản lý tiến độ: [link GitHub Project]
- Thiết kế (Figma): [link]
- Ghi chú đọc mã lập lịch: docs/scheduler-notes.md
- Báo cáo: [link hoặc thư mục reports/]

## Phiên bản xv6 gốc
[commit hoặc ngày clone], nguồn: [link repo xv6-riscv]

## Cách build và chạy
(Ubuntu, QEMU)
- Cài công cụ: [lệnh cài toolchain riscv64 và qemu]
- Build và chạy: `make qemu`
- Chạy kiểm thử: chạy `usertests` trong shell của xv6

## Cấu trúc thư mục
- kernel/: mã nguồn kernel (phần nhóm sửa nằm chủ yếu ở proc.c,
  proc.h, trap.c)
- user/: chương trình người dùng và test
- docs/: ghi chú, sơ đồ

## Quy ước làm việc
Không push thẳng lên main. Mỗi việc làm trên một nhánh riêng (ví dụ feature/vruntime, docs/figma), xong thì tạo Pull Request cho một bạn khác xem trước khi gộp. Làm vậy tránh một người làm hỏng bản xv6 đang chạy được của cả nhóm.
Chỉ gộp khi build được và usertests không tệ hơn baseline. Quy tắc này quan trọng nhất với đề này, vì scheduler hỏng thì cả hệ thống không khởi động.
Mọi người dùng cùng một bản xv6 gốc (ghi ở mục “Phiên bản xv6 gốc”), không tự cập nhật bản khác
