#ifndef XV6_RBTREE_H
#define XV6_RBTREE_H

#include "types.h"
#include "spinlock.h"

// Tiền khai báo struct proc để liên kết với kernel/proc.h
struct proc;

// 1. Định nghĩa màu của nút trên Cây Đỏ-Đen
enum rb_color {
  RB_RED = 0,
  RB_BLACK = 1
};

// 2. Cấu trúc một nút của Cây Đỏ-Đen (Nhúng vào trong mỗi struct proc)
struct rb_node {
  struct rb_node *parent;   // Con trỏ trỏ tới nút cha
  struct rb_node *left;     // Con trỏ trỏ tới cây con trái (vruntime nhỏ hơn)
  struct rb_node *right;    // Con trỏ trỏ tới cây con phải (vruntime lớn hơn)
  enum rb_color color;      // Màu của nút (RB_RED hoặc RB_BLACK)

  // Khóa sắp xếp trên cây (Key)
  uint64 vruntime;          // Khóa chính: Thời gian chạy ảo của tiến trình
  int pid;                  // Khóa phụ: Phân biệt khi 2 tiến trình có cùng vruntime

  // Dữ liệu liên kết tiến trình
  struct proc *proc;        // Con trỏ trỏ ngược về tiến trình sở hữu nút này
  int on_rq;                // Cờ đánh dấu: 1 nếu đang nằm trên cây, 0 nếu ở ngoài
};

// 3. Cấu trúc gốc quản lý toàn bộ Cây Đỏ-Đen (CFS Runqueue)
struct rb_root {
  struct rb_node *root;     // Nút gốc của cây
  struct rb_node *leftmost; // Trỏ trực tiếp tới nút có vruntime nhỏ nhất -> lấy min O(1)
  int nr_running;           // Tổng số tiến trình RUNNABLE hiện có trên cây
  uint64 min_vruntime;      // Giá trị vruntime nhỏ nhất của hệ thống hiện tại
  struct spinlock lock;     // Khóa đồng bộ khi nhiều CPU cùng truy cập cây
};

// ============================================================================
// CÁC HÀM THAO TÁC CỐT LÕI (insert, delete, min)
// ============================================================================

// Khởi tạo cây đỏ-đen rỗng và spinlock
void rb_init(struct rb_root *tree);

// Khởi tạo một nút trước khi đưa vào cây
void rb_node_init(struct rb_node *node, struct proc *p);

// 1. INSERT: Chèn một nút tiến trình vào cây đỏ-đen và tự cân bằng
void rb_insert(struct rb_root *tree, struct rb_node *node);

// 2. DELETE: Xóa một nút tiến trình khỏi cây đỏ-đen và tự cân bằng
void rb_delete(struct rb_root *tree, struct rb_node *node);

// 3. MIN: Trả về nút có vruntime nhỏ nhất (nút ngoài cùng bên trái)
struct rb_node* rb_min(struct rb_root *tree);

// Lấy ra và xóa luôn tiến trình có vruntime nhỏ nhất khỏi cây
struct proc* rb_pop_min(struct rb_root *tree);

// ============================================================================
// HÀM PHỤ TRỢ & KIỂM THỬ (UNIT TEST)
// ============================================================================

// Hàm so sánh thứ tự 2 nút dựa trên vruntime (và pid nếu vruntime bằng nhau)
static inline int rb_compare(const struct rb_node *a, const struct rb_node *b) {
  if (a->vruntime != b->vruntime)
    return (a->vruntime < b->vruntime) ? -1 : 1;
  return (a->pid < b->pid) ? -1 : (a->pid > b->pid ? 1 : 0);
}

// Hàm kiểm tra 5 tính chất của Cây Đỏ-Đen phục vụ Unit Test
int rb_verify_properties(struct rb_root *tree);

#endif // XV6_RBTREE_H
