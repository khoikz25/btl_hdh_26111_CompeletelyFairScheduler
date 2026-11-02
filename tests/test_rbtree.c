// Test cây đỏ-đen trên Ubuntu (ngoài kernel xv6).
// Build và chạy: xem lệnh gcc trong hướng dẫn.

#include <stdio.h>
#include <stdlib.h>
#include "rbtree.h"

#define MAXN 20000

// ---- Đồ giả để compile ngoài kernel ----
struct proc { int idx; };                       // header chỉ khai báo trước struct proc

void
initlock(struct spinlock *lk, char *name)       // bản giả của hàm trong kernel/spinlock.c
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// ---- Khung test ----
static int fails = 0;
static int checks = 0;

#define EXPECT(cond, msg) do { \
  checks++; \
  if(!(cond)){ fails++; printf("  FAIL: %s (dòng %d)\n", msg, __LINE__); } \
} while(0)

static struct rb_node pool[MAXN];
static struct proc procs[MAXN];
static int live[MAXN];

// Tìm nút nhỏ nhất bằng cách quét mảng (để so với rb_min).
static struct rb_node *
brute_min(int n)
{
  struct rb_node *best = NULL;
  for(int i = 0; i < n; i++){
    if(!live[i]) continue;
    if(best == NULL || rb_compare(&pool[i], best) < 0)
      best = &pool[i];
  }
  return best;
}

static void
verify(struct rb_root *t, int n, int expect_count, const char *where)
{
  checks++;
  if(!rb_verify_properties(t)){ fails++; printf("  FAIL: rb_verify_properties sai tại %s\n", where); }
  checks++;
  if(t->nr_running != expect_count){ fails++; printf("  FAIL: nr_running=%d, mong đợi %d tại %s\n", t->nr_running, expect_count, where); }
  checks++;
  if(rb_min(t) != brute_min(n)){ fails++; printf("  FAIL: rb_min sai tại %s\n", where); }
}

static int
height(struct rb_node *n)
{
  if(n == NULL) return 0;
  int l = height(n->left), r = height(n->right);
  return 1 + (l > r ? l : r);
}

static void
setup(int n, unsigned long (*keyfn)(int))
{
  for(int i = 0; i < n; i++){
    procs[i].idx = i;
    rb_node_init(&pool[i], &procs[i]);
    pool[i].vruntime = keyfn(i);
    pool[i].pid = i;                 // pid duy nhất
    live[i] = 0;
  }
}

static unsigned long k_asc(int i)  { return i; }
static unsigned long k_desc(int i) { return 100000 - i; }
static unsigned long k_rand(int i) { (void)i; return rand() % 100000; }
static unsigned long k_same(int i) { (void)i; return 42; }      // vruntime trùng hết
static unsigned long k_few(int i)  { (void)i; return rand() % 5; }

static void
report(const char *name, int f0)
{
  printf("[%s] %s\n", fails == f0 ? "PASS" : "FAIL", name);
}

static void
test_empty(void)
{
  int f0 = fails;
  struct rb_root t;
  rb_init(&t);
  EXPECT(rb_min(&t) == NULL, "cây rỗng: rb_min phải NULL");
  EXPECT(rb_pop_min(&t) == NULL, "cây rỗng: rb_pop_min phải NULL");
  EXPECT(t.nr_running == 0, "cây rỗng: nr_running=0");
  EXPECT(rb_verify_properties(&t), "cây rỗng: hợp lệ");
  report("cây rỗng", f0);
}

static void
test_single(void)
{
  int f0 = fails;
  struct rb_root t;
  struct proc p = {7};
  struct rb_node a;
  rb_init(&t);
  rb_node_init(&a, &p);
  a.vruntime = 5; a.pid = 1;
  rb_insert(&t, &a);
  EXPECT(t.root == &a, "1 nút: là gốc");
  EXPECT(a.color == RB_BLACK, "1 nút: màu đen");
  EXPECT(a.on_rq == 1, "1 nút: on_rq = 1");
  EXPECT(rb_min(&t) == &a, "1 nút: rb_min");
  EXPECT(rb_verify_properties(&t), "1 nút: hợp lệ");
  EXPECT(rb_pop_min(&t) == &p, "pop trả đúng proc");
  EXPECT(a.on_rq == 0, "sau pop: on_rq = 0");
  EXPECT(t.nr_running == 0 && rb_min(&t) == NULL, "sau pop: rỗng");
  report("1 nút: chèn rồi pop", f0);
}

