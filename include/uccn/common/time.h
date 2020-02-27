#ifndef UCCN_COMMON_TIME_H_
#define UCCN_COMMON_TIME_H_

#include <time.h>

#define TIMESPEC_ZERO {0, 0}
#define TIMESPEC_INF {-1, 0}

#define TIMESPEC_ZERO_INIT(tp) (tp)->tv_sec = 0, (tp)->tv_nsec = 0
#define TIMESPEC_INF_INIT(tp) (tp)->tv_sec = -1, (tp)->tv_nsec = -1
#define TIMESPEC_SECONDS_INIT(tp, sec) (tp)->tv_sec = sec, (tp)->tv_nsec = 0

#define TIMESPEC_ISFINITE(tp) ((tp)->tv_sec >= 0)

#if defined(__cplusplus)
extern "C"
{
#endif

static inline int timespec_cmp(const struct timespec * a, const struct timespec * b) {
  if (a->tv_sec < 0 && b->tv_sec >= 0) return 1;
  if (a->tv_sec >= 0 && b->tv_sec < 0) return -1;
  if (a->tv_sec < 0 && b->tv_sec < 0) return 0;

  if (a->tv_sec > b->tv_sec) return 1;
  if (a->tv_sec < b->tv_sec) return -1;
  if (a->tv_nsec > b->tv_nsec) return 1;
  if (a->tv_nsec < b->tv_nsec) return -1;
  return 0;
}

static inline void timespec_add(struct timespec * a, const struct timespec * b) {
  if (a->tv_sec < 0 || b->tv_sec < 0) {
    TIMESPEC_INF_INIT(a);
    return;
  }
  a->tv_sec += b->tv_sec;
  a->tv_nsec += b->tv_nsec;
  a->tv_sec += a->tv_nsec / 1000000000L;
  a->tv_nsec = a->tv_nsec % 1000000000L;
}

static inline void timespec_diff(struct timespec * a, const struct timespec * b) {
  if (a->tv_sec < 0 || b->tv_sec < 0) {
    TIMESPEC_INF_INIT(a);
    return;
  }
  if (timespec_cmp(a, b) > 0) {
    a->tv_sec = a->tv_sec - b->tv_sec;
    a->tv_nsec = a->tv_nsec - b->tv_nsec;
  } else {
    a->tv_sec = b->tv_sec - a->tv_sec;
    a->tv_nsec = b->tv_nsec - a->tv_nsec;
  }
  if (a->tv_nsec < 0) {
    a->tv_nsec += 1000000000L;
    a->tv_sec -= 1;
  }
}

#if defined(__cplusplus)
}
#endif

#endif // UCCN_COMMON_TIME_H_
