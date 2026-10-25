#ifndef XV6_CRC32_H
#define XV6_CRC32_H

#include "types.h"

uint32 crc32(uint32 crc, const void *buf, uint64 size);

#endif
