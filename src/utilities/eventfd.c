#include "uccn/utilities/eventfd.h"

#include <fcntl.h>
#include <unistd.h>

int eventfd_init(struct eventfd_s * ev) {
  int ret;
  if ((ret = pipe(ev->fd)) < 0) {
    return ret;
  }
  if ((ret = fcntl(ev->fd[1], F_GETFL, 0)) < 0) {
    return ret;
  }
  return fcntl(ev->fd[1], F_SETFL, ret | O_NONBLOCK);
}

int eventfd_set(struct eventfd_s * ev)
{
  return write(ev->fd[1], "\n", 1);
}

int eventfd_clear(struct eventfd_s * ev)
{
  char tmp;
  return read(ev->fd[0], &tmp, 1);
}

int eventfd_fini(struct eventfd_s * ev)
{
  return close(ev->fd[0]) | close(ev->fd[1]);
}
