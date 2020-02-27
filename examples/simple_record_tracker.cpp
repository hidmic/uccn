#include <signal.h>
#include <time.h>
#include <syslog.h>

#include <iostream>
#include <stdexcept>

#include "uccn/uccn.hpp"

#include "scoped_ptr.hpp"
#include "simple_data.hpp"

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
}

int main(int argc, char * argv[]) {
  (void)argc;
#if CONFIG_UCCN_LOGGING
  openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);
  setlogmask(LOG_UPTO(LOG_INFO));
#endif


  sighandler_t old_sigint_handler = signal(SIGINT, sigint_handler);
  try {
    uccn::network network("127.0.0.1", "255.0.0.0");
    uccn::node node(network, "tracker");

    uccn::record<examples::simple_data> test_record("/test");
    node.track<examples::simple_data>(
        test_record, [&test_record](const examples::simple_data & data) {
          std::cout << "Got '" << data.data << "'#" << data.number
                    << " on '" << test_record.path()
                    << "' at time " << time(NULL) << std::endl;
        });

    auto guard = g_node.reset(&node);

    node.spin();
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;
  }
  signal(SIGINT, old_sigint_handler);

#if CONFIG_UCCN_LOGGING
  closelog();
#endif

  return 0;
}