// Chèn/xóa hai lần liên tiếp không được làm hỏng cây.
static void
test_double_ops(void)
{
  int f0 = fails;
  struct rb_root t;
  struct proc p = {1};
  struct rb_node a;
  rb_init(&t);
  rb_node_init(&a, &p);
  a.vruntime = 10; a.pid = 1;
  rb_insert(&t, &a);
  rb_insert(&t, &a);                       // chèn lần 2: phải bị bỏ qua
  EXPECT(t.nr_running == 1, "chèn 2 lần: nr_running vẫn 1");
  EXPECT(rb_verify_properties(&t), "chèn 2 lần: hợp lệ");
  rb_delete(&t, &a);
  rb_delete(&t, &a);                       // xóa lần 2: phải bị bỏ qua
  EXPECT(t.nr_running == 0, "xóa 2 lần: nr_running = 0");
  EXPECT(rb_verify_properties(&t), "xóa 2 lần: hợp lệ");
  report("chèn/xóa lặp không hỏng cây", f0);
}

// Chèn n nút theo keyfn, kiểm tra sau MỖI lần chèn; rồi xóa theo del_mode.
static void
test_insert_then_delete(const char *name, int n, unsigned long (*keyfn)(int), int del_mode)
{
  int f0 = fails;
  struct rb_root t;
  rb_init(&t);
  setup(n, keyfn);
  int cnt = 0;
  for(int i = 0; i < n; i++){
    rb_insert(&t, &pool[i]);
    live[i] = 1; cnt++;
    verify(&t, n, cnt, "sau chèn");
  }
  int h = height(t.root);
  int bound = 0; for(int x = n + 1; x > 1; x >>= 1) bound++;   // ~log2(n+1)
  EXPECT(h <= 2 * (bound + 1), "chiều cao <= 2*log2(n+1)");

  for(int k = 0; k < n; k++){
    int i;
    if(del_mode == 0) i = k;                     // xóa theo thứ tự chèn
    else if(del_mode == 1) i = n - 1 - k;        // ngược lại
    else {                                       // ngẫu nhiên
      do { i = rand() % n; } while(!live[i]);
    }
    rb_delete(&t, &pool[i]);
    live[i] = 0; cnt--;
    verify(&t, n, cnt, "sau xóa");
  }
  EXPECT(t.nr_running == 0 && rb_min(&t) == NULL, "xóa hết => rỗng");
  report(name, f0);
}

// Liên tục pop min: vruntime phải ra theo thứ tự tăng dần.
static void
test_pop_min_sorted(void)
{
  int f0 = fails;
  struct rb_root t;
  int n = 1000;
  rb_init(&t);
  setup(n, k_rand);
  for(int i = 0; i < n; i++){ rb_insert(&t, &pool[i]); live[i] = 1; }
  struct rb_node *prev = NULL;
  struct proc *p;
  int popped = 0;
  while((p = rb_pop_min(&t)) != NULL){
    struct rb_node *cur = &pool[p->idx];
    if(prev != NULL)
      EXPECT(rb_compare(prev, cur) < 0, "pop min: thứ tự tăng dần");
    prev = cur;
    EXPECT(rb_verify_properties(&t), "pop min: hợp lệ");
    popped++;
  }
  EXPECT(popped == n, "pop min: đủ n nút");
  report("pop min liên tục => tăng dần", f0);
}

// Stress: chèn/xóa ngẫu nhiên xen kẽ, seed cố định.
static void
test_stress(int ops, int n, unsigned long (*keyfn)(int), const char *name)
{
  int f0 = fails;
  struct rb_root t;
  rb_init(&t);
  setup(n, keyfn);
  int cnt = 0;
  for(int o = 0; o < ops; o++){
    int i = rand() % n;
    if(live[i]){
      rb_delete(&t, &pool[i]); live[i] = 0; cnt--;
    } else {
      pool[i].vruntime = keyfn(i);               // vruntime mới mỗi lần chèn lại
      rb_insert(&t, &pool[i]); live[i] = 1; cnt++;
    }
    verify(&t, n, cnt, "stress");
  }
  report(name, f0);
}

int
main(void)
{
  srand(12345);                                // seed cố định => lặp lại được
  test_empty();
  test_single();
  test_double_ops();
  test_insert_then_delete("chèn tăng dần 100, xóa thuận", 100, k_asc, 0);
  test_insert_then_delete("chèn giảm dần 100, xóa ngược", 100, k_desc, 1);
  test_insert_then_delete("chèn ngẫu nhiên 1000, xóa ngẫu nhiên", 1000, k_rand, 2);
  test_insert_then_delete("vruntime trùng hết, chỉ khác pid", 200, k_same, 2);
  test_insert_then_delete("vruntime trùng nhiều, 500 nút", 500, k_few, 2);
  test_pop_min_sorted();
  test_stress(1000, 64, k_rand, "stress 1000 thao tác (64 nút ~ NPROC)");
  test_stress(10000, 500, k_rand, "stress 10000 thao tác");
  test_stress(10000, 300, k_few, "stress 10000 thao tác, vruntime trùng nhiều");

  printf("\n%d kiểm tra, %d lỗi\n", checks, fails);
  printf("%s\n", fails == 0 ? "ALL PASS" : "SOME FAIL");
  return fails != 0;
}
