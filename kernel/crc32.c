#include "types.h"
#include "crc32.h"

// CRC32 da thuc 0xEDB88320 (giong zlib/ethernet), tinh tung bit.
// Goi lan dau voi crc = 0; muon tinh noi tiep nhieu doan thi truyen
// ket qua lan truoc vao lan sau: crc = crc32(crc, buf, n).
uint32
crc32(uint32 crc, const void *buf, uint64 size)
{
  const uchar *p = buf;
  int k;

  crc = ~crc;
  while (size--) {
    crc ^= *p++;
    for (k = 0; k < 8; k++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
  }
  return ~crc;
}
