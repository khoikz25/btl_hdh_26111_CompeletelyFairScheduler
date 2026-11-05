// schedtest: đo mức chia sẻ CPU giữa N tiến trình.
// Dùng: schedtest [N] [ticks]     (mặc định N=4, ticks=100)
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXPROC 50

struct result {
  int idx;     // tiến trình thứ mấy (0..N-1)
  int pid;
  int count;   // số vòng lặp đã chạy xong trong khoảng đo
};

// Chạy vòng lặp tính toán đến khi uptime() >= end, trả về số vòng.
static int
spin(int end)
{
  volatile int x = 0;
  int count = 0;

  while(uptime() < end){
    for(int i = 0; i < 1000; i++)
      x += i;
    count++;
  }
  return count;
}

int
main(int argc, char *argv[])
{
  int n = 4, duration = 100;
  int fds[2];
  struct result r[MAXPROC];
  int total = 0, min = -1, max = 0;

  if(argc > 1)
    n = atoi(argv[1]);
  if(argc > 2)
    duration = atoi(argv[2]);
  if(n < 1 || n > MAXPROC || duration < 1){
    fprintf(2, "usage: schedtest [N 1..%d] [ticks]\n", MAXPROC);
    exit(1);
  }

  if(pipe(fds) < 0){
    fprintf(2, "schedtest: pipe failed\n");
    exit(1);
  }

  // Mọi tiến trình cùng dừng tại một thời điểm => cùng khoảng đo.
  int end = uptime() + duration;

  for(int i = 0; i < n; i++){
    int pid = fork();
    if(pid < 0){
      fprintf(2, "schedtest: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      struct result me;
      close(fds[0]);
      me.idx = i;
      me.pid = getpid();
      me.count = spin(end);
      write(fds[1], &me, sizeof(me));
      close(fds[1]);
      exit(0);
    }
  }

  close(fds[1]);
  for(int i = 0; i < n; i++){
    struct result tmp;
    if(read(fds[0], &tmp, sizeof(tmp)) != sizeof(tmp)){
      fprintf(2, "schedtest: read failed\n");
      exit(1);
    }
    r[tmp.idx] = tmp;
  }
  close(fds[0]);
  for(int i = 0; i < n; i++)
    wait(0);

  for(int i = 0; i < n; i++){
    total += r[i].count;
    if(min < 0 || r[i].count < min)
      min = r[i].count;
    if(r[i].count > max)
      max = r[i].count;
  }

  printf("schedtest: N=%d ticks=%d\n", n, duration);
  for(int i = 0; i < n; i++){
    int pct10 = total > 0 ? (int)((long)r[i].count * 1000 / total) : 0;
    printf("proc %d pid %d count %d share %d.%d%%\n",
           r[i].idx, r[i].pid, r[i].count, pct10 / 10, pct10 % 10);
  }
  printf("total %d min %d max %d\n", total, min, max);
  exit(0);
}

