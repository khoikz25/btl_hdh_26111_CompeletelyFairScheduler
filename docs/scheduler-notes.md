# Ghi chú đọc code lập lịch xv6-riscv
  Người ghi: Khôi
  Ngày 23/10/20206

## 1. Trạng thái process (kernel/proc.h)
- procstate: UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE.
- Scheduler chỉ chọn process ở trạng thái RUNNABLE.
- Các trường trong struct proc liên quan đến lập lịch: lock, state,
  chan, context. Quy tắc: giữ p->lock khi đọc/ghi state.
- struct proc chưa có trường nào về độ ưu tiên hay thời gian chạy
  -> CFS sẽ phải thêm trường mới (dự kiến: vruntime, trọng số).
## 2. Các hàm chính (kernel/proc.c)

Lưu ý phiên bản: bản này dùng kfork/kexit/kwait/kkill và cặp
sleep_prepare(chan) + sleep() (không có tham số lock như bản cũ).

  ### scheduler()
- Mỗi CPU chạy một vòng lặp scheduler riêng, không bao giờ return.
- Duyệt proc[0..NPROC), acquire p->lock, nếu RUNNABLE thì đặt
  RUNNING, c->proc = p, swtch(&c->context, &p->context).
- Sau swtch (process đã nhường CPU): c->proc = 0, release p->lock,
  đi tiếp process kế tiếp trong mảng (không quay lại đầu).
- Không có process nào chạy được thì wfi.
- Kết luận: round-robin theo vị trí mảng, không có ưu tiên.

  ### sched()
- Gọi khi process đã giữ p->lock và đã đổi state (khác RUNNING).
- Kiểm tra: đang giữ p->lock, noff == 1, ngắt đã tắt.
- swtch(&p->context, &mycpu()->context) để về scheduler; lưu/khôi
  phục intena quanh swtch.

  ### yield()
- acquire p->lock; state = RUNNABLE; sched(); release p->lock.
- Đây là cách process nhường CPU (được gọi từ trap.c, cần xác nhận).

  ### sleep_prepare(chan) / sleep() / wakeup(chan)
- sleep_prepare: đặt p->chan = chan (dưới p->lock).
- sleep: nếu p->chan còn khác 0 thì state = SLEEPING và sched();
  nếu wakeup đã xoá chan trước đó thì không ngủ (tránh mất wakeup).
- wakeup: duyệt mọi process, nếu p->chan == chan thì xoá chan và,
  nếu đang SLEEPING, đặt RUNNABLE.

  ### Vòng đời process
- allocproc: USED, context.ra = forkret, context.sp = kstack + PGSIZE.
- forkret: release p->lock (đang giữ từ scheduler), lần đầu chạy
  fsinit và kexec("/init"), rồi về user space.
- kexit: đặt ZOMBIE rồi sched(), không quay lại.
- kwait: cha tìm con ZOMBIE rồi freeproc.

  ### Các điểm đặt state = RUNNABLE (nơi process "vào hàng đợi")
userinit, kfork, yield, wakeup, kkill.
Điểm "ra khỏi hàng đợi": scheduler() đặt RUNNING.
(Liên quan trực tiếp đến thiết kế CFS với cây đỏ-đen.)
## 3. Cơ chế ngắt timer (kernel/trap.c)

Luồng khi timer hết hạn:
timer interrupt -> usertrap() (từ user) hoặc kerneltrap() (từ kernel)
-> devintr() nhận scause 0x8000000000000005 -> clockintr()
-> devintr trả về 2 -> handler gọi yield()
-> state = RUNNABLE -> sched() -> swtch về scheduler().

- clockintr(): CPU 0 tăng ticks và wakeup(&ticks); mọi CPU đặt
  hẹn lần sau bằng w_stimecmp(r_time() + 1000000).
- Time slice = khoảng cách giữa hai timer interrupt: 1000000 đơn vị
  r_time(), comment trong code ghi ~1/10 giây (chưa kiểm chứng;
  cần xem start.c).
- yield() được gọi vô điều kiện ở mỗi timer interrupt.
- usertrap(): yield khi which_dev == 2.
- kerneltrap(): yield khi which_dev == 2 và myproc() != 0 (vì
  scheduler() có thể bị ngắt khi chưa có process nào chạy).
- kerneltrap() khôi phục sepc/sstatus sau yield().
- ticks chỉ tăng ở CPU 0; mọi CPU đều nhận timer riêng.
## 4. Chuyển ngữ cảnh (kernel/proc.h)
- struct context: ra, sp, s0-s11 (chỉ callee-saved, vì swtch() được
  gọi như một hàm).
- struct cpu: proc (process đang chạy), context (context của
  scheduler trên CPU đó), noff/intena (quản lý tắt ngắt cho lock).
- Mỗi CPU có một scheduler riêng; chuyển giữa hai process luôn đi
  qua scheduler (process A -> scheduler -> process B).
- trapframe (user) khác context (kernel): trapframe lưu thanh ghi
  user khi vào trap, context lưu thanh ghi kernel khi swtch().
## 5. Kết luận cho CFS (dự kiến, nhóm chưa chốt)

Chỗ cần thay đổi nếu thay round-robin bằng CFS:
- kernel/proc.h: thêm trường vào struct proc (vruntime, trọng số).
- kernel/proc.c:
  * scheduler(): thay vòng duyệt mảng bằng chọn process có vruntime
    nhỏ nhất.
  * Các điểm vào hàng đợi (state = RUNNABLE): userinit, kfork,
    yield, wakeup, kkill -> chèn vào cấu trúc dữ liệu.
  * Điểm ra khỏi hàng đợi: scheduler() khi đặt RUNNING; kexit.
  * allocproc/kfork: khởi tạo vruntime cho process mới.
- kernel/trap.c: cập nhật vruntime của process đang chạy mỗi tick;
  quyết định có yield() hay không.
