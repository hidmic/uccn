#ifndef UCCN_COMMON_BUFFER_H_
#define UCCN_COMMON_BUFFER_H_

#include <stddef.h>

struct buffer_head_s
{
  void * data;
  size_t length;
  size_t size;
};

#define stack_buffer_init(pbuffer, member)                               \
  do {                                                                   \
    ((struct buffer_head_s *)pbuffer)->data = (pbuffer)->member;         \
    ((struct buffer_head_s *)pbuffer)->size = sizeof((pbuffer)->member); \
    ((struct buffer_head_s *)pbuffer)->length = 0;                       \
  } while(0)

#endif // UCCN_COMMON_BUFFER_H_
