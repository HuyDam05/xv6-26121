#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

#define NFILES  8
#define NBLOCKS 2

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

static void
filename(char *name, int i)
{
  strcpy(name, "ireclaim0.tmp");
  name[8] = '0' + i;
}

int
main(int argc, char **argv)
{
  struct fsusage before, during, after;
  char data[BSIZE];
  char name[16];
  int fds[NFILES];
  int fd;

  if (argc < 2) {
    printf("usage: ireclaimtest prepare | verify BLOCKS INODES\n");
    exit(1);
  }

  if (strcmp(argv[1], "prepare") == 0) {
    if (NFILES + 3 > NOFILE)
      fail("too many file descriptors");

    // Warm up directory slots before measuring baseline.
    for (int i = 0; i < NFILES; i++) {
      filename(name, i);
      unlink(name);

      fds[i] = open(name, O_CREATE | O_RDWR);
      if (fds[i] < 0)
        fail("warmup create");
    }

    for (int i = 0; i < NFILES; i++) {
      filename(name, i);
      close(fds[i]);

      if (unlink(name) < 0)
        fail("warmup unlink");
    }

    if (fsusage(&before) < 0)
      fail("baseline fsusage");

    show_usage("BASELINE", &before);

    // Allocate multiple files and keep every descriptor open.
    for (int i = 0; i < NFILES; i++) {
      filename(name, i);

      fds[i] = open(name, O_CREATE | O_RDWR);
      if (fds[i] < 0)
        fail("create orphan file");

      memset(data, 'A' + i, sizeof(data));

      for (int j = 0; j < NBLOCKS; j++) {
        if (write(fds[i], data, sizeof(data)) != sizeof(data))
          fail("write orphan data");
      }
    }

    // Remove directory links without closing descriptors.
    for (int i = 0; i < NFILES; i++) {
      filename(name, i);

      if (unlink(name) < 0)
        fail("unlink open file");

      fd = open(name, O_RDONLY);
      if (fd >= 0) {
        close(fd);
        fail("orphan is still reachable");
      }

      printf("ORPHAN_READY file=%s fd=%d\n", name, fds[i]);
    }

    if (fsusage(&during) < 0)
      fail("fsusage after unlink");

    show_usage("DURING", &during);

    int used_blocks = before.free_blocks - during.free_blocks;

    int used_inodes = before.free_inodes - during.free_inodes;

    printf("ALLOCATED blocks=%d inodes=%d\n", used_blocks, used_inodes);

    if (used_inodes != NFILES)
      fail("unexpected allocated inode count");

    if (used_blocks != NFILES * NBLOCKS)
      fail("unexpected allocated block count");

    printf("IRECLAIM_READY baseline_blocks=%d baseline_inodes=%d\n",
           before.free_blocks, before.free_inodes);

    // Keep all descriptors open until QEMU is killed.
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

    for (int i = 0; i < NFILES; i++) {
      filename(name, i);

      fd = open(name, O_RDONLY);
      if (fd >= 0) {
        close(fd);
        fail("unlinked file still reachable");
      }

      printf("VERIFIED_UNREACHABLE file=%s\n", name);
    }

    printf("PASS ireclaim: inode and blocks restored\n");
    printf("PASS stress: all %d orphan files reclaimed\n", NFILES);

    exit(0);
  }

  fail("unknown mode");
  exit(1);
}
