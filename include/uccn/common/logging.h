#ifndef UCCN_COMMON_LOGGING_H_
#define UCCN_COMMON_LOGGING_H_

#include "uccn/config.h"

#include <string.h>
#include <syslog.h>

#define RUNTIME_ERR(msg, ...)                   \
  msg " in %s @ " __FILE__ ":%d",               \
    ##__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__

#define SYSTEM_ERR_FROM(line, msg, ...)                             \
  "%s (%d). " msg " in %s @ " __FILE__ ":%d",                       \
  strerror(errno), errno, ##__VA_ARGS__, __PRETTY_FUNCTION__, line

#define BACKTRACE_FROM(line) \
  "(backtrace) %s @ " __FILE__ ":%d", __PRETTY_FUNCTION__, line

#ifdef CONFIG_UCCN_LOGGING

#define uccnerr(msg, ...) syslog(LOG_ERR, "[uccn] " msg, ##__VA_ARGS__)
#define uccnwarn(msg, ...) syslog(LOG_WARNING, "[uccn] " msg, ##__VA_ARGS__)
#define uccninfo(msg, ...) syslog(LOG_INFO, "[uccn] " msg, ##__VA_ARGS__)
#define uccndbg(msg, ...) syslog(LOG_DEBUG, "[uccn] " msg, ##__VA_ARGS__)

#else

#define uccnerr(msg, ...)
#define uccnwarn(msg, ...)
#define uccninfo(msg, ...)
#define uccndbg(msg, ...)

#endif


#endif // UCCN_COMMON_LOGGING_H_
