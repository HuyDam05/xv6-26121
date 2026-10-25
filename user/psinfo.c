#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int psinfo(void);

int
main(void)
{
  psinfo();
  exit(0);
}
