#include <signal.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "uccn/uccn.hpp"

#include "scoped_ptr.hpp"
#include "simple_data.hpp"

bool g_stopped = false;
scoped_ptr<uccn::node> g_node;

typedef void (*sighandler_t)(int);

void sigint_handler(int signum) {
  (void)signum;
  if (g_node) {
    try {
      g_node->stop();
    } catch(...) {
      // noop
    }
  }
  g_stopped = true;
}

int main(int argc, char * argv[]) {
#if CONFIG_UCCN_LOGGING
  openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);
  setlogmask(LOG_UPTO(LOG_INFO));
#endif


  using namespace std::literals::chrono_literals;
  sighandler_t old_sigint_handler = signal(SIGINT, sigint_handler);
  try {
    uccn::network network("127.0.0.1", "255.0.0.0");

    uccn::node node(network, "provider");

    uccn::record<examples::simple_data> test_record("/test");

    uccn::record_provider<examples::simple_data>
      test_provider = node.advertise(test_record);

    examples::simple_data test_content;
    strncpy(test_content.data, argc > 1 ? argv[1] : "Hello world", sizeof(test_content.data));
    test_content.number = 0;

    auto guard = g_node.reset(&node);

    while (!g_stopped) {
      std::cout << "Posting '" << test_content.data
                << "'#" << test_content.number
                << " on '" << test_record.path()
                << "' at time " << time(NULL) << std::endl;
      test_provider.post(test_content);
      node.spin(1s);
      ++test_content.number;
    };
  } catch(const std::exception & e) {
    std::cerr << e.what() << std::endl;
  }
  signal(SIGINT, old_sigint_handler);

#if CONFIG_UCCN_LOGGING
  closelog();
#endif

  return 0;
}
