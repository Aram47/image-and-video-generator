#include "app/application.hpp"
#include "app/server_config.hpp"

#include <pthread.h>
#include <signal.h>

#include <csignal>
#include <iostream>

int main(int argc, char** argv) {
  std::signal(SIGPIPE, SIG_IGN);

  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGTERM);
  if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
    std::cerr << "Failed to block signals.\n";
    return 1;
  }

  try {
    if (argc > 2) {
      std::cerr << "Usage: generator_server [config]\n";
      return 1;
    }
    gen::ServerConfig config = argc == 2 ? gen::ServerConfig::load(argv[1]) : gen::ServerConfig{};
    gen::Application application(std::move(config));
    application.start();
    int received = 0;
    if (sigwait(&signals, &received) != 0) {
      application.stop();
      return 1;
    }
    application.stop();
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
  return 0;
}
