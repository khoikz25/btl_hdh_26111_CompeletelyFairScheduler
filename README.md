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
- Quản lý tiến độ: https://trello.com/invite/b/6ada1161535927772786eb13/ATTI63f2db75f7999c026ee08d270ae61ee0ACC71179/26111-cfs-xv6
- Thiết kế (Figma): https://www.figma.com/board/cly03pvkT3ury8yGiv1NSZ/CFS-xv6---Thi%E1%BA%BFt-k%E1%BA%BF?node-id=0-1&t=cDjm5uoyz1qmr3hk-1
- Ghi chú đọc mã lập lịch: docs/scheduler-notes.md

## Phiên bản xv6 gốc
c787f03, nguồn: https://github.com/mit-pdos/xv6-riscv
Các commit trước tag này thuộc lịch sử gốc của xv6, không phải đóng góp của nhóm.
   Xem riêng việc của nhóm: `git log --oneline base-xv6..main` và `git shortlog -sn base-xv6..main`
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
Mọi người dùng cùng một bản xv6 gốc (ghi ở mục “Phiên bản xv6 gốc”), không tự cập nhật bản khác<<<<<<< khoi/readme-base-note
## Nguồn code gốc

Repo dùng bản xv6-riscv của MIT (https://github.com/mit-pdos/xv6-riscv) làm nền.
Tag `base-xv6` (commit `c787f03`) đánh dấu điểm xuất phát của nhóm.
Các commit trước tag này thuộc lịch sử gốc của xv6, không phải đóng góp của nhóm.

Để xem riêng phần việc của nhóm:

    git log --oneline base-xv6..main
    git shortlog -sn base-xv6..main

## Build và chạy

    make qemu

## Cấu trúc thư mục của nhóm

- `kernel/rbtree.h`, `kernel/rbtree.c`: cây đỏ-đen dùng cho hàng đợi CFS
- `docs/`: ghi chú và tài liệu
