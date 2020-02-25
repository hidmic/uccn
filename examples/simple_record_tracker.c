#include <stdio.h>
#include <syslog.h>

#include "uccn/uccn.h"

#include "simple_record.h"

void print_simple_record(struct uccn_content_tracker_s * tracker,
                         struct simple_record_data_s * test_content) {
  printf("Got '%s' on '%s'\n", test_content->data, tracker->endpoint.resource->path);
}

int main(int argc, char * argv[]) {
  int ret;
  struct uccn_node_s node;
  struct uccn_network_s network;
  struct uccn_record_s test_record;

  (void)argc;
#ifdef CONFIG_UCCN_LOGGING
  openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);
#endif
  setlogmask(LOG_UPTO(LOG_INFO));

  if (!inet_aton("127.0.0.1", &network.inetaddr) ||
      !inet_aton("255.0.0.0", &network.netmaskaddr))
  {
    return -1;
  }

  if (uccn_node_init(&node, "tracker", &network) != 0) {
    return -1;
  }

  uccn_record_init(&test_record, "/test", &simple_record_typesupport);
  if (NULL == uccn_track(&node, (struct uccn_resource_s *)&test_record,
                         (uccn_content_track_fn)print_simple_record)) {
    return -1;
  }

  uccn_spin(&node, NULL);

  ret = uccn_node_fini(&node);

#ifdef CONFIG_UCCN_LOGGING
  closelog();
#endif

  return ret;
}
