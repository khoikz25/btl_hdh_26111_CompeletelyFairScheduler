// Cây đỏ-đen cho bộ lập lịch CFS (cài đặt cho kernel/rbtree.h của nhóm).
//
// Quy ước quan trọng:
//  - Các hàm ở đây KHÔNG tự khóa tree->lock. Người gọi (scheduler, wakeup...)
//    tự acquire/release theo thứ tự lock đã chốt ở T1.4.
//  - Không dùng malloc/printf/libc nên build được cả trong kernel lẫn gcc thường.
//  - Mỗi nút phải có (vruntime, pid) khác nhau. Muốn đổi vruntime của một nút
//    thì rb_delete, đổi giá trị, rồi rb_insert lại.

#include "rbtree.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

// Có sẵn trong kernel/spinlock.c; khai báo lại để file này không phải
// include defs.h (nhờ vậy còn compile được trên Ubuntu để test).
void initlock(struct spinlock *, char *);

// a có đứng trước b không?
static int
rb_less(struct rb_node *a, struct rb_node *b)
{
  return rb_compare(a, b) < 0;
}

// NULL (lá NIL) được coi là đen.
static int
is_red(struct rb_node *n)
{
  return n != NULL && n->color == RB_RED;
}

static int
is_black(struct rb_node *n)
{
  return n == NULL || n->color == RB_BLACK;
}

void
rb_init(struct rb_root *tree)
{
  tree->root = NULL;
  tree->leftmost = NULL;
  tree->nr_running = 0;
  tree->min_vruntime = 0;
  initlock(&tree->lock, "rbtree");
}

// Sau hàm này người gọi phải tự đặt node->vruntime và node->pid
// (lấy từ proc) rồi mới rb_insert.
void
rb_node_init(struct rb_node *node, struct proc *p)
{
  node->parent = NULL;
  node->left = NULL;
  node->right = NULL;
  node->color = RB_RED;
  node->vruntime = 0;
  node->pid = 0;
  node->proc = p;
  node->on_rq = 0;
}

// 2.1: nút nhỏ nhất, O(1). Cây rỗng trả NULL.
struct rb_node *
rb_min(struct rb_root *tree)
{
  return tree->leftmost;
}

