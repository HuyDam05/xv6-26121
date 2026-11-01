#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"
#include "crash.h"

// Crash-injection: mo phong mat dien bang cach thoat QEMU ngay lap tuc.
// Mac dinh TAT (CRASH_NONE) nen kernel binh thuong khong bi anh huong.
// crash_set(point, count): crash o lan thu `count` ma crash_check(point)
// duoc goi. Voi CRASH_DISK_WRITE, do la lan ghi block thu `count`.

#ifndef CRASH_DEFAULT_POINT
#define CRASH_DEFAULT_POINT CRASH_NONE
#endif
#ifndef CRASH_DEFAULT_COUNT
#define CRASH_DEFAULT_COUNT 0
#endif

static volatile int crash_point = CRASH_DEFAULT_POINT;
static volatile int crash_left = CRASH_DEFAULT_COUNT;

static void
crash_now(int point)
{
  printk("CRASH: point=%d\n", point);
  // thiet bi test cua QEMU virt: ghi 0x5555 la thoat QEMU (ma tra ve 0)
  *(volatile uint32 *)QEMU_TEST = 0x5555;
  for (;;)
    ;
}

void
crash_init(void)
{
  crash_left = CRASH_DEFAULT_COUNT;
  crash_point = CRASH_DEFAULT_POINT;
}

void
crash_set(int point, int count)
{
  if (point == CRASH_NONE || count <= 0) {
    crash_point = CRASH_NONE;
    return;
  }
  crash_left = count;
  crash_point = point;
}

void
crash_check(int point)
{
  if (crash_point == CRASH_NONE || point != crash_point)
    return;
  if (__sync_sub_and_fetch(&crash_left, 1) == 0)
    crash_now(point);
}
