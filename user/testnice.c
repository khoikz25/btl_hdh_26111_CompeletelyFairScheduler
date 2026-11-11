#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Bộ sinh số ngẫu nhiên đơn giản (Linear Congruential Generator)
static unsigned int next_seed = 1;

static int
rand(void)
{
  next_seed = next_seed * 1103515245 + 12345;
  return (unsigned int)(next_seed / 65536) % 32768;
}

static void
srand(unsigned int seed)
{
  next_seed = seed;
}

// Sinh một giá trị nice ngẫu nhiên trong khoảng hợp lệ [-20, 19]
static int
random_nice(void)
{
  return -20 + (rand() % 40);
}

// Hàm phụ trợ kiểm tra từng ca thử nghiệm biên (Task 5)
static void
check_setnice(int nice_val, int expected_ret, const char *desc)
{
  int ret = setnice(nice_val);
  if (ret == expected_ret) {
    printf("[PASS] %s: setnice(%d) -> tra ve %d\n", desc, nice_val, ret);
  } else {
    printf("[FAIL] %s: setnice(%d) -> ky vong %d nhung nhan ve %d\n", desc,
           nice_val, expected_ret, ret);
    exit(1);
  }
}

int
main(int argc, char *argv[])
{
  // KIỂM CHỨNG TÍNH ĐÚNG ĐẮN VÀ BẢO VỆ BIÊN CỦA SETNICE
  printf("      KIEM TRA SYSTEM CALL SETNICE     \n");

  // 1. Kiem tra bat cac gia tri khong hop le (ngoai doan [-20, 19]) -> tra ve -1
  printf("1. Kiem tra bat cac gia tri vuot nguong [-20, 19]:\n");
  check_setnice(-25, -1, "Nho hon bien duoi (-25)");
  check_setnice(-21, -1, "Sat ngoai bien duoi (-21)");
  check_setnice(20, -1, "Sat ngoai bien tren (20)");
  check_setnice(100, -1, "Lon hon bien tren (100)");

  // 2. Kiem tra thiet lap cac gia tri hop le (trong doan [-20, 19]) -> tra ve 0
  printf("\n2. Kiem tra thiet lap cac gia tri hop le:\n");
  check_setnice(-20, 0, "Tai bien duoi hop le (-20)");
  check_setnice(19, 0, "Tai bien tren hop le (19)");
  check_setnice(0, 0, "Gia tri mac dinh (0)");
  check_setnice(-10, 0, "Gia tri uu tien cao (-10)");
  check_setnice(10, 0, "Gia tri uu tien thap (10)");

  setnice(0);
  // KIỂM CHỨNG TỐC ĐỘ TĂNG VRUNTIME VỚI NICE NGẪU NHIÊN
  printf("   KIEM TRA TOC DO TANG VRUNTIME       \n");

  // Khoi tao seed ngau nhien tu thoi gian he thong
  srand(uptime());

  int nice1 = random_nice();
  int nice2 = random_nice();
  while (nice2 == nice1) {
    nice2 = random_nice(); // Đảm bảo 2 tiến trình có mức nice khác nhau
  }

  printf("Sinh ngau nhien muc nice:\n");
  printf("  -> Tien trinh con 1: nice = %d\n", nice1);
  printf("  -> Tien trinh con 2: nice = %d\n", nice2);
  printf("Dang chay tinh toan CPU de do luong...\n\n");

  // Tiến trình con 1
  int pid1 = fork();
  if (pid1 == 0) {
    setnice(nice1);
    volatile int x = 0;
    for (int i = 0; i < 200000000; i++)
      x += i;
    (void)x;
    exit(0);
  }

  // Tiến trình con 2
  int pid2 = fork();
  if (pid2 == 0) {
    setnice(nice2);
    volatile int x = 0;
    for (int i = 0; i < 200000000; i++)
      x += i;
    (void)x;
    exit(0);
  }

  // Chờ cả 2 tiến trình con hoàn tất
  wait(0);
  wait(0);

  exit(0);
}