// Xoay trái tại x (y = con phải của x đi lên thay x).
static void
rotate_left(struct rb_root *t, struct rb_node *x)
{
  struct rb_node *y = x->right;

  x->right = y->left;
  if(y->left != NULL)
    y->left->parent = x;
  y->parent = x->parent;
  if(x->parent == NULL)
    t->root = y;
  else if(x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;
  y->left = x;
  x->parent = y;
}

// Xoay phải tại x (y = con trái của x đi lên thay x).
static void
rotate_right(struct rb_root *t, struct rb_node *x)
{
  struct rb_node *y = x->left;

  x->left = y->right;
  if(y->right != NULL)
    y->right->parent = x;
  y->parent = x->parent;
  if(x->parent == NULL)
    t->root = y;
  else if(x == x->parent->right)
    x->parent->right = y;
  else
    x->parent->left = y;
  y->right = x;
  x->parent = y;
}

// 2.3: sửa cây sau khi chèn nút n (n đang đỏ).
static void
insert_fixup(struct rb_root *t, struct rb_node *n)
{
  while(n != t->root && is_red(n->parent)){
    struct rb_node *p = n->parent;
    struct rb_node *g = p->parent;   // cha đỏ thì không thể là gốc => g tồn tại
    struct rb_node *u;               // bác (anh em của cha)

    if(p == g->left){
      u = g->right;
      if(is_red(u)){                 // TH1: bác đỏ => đổi màu, đi lên
        p->color = RB_BLACK;
        u->color = RB_BLACK;
        g->color = RB_RED;
        n = g;
      } else {
        if(n == p->right){           // TH2: gấp khúc => xoay về thẳng hàng
          n = p;
          rotate_left(t, n);
          p = n->parent;
        }
        p->color = RB_BLACK;         // TH3: thẳng hàng
        g->color = RB_RED;
        rotate_right(t, g);
      }
    } else {                         // đối xứng
      u = g->left;
      if(is_red(u)){
        p->color = RB_BLACK;
        u->color = RB_BLACK;
        g->color = RB_RED;
        n = g;
      } else {
        if(n == p->left){
          n = p;
          rotate_right(t, n);
          p = n->parent;
        }
        p->color = RB_BLACK;
        g->color = RB_RED;
        rotate_left(t, g);
      }
    }
  }
  t->root->color = RB_BLACK;
}

// 2.2: chèn node (đã rb_node_init, đã đặt vruntime và pid).
// Nút đã nằm trên cây (on_rq = 1) thì bỏ qua, tránh làm hỏng cây.
void
rb_insert(struct rb_root *tree, struct rb_node *node)
{
  struct rb_node *p = NULL;
  struct rb_node *cur = tree->root;
  int is_leftmost = 1;               // đi toàn bên trái => nút mới là min

  if(node->on_rq)
    return;

  while(cur != NULL){
    p = cur;
    if(rb_less(node, cur)){
      cur = cur->left;
    } else {
      cur = cur->right;
      is_leftmost = 0;
    }
  }

  node->parent = p;
  node->left = NULL;
  node->right = NULL;
  node->color = RB_RED;

  if(p == NULL)
    tree->root = node;
  else if(rb_less(node, p))
    p->left = node;
  else
    p->right = node;

  if(is_leftmost)
    tree->leftmost = node;
  tree->nr_running++;
  node->on_rq = 1;

  insert_fixup(tree, node);
}

// 2.5: sửa cây sau khi xóa một nút đen.
// x là nút thế chỗ (có thể NULL), parent là cha của x.
static void
delete_fixup(struct rb_root *t, struct rb_node *x, struct rb_node *parent)
{
  struct rb_node *w;   // anh em của x

  while(x != t->root && is_black(x)){
    if(x == parent->left){
      w = parent->right;
      if(is_red(w)){                           // TH1: anh em đỏ
        w->color = RB_BLACK;
        parent->color = RB_RED;
        rotate_left(t, parent);
        w = parent->right;
      }
      if(is_black(w->left) && is_black(w->right)){   // TH2
        w->color = RB_RED;
        x = parent;
        parent = x->parent;
      } else {
        if(is_black(w->right)){                // TH3
          w->left->color = RB_BLACK;
          w->color = RB_RED;
          rotate_right(t, w);
          w = parent->right;
        }
        w->color = parent->color;              // TH4
        parent->color = RB_BLACK;
        if(w->right != NULL)
          w->right->color = RB_BLACK;
        rotate_left(t, parent);
        x = t->root;
        break;
      }
    } else {                                   // đối xứng
      w = parent->left;
      if(is_red(w)){
        w->color = RB_BLACK;
        parent->color = RB_RED;
        rotate_right(t, parent);
        w = parent->left;
      }
      if(is_black(w->left) && is_black(w->right)){
        w->color = RB_RED;
        x = parent;
        parent = x->parent;
      } else {
        if(is_black(w->left)){
          w->right->color = RB_BLACK;
          w->color = RB_RED;
          rotate_left(t, w);
          w = parent->left;
        }
        w->color = parent->color;
        parent->color = RB_BLACK;
        if(w->left != NULL)
          w->left->color = RB_BLACK;
        rotate_right(t, parent);
        x = t->root;
        break;
      }
    }
  }
  if(x != NULL)
    x->color = RB_BLACK;
}

// Cho cha của old trỏ sang nw (nw có thể NULL).
static void
replace_child(struct rb_root *t, struct rb_node *old, struct rb_node *nw)
{
  struct rb_node *p = old->parent;

  if(p == NULL)
    t->root = nw;
  else if(p->left == old)
    p->left = nw;
  else
    p->right = nw;
  if(nw != NULL)
    nw->parent = p;
}

// 2.4: gỡ node khỏi cây. Nút không nằm trên cây (on_rq = 0) thì bỏ qua.
void
rb_delete(struct rb_root *tree, struct rb_node *node)
{
  struct rb_node *z = node;
  struct rb_node *child, *parent, *y;
  enum rb_color color;

  if(!z->on_rq)
    return;

  // Cập nhật leftmost trước. z là min thì không có con trái,
  // nên nút min kế tiếp là min của cây con phải, hoặc cha của z.
  if(tree->leftmost == z){
    if(z->right != NULL){
      y = z->right;
      while(y->left != NULL)
        y = y->left;
      tree->leftmost = y;
    } else {
      tree->leftmost = z->parent;
    }
  }

  if(z->left == NULL || z->right == NULL){
    // z có 0 hoặc 1 con: nối con của z vào chỗ z
    child = (z->left != NULL) ? z->left : z->right;
    parent = z->parent;
    color = z->color;
    replace_child(tree, z, child);
  } else {
    // z có 2 con: lấy nút kế tiếp y (nhỏ nhất bên phải) đặt vào chỗ z
    y = z->right;
    while(y->left != NULL)
      y = y->left;
    color = y->color;
    child = y->right;
    if(y->parent == z){
      parent = y;
    } else {
      parent = y->parent;
      replace_child(tree, y, child);       // gỡ y khỏi vị trí cũ
      y->right = z->right;
      y->right->parent = y;
    }
    replace_child(tree, z, y);             // y thế chỗ z
    y->left = z->left;
    y->left->parent = y;
    y->color = z->color;
  }

  if(color == RB_BLACK)
    delete_fixup(tree, child, parent);

  z->parent = NULL;
  z->left = NULL;
  z->right = NULL;
  z->on_rq = 0;
  tree->nr_running--;
}

// Lấy ra và gỡ luôn tiến trình có vruntime nhỏ nhất. Cây rỗng trả NULL.
struct proc *
rb_pop_min(struct rb_root *tree)
{
  struct rb_node *n = rb_min(tree);
  struct proc *p;

  if(n == NULL)
    return NULL;
  p = n->proc;
  rb_delete(tree, n);
  return p;
}

// ---- Kiểm tra bất biến (dùng cho test và debug) ----

// Trả về chiều cao đen của cây con n, hoặc -1 nếu vi phạm.
// lo/hi: cận dưới/trên theo thứ tự BST.
static int
check_node(struct rb_node *n, struct rb_node *lo, struct rb_node *hi, int *cnt)
{
  int hl, hr;

  if(n == NULL)
    return 1;                              // NIL tính là 1 nút đen
  (*cnt)++;
  if(!n->on_rq)
    return -1;                             // nút trên cây phải có on_rq = 1
  if(lo != NULL && rb_less(n, lo))
    return -1;                             // sai thứ tự BST
  if(hi != NULL && rb_less(hi, n))
    return -1;
  if(n->color != RB_RED && n->color != RB_BLACK)
    return -1;
  if(n->color == RB_RED && (is_red(n->left) || is_red(n->right)))
    return -1;                             // luật 4: đỏ-đỏ
  if(n->left != NULL && n->left->parent != n)
    return -1;                             // con trỏ cha sai
  if(n->right != NULL && n->right->parent != n)
    return -1;
  hl = check_node(n->left, lo, n, cnt);
  hr = check_node(n->right, n, hi, cnt);
  if(hl < 0 || hr < 0 || hl != hr)
    return -1;                             // luật 5: khác số nút đen
  return hl + (n->color == RB_BLACK ? 1 : 0);
}

// Trả về 1 nếu cây hợp lệ, 0 nếu vi phạm.
int
rb_verify_properties(struct rb_root *tree)
{
  int cnt = 0;
  struct rb_node *m;

  if(tree->root == NULL)
    return tree->nr_running == 0 && tree->leftmost == NULL;
  if(tree->root->color != RB_BLACK || tree->root->parent != NULL)
    return 0;                              // luật 2
  if(check_node(tree->root, NULL, NULL, &cnt) < 0)
    return 0;
  if(cnt != tree->nr_running)
    return 0;
  m = tree->root;
  while(m->left != NULL)
    m = m->left;
  return m == tree->leftmost;              // leftmost phải đúng
}
