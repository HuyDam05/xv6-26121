
#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define ROUNDS 20
#define FILE_A "n15file"
#define FILE_B "n15link"
#define DIR_A  "n15dir"
#define CHILD  "n15dir/child"

static char data[BSIZE];

static void
fail(char *message)
{
  printf("N15 FAIL: %s\n", message);
  exit(1);
}

static void
show_usage(char *label, struct fsusage *u)
{
  printf("N15 %s: free_blocks=%d free_inodes=%d\n", label, u->free_blocks,
         u->free_inodes);
}

static void
create_workload(void)
{
  int fd = open(FILE_A, O_CREATE | O_RDWR);
  if (fd < 0)
    fail("create file");

  for (int i = 0; i < 3; i++) {
    if (write(fd, data, BSIZE) != BSIZE)
      fail("write file");
  }
  close(fd);

  if (link(FILE_A, FILE_B) < 0)
    fail("create hard link");

  if (mkdir(DIR_A) < 0)
    fail("create directory");

  fd = open(CHILD, O_CREATE | O_RDWR);
  if (fd < 0)
    fail("create child");

  if (write(fd, data, BSIZE) != BSIZE)
    fail("write child");

  close(fd);
}

static void
remove_workload(void)
{
  if (unlink(FILE_B) < 0)
    fail("remove hard link");

  if (unlink(FILE_A) < 0)
    fail("remove file");

  if (unlink(CHILD) < 0)
    fail("remove child");

  if (unlink(DIR_A) < 0)
    fail("remove directory");
}

int
main(void)
{
  struct fsusage before, during, after;

  memset(data, 'A', sizeof(data));

  // Warm-up: reserve any directory slots needed
  // before taking the baseline.
  create_workload();
  remove_workload();

  if (fsusage(&before) < 0)
    fail("read baseline");

  show_usage("BEFORE", &before);

  for (int i = 0; i < ROUNDS; i++) {
    create_workload();

    if (fsusage(&during) < 0)
      fail("read active usage");

    if (during.free_blocks >= before.free_blocks ||
        during.free_inodes >= before.free_inodes)
      fail("allocation not observed");

    remove_workload();

    if (fsusage(&after) < 0)
      fail("read usage after cleanup");

    if (after.free_blocks != before.free_blocks ||
        after.free_inodes != before.free_inodes) {
      show_usage("BEFORE", &before);
      show_usage("AFTER", &after);
      fail("resource mismatch");
    }

    printf("N15 ROUND %d PASS\n", i + 1);
  }

  show_usage("AFTER", &after);

  printf("N15 RESULT: PASS\n");
  printf("rounds=%d delta_blocks=0 delta_inodes=0\n", ROUNDS);

  exit(0);
}
