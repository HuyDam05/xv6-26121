#include <stdio.h>
#include "../kernel/crc32.h"

int
main(void)
{
  const char *s = "123456789";
  uint32 whole = crc32(0, s, 9);
  uint32 chained = crc32(crc32(0, s, 4), s + 4, 5);
  uint32 empty = crc32(0, s, 0);

  printf("crc32(\"123456789\")  = 0x%08X (can: 0xCBF43926)\n", whole);
  printf("noi tiep 4 + 5 byte = 0x%08X (can: 0xCBF43926)\n", chained);
  printf("chuoi rong          = 0x%08X (can: 0x00000000)\n", empty);

  if (whole == 0xCBF43926 && chained == 0xCBF43926 && empty == 0) {
    printf("PASS\n");
    return 0;
  }
  printf("FAIL\n");
  return 1;
}
