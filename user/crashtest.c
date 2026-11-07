#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// crashtest: dat diem crash trong kernel roi chay tai ghi file.
//   crashtest                    in "ready" (giu hanh vi cu)
//   crashtest <point> <count>    dat diem crash, chay tai co san
//   crashtest <point> <count> cmd [args...]
//                                dat diem crash, roi exec cmd lam tai
// point: 0 = tat, 1..5 xem kernel/crash.h. count: crash o lan thu count.
// Diem crash la bien tinh cua kernel nen van con hieu luc sau exec/fork.

enum { NFILES = 8, NBLOCKS = 4, ROUNDS = 20 };

static char buf[1024];

static void
workload(void)
{
  char name[8];

  memset(buf, 'x', sizeof(buf));
  for (int r = 0; r < ROUNDS; r++) {
    for (int i = 0; i < NFILES; i++) {
      name[0] = 'c';
      name[1] = 't';
      name[2] = '0' + i;
      name[3] = 0;
      int fd = open(name, O_CREATE | O_RDWR);
      if (fd < 0) {
        printf("crashtest: create %s failed\n", name);
        exit(1);
      }
      for (int b = 0; b < NBLOCKS; b++) {
        if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
          printf("crashtest: write failed\n");
          exit(1);
        }
      }
      close(fd);
    }
    for (int i = 0; i < NFILES; i++) {
      name[0] = 'c';
      name[1] = 't';
      name[2] = '0' + i;
      name[3] = 0;
      unlink(name);
    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("crashtest: ready\n");
    exit(0);
  }
  if (argc < 3) {
    printf("usage: crashtest [point count [cmd args...]]\n");
    exit(1);
  }
  if (crash_ctl(atoi(argv[1]), atoi(argv[2])) < 0) {
    printf("crashtest: crash_ctl(%s, %s) failed: invalid argument\n", argv[1],
           argv[2]);
    exit(1);
  }
  if (argc > 3) {
    exec(argv[3], &argv[3]);
    printf("crashtest: exec %s failed\n", argv[3]);
    exit(1);
  }
  workload();
  printf("crashtest: workload done, no crash\n");
  exit(0);
}
