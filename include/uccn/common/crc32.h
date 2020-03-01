#ifndef UCCN_COMMON_CRC32_
#define UCCN_COMMON_CRC32_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

uint32_t crc32(const uint8_t * buffer, size_t size);

#if defined(__cplusplus)
}
#endif

#endif  // UCCN_COMMON_CRC32_
