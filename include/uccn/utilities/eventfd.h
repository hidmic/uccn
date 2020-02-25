#ifndef UCCN_UTILITIES_EVENTFD_H_
#define UCCN_UTILITIES_EVENTFD_H_

struct eventfd_s
{
  int fd[2];
};

#if defined(__cplusplus)
extern "C"
{
#endif

int eventfd_init(struct eventfd_s * ev);

int eventfd_set(struct eventfd_s * ev);

static inline int eventfd_fileno(struct eventfd_s * ev) {
  return ev->fd[0];
}

int eventfd_clear(struct eventfd_s * ev);

int eventfd_fini(struct eventfd_s * ev);

#if defined(__cplusplus)
}
#endif

#endif  // UCCN_UTILITIES_EVENTFD_H_
