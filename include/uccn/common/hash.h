#ifndef UCCN_COMMON_HASH_
#define UCCN_COMMON_HASH_

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

uint32_t djb2(const char * path);

#if defined(__cplusplus)
}
#endif

#endif  // UCCN_COMMON_HASH_
