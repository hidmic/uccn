#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#include "uccn/uccn.h"

#include "simple_data.h"

bool g_stopped = false;
struct uccn_node_s * g_node = NULL;

typedef void (*sighandler_t)(int);

void sigint_handler(int signum) {
  (void)signum;
  if (g_node != NULL) {
    (void)uccn_stop(g_node);
  }
  g_stopped = true;
}

int main(int argc, char * argv[]) {
  int ret;
  struct timespec timeout;

  struct uccn_node_s node;
  struct uccn_network_s network;
  struct uccn_record_s test_record;
  struct examples_simple_data_s test_content;
  struct uccn_content_provider_s * test_provider;
  sighandler_t old_sigint_handler;

#if CONFIG_UCCN_LOGGING
  openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);
  setlogmask(LOG_UPTO(LOG_INFO));
#endif

  inet_aton("127.0.0.1", &network.inetaddr);
  inet_aton("255.0.0.0", &network.netmaskaddr);

  if (uccn_node_init(&node, &network, "provider") != 0) {
    perror("Failed to initialize 'provider' node.\n");
    return -1;
  }

  uccn_record_init(&test_record, "/test", get_examples_simple_data_typesupport());
  test_provider = uccn_advertise(&node, (struct uccn_resource_s *)&test_record);
  if (test_provider == NULL) {
    perror("Failed to advertise '/test' resource.\n");
    return -1;
  }
  strncpy(test_content.data, argc > 1 ? argv[1] : "Hello world", sizeof(test_content.data));
  test_content.number = 0;

  g_node = &node;
  old_sigint_handler = signal(SIGINT, sigint_handler);

  TIMESPEC_SECONDS_INIT(&timeout, 1);
  while (!g_stopped) {
    printf("Posting '%s'#%d on '%s' at time %ld\n",
           test_content.data, test_content.number,
           test_provider->endpoint.resource->path, time(NULL));
    if (uccn_post(test_provider, &test_content) < 0) {
      perror("Failed to post content to '/test' resource.\n");
      break;
    }
    if (uccn_spin(&node, &timeout) < 0) {
      perror("Failed to spin on 'provider' node.\n");
      break;
    }
    ++test_content.number;
  }
  signal(SIGINT, old_sigint_handler);
  g_node = NULL;

  ret = uccn_node_fini(&node);
  if (ret < 0) {
    perror("Failed to finalize 'provider' node.\n");
  }
#if CONFIG_UCCN_LOGGING
  closelog();
#endif

  return ret;
}
