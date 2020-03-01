#include "uccn/utilities/upoll.h"

#include <assert.h>

#include "uccn/common/time.h"

int upoll(struct upoll_s * upolls, size_t npolls, struct timespec * next_poll_time) {
  size_t i;
  int ret = 0;
  struct timespec current_time;

  if (npolls > 0) {
    assert(upolls != NULL);
    assert(next_poll_time != NULL);

    if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
      return ret;
    }
    TIMESPEC_INF_INIT(next_poll_time);
    for (i = 0; i < npolls; ++i) {
      if (timespec_cmp(&current_time, &upolls[i].next_poll_time) >= 0) {
        upolls[i].poll(upolls[i].arg);

        if (TIMESPEC_ISZERO(&upolls[i].next_poll_time)) {
          upolls[i].next_poll_time = current_time;
        }
        timespec_add(&upolls[i].next_poll_time, &upolls[i].polling_period);
      }
      if (timespec_cmp(next_poll_time, &upolls[i].next_poll_time) > 0) {
        *next_poll_time = upolls[i].next_poll_time;
      }
    }
  }

  return ret;
}
