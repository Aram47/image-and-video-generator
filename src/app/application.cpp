#include "app/application.hpp"

#include "jobs/generation_executor.hpp"
#include "storage/frame_cleaner.hpp"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace gen {

Application::Application(ServerConfig config, std::unique_ptr<IJobExecutor> executor, std::ostream* log)
    : config_(std::move(config)),
      logStream_(log == nullptr ? &std::cerr : log),
      logger_(*logStream_),
      output_(config_.outputDirectory),
      quota_(config_.outputDirectory, config_.maxOutputBytes),
      jobsOnDisk_(config_.jobDirectory, config_.outputDirectory),
      metrics_(logger_, config_.dataDirectory() / "metrics.json"),
      verifier_(),
      jobs_(config_.queueCapacity,
            executor ? std::move(executor)
                     : std::make_unique<GenerationExecutor>(output_, config_.keepFramesOnEncodingFailure),
            logger_, jobsOnDisk_, quota_, metrics_, config_.logPrompts),
      dispatcher_(jobs_, logger_, config_.maxMessageBytes, metrics_, verifier_),
      server_(config_, dispatcher_, logger_) {}

Application::~Application() { stop(); }

void Application::start() {
  if (stopped_.load()) {
    throw std::logic_error("Application is stopped.");
  }
  config_.validate();
  config_.ensureTlsMaterial();
  if (config_.authRequired) {
    verifier_.require(config_.readAuthToken());
  } else {
    verifier_.disable();
  }
  std::signal(SIGPIPE, SIG_IGN);
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    throw std::logic_error("Application is already running.");
  }
  try {
    lock_ = std::make_unique<ProcessLock>(config_.dataDirectory() / "server.lock");
    FrameCleaner::purge(config_.outputDirectory, config_.frameRetentionSeconds);
    quota_.scan();
    jobs_.start();
    server_.start();
  } catch (...) {
    try {
      server_.stop();
    } catch (...) {
    }
    jobs_.stop();
    lock_.reset();
    running_.store(false);
    throw;
  }
}

void Application::stop() {
  if (stopped_.exchange(true)) {
    return;
  }
  try {
    server_.stop();
    jobs_.stop();
  } catch (...) {
  }
  lock_.reset();
  running_.store(false);
}

std::uint16_t Application::port() const { return server_.port(); }

const std::string& Application::boundAddress() const { return server_.boundAddress(); }

}  // namespace gen
