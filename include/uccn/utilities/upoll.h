#ifndef UCCN_UTILITIES_UPOLL_H_
#define UCCN_UTILITIES_UPOLL_H_

#include <stddef.h>
#include <time.h>

typedef void (*upoll_fn)(void * arg);

struct upoll_s {
  void * arg;
  upoll_fn poll;

  struct timespec polling_period;
  struct timespec next_poll_time;
};

#if defined(__cplusplus)
extern "C"
{
#endif

int upoll(struct upoll_s * polls, size_t npolls, struct timespec * next_poll_time);

#if defined(__cplusplus)
}
#endif

#endif // UCCN_UTILITIES_UPOLL_H_
