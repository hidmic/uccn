#include <signal.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>

#include "uccn/uccn.h"

#include "simple_data.h"

struct uccn_node_s * g_node;

typedef void (*sighandler_t)(int);

void sigint_handler(int signum) {
  (void)signum;
  (void)uccn_stop(g_node);
}

void print_simple_record(struct uccn_content_tracker_s * tracker,
                         struct examples_simple_data_s * test_content) {
  printf("Got '%s'#%d on '%s' at time %ld\n", test_content->data,
         test_content->number, tracker->endpoint.resource->path,
         time(NULL));
}

int main(int argc, char * argv[]) {
  int ret;
  struct uccn_node_s node;
  struct uccn_network_s network;
  struct uccn_record_s test_record;
  sighandler_t old_sigint_handler;

  (void)argc;
#if CONFIG_UCCN_LOGGING
  openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);
  //setlogmask(LOG_UPTO(LOG_INFO));
#endif

  inet_aton("127.0.0.1", &network.inetaddr);
  inet_aton("255.0.0.0", &network.netmask);

  if (uccn_node_init(&node, &network, "tracker") != 0) {
    perror("Failed to initialize tracker node.\n");
    return -1;
  }

  uccn_record_init(&test_record, "/test", get_examples_simple_data_typesupport());
  if (NULL == uccn_track(&node, (struct uccn_resource_s *)&test_record,
                         (uccn_content_track_fn)print_simple_record, NULL)) {
    perror("Failed to track /test resource.\n");
    return -1;
  }

  g_node = &node;
  old_sigint_handler = signal(SIGINT, sigint_handler);

  if (uccn_spin(&node, NULL) < 0) {
    perror("Failed to spin on 'tracker' node.\n");
  }

  signal(SIGINT, old_sigint_handler);
  g_node = NULL;

  ret = uccn_node_fini(&node);
  if (ret < 0) {
    perror("Failed to finalize 'tracker' node.\n");
  }

#if CONFIG_UCCN_LOGGING
  closelog();
#endif

  return ret;
}
