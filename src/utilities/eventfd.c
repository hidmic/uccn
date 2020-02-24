#include "uccn/utilities/eventfd.h"

#include <fcntl.h>
#include <unistd.h>

int eventfd_init(struct eventfd_s * ev) {
  int ret;
  if ((ret = pipe(ev->fd)) == 0) {
    ret = fcntl(ev->fd[0], F_SETFD, O_NONBLOCK);
  }
  return ret;
}

int eventfd_set(struct eventfd_s * ev)
{
  return write(ev->fd[1], "\0", 1);
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
