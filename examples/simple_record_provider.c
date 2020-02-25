#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#include "uccn/uccn.h"

#include "simple_record.h"

int main(int argc, char * argv[]) {
  int ret;
  struct timespec timeout;

  struct uccn_node_s node;
  struct uccn_network_s network;
  struct uccn_record_s test_record;
  struct simple_record_data_s test_content;
  struct uccn_content_provider_s * test_provider;

#ifdef CONFIG_UCCN_LOGGING
  openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);
#endif
  setlogmask(LOG_UPTO(LOG_INFO));

  if (!inet_aton("127.0.0.1", &network.inetaddr) ||
      !inet_aton("255.0.0.0", &network.netmaskaddr))
  {
    return -1;
  }

  if (uccn_node_init(&node, "provider", &network) != 0) {
    return -1;
  }

  uccn_record_init(&test_record, "/test", &simple_record_typesupport);
  test_provider = uccn_advertise(&node, (struct uccn_resource_s *)&test_record);
  if (test_provider == NULL) {
    return -1;
  }
  strncpy(test_content.data, argc > 1 ? argv[1] : "Hello world", sizeof(test_content.data));

  TIMESPEC_SECONDS_INIT(&timeout, 1);
  while (true) {
    printf("Posting '%s' on '%s' at time %ld\n", test_content.data,
           test_provider->endpoint.resource->path, time(NULL));
    if (uccn_post(test_provider, &test_content) < 0) {
      break;
    }
    if (uccn_spin(&node, &timeout) != 0) {
      break;
    }
  }

  ret = uccn_node_fini(&node);

#ifdef CONFIG_UCCN_LOGGING
  closelog();
#endif

  return ret;
}
