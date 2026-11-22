#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define TESTFILE "ireclaim.tmp"

static void
fail(char *msg)
{
  printf("FAIL %s\n", msg);
  exit(1);
}

static void
show_usage(char *label, struct fsusage *u)
{
  printf("%s: free_blocks=%d free_inodes=%d\n", label, u->free_blocks,
         u->free_inodes);
}

int
main(int argc, char **argv)
{
  struct fsusage before, during, after;
  char data[BSIZE];
  int fd;

  if (argc < 2) {
    printf("usage: ireclaimtest prepare | verify BLOCKS INODES\n");
    exit(1);
  }

  if (strcmp(argv[1], "prepare") == 0) {
    // Warm up a directory entry before recording baseline.
    unlink(TESTFILE);

    fd = open(TESTFILE, O_CREATE | O_RDWR);
    if (fd < 0)
      fail("warmup create");

    close(fd);

    if (unlink(TESTFILE) < 0)
      fail("warmup unlink");

    if (fsusage(&before) < 0)
      fail("baseline fsusage");

    show_usage("BASELINE", &before);

    fd = open(TESTFILE, O_CREATE | O_RDWR);
    if (fd < 0)
      fail("create orphan test file");

    memset(data, 'A', sizeof(data));

    if (write(fd, data, sizeof(data)) != sizeof(data))
      fail("write block 1");

    if (write(fd, data, sizeof(data)) != sizeof(data))
      fail("write block 2");

    if (unlink(TESTFILE) < 0)
      fail("unlink open file");

    if (fsusage(&during) < 0)
      fail("fsusage after unlink");

    show_usage("DURING", &during);

    if (during.free_inodes + 1 != before.free_inodes)
      fail("orphan inode not allocated");

    if (during.free_blocks + 2 > before.free_blocks)
      fail("orphan data blocks not allocated");

    printf("IRECLAIM_READY baseline_blocks=%d baseline_inodes=%d\n",
           before.free_blocks, before.free_inodes);

    // Keep fd open until QEMU is forcibly terminated.
    for (;;)
      pause(1000);
  }

  if (strcmp(argv[1], "verify") == 0) {
    if (argc != 4)
      fail("verify requires baseline blocks and inodes");

    int expected_blocks = atoi(argv[2]);
    int expected_inodes = atoi(argv[3]);

    if (fsusage(&after) < 0)
      fail("fsusage after reboot");

    show_usage("AFTER", &after);

    if (after.free_blocks != expected_blocks)
      fail("block leak after recovery");

    if (after.free_inodes != expected_inodes)
      fail("inode leak after recovery");

    fd = open(TESTFILE, O_RDONLY);
    if (fd >= 0) {
      close(fd);
      fail("unlinked file is still reachable");
    }

    printf("PASS ireclaim: inode and blocks restored\n");
    exit(0);
  }

  fail("unknown mode");
  exit(1);
}
