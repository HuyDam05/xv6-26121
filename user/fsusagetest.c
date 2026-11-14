
#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define TESTFILE "fsusage.tmp"

static void
check(int ok, char *name)
{
  if (!ok) {
    printf("FAIL %s\n", name);
    exit(1);
  }
  printf("PASS %s\n", name);
}

int
main(void)
{
  struct fsusage before, created, written, after;
  char data[BSIZE];
  int fd;

  check(fsusage(&before) == 0 &&
        before.total_blocks > 0 &&
        before.free_blocks <= before.total_blocks &&
        before.total_inodes > 0 &&
        before.free_inodes <= before.total_inodes,
        "F01 valid statistics");

  check(fsusage((struct fsusage *)0) == -1,
        "F02 null pointer");

  check(fsusage((struct fsusage *)
        0xffffffffffffffffULL) == -1,
        "F03 invalid pointer");

  // Warm up the directory slot before baseline.
  unlink(TESTFILE);
  fd = open(TESTFILE, O_CREATE | O_RDWR);
  check(fd >= 0, "F04 warmup create");
  close(fd);
  check(unlink(TESTFILE) == 0, "F05 warmup unlink");

  check(fsusage(&before) == 0,
        "F06 baseline");

  fd = open(TESTFILE, O_CREATE | O_RDWR);
  check(fd >= 0, "F07 create");

  check(fsusage(&created) == 0 &&
        created.free_inodes + 1 == before.free_inodes,
        "F08 inode allocation");

  memset(data, 'A', sizeof(data));
  check(write(fd, data, sizeof(data)) == sizeof(data) &&
        write(fd, data, sizeof(data)) == sizeof(data),
        "F09 write two blocks");

  close(fd);

  check(fsusage(&written) == 0 &&
        written.free_blocks + 2 <= created.free_blocks,
        "F10 block allocation");

  check(unlink(TESTFILE) == 0,
        "F11 unlink");

  check(fsusage(&after) == 0 &&
        after.free_inodes == before.free_inodes &&
        after.free_blocks == before.free_blocks,
        "F12 resources restored");

  printf("fsusagetest: ALL TESTS PASSED\n");
  exit(0);
}
