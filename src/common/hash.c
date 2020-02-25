#include "uccn/common/hash.h"

uint32_t djb2(const char * path) {
  uint32_t hash = 5381;

  char c;
  while ((c = *path++) != '\0') {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}
