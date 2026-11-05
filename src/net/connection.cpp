#include "net/connection.hpp"

#include <string>

namespace gen {

Connection::Connection(std::unique_ptr<ByteStream> stream, RequestDispatcher& dispatcher, std::size_t maxMessageBytes)
    : stream_(std::move(stream)), dispatcher_(dispatcher), framer_(maxMessageBytes) {}

void Connection::run() {
  struct Closer {
    Connection& connection;
    ~Closer() { connection.closeStream(); }
  } closer{*this};

  try {
    readLoop();
  } catch (...) {
  }
}

void Connection::requestStop() {
  std::lock_guard lock(mutex_);
  if (stream_) {
    stream_->shutdown();
  }
}

void Connection::closeStream() {
  std::lock_guard lock(mutex_);
  stream_.reset();
}

bool Connection::sendAll(std::string_view data) {
  while (!data.empty()) {
    const ssize_t sent = stream_->write(data.data(), data.size());
    if (sent < 0) {
      return false;
    }
    if (sent == 0) {
      return false;
    }
    data.remove_prefix(static_cast<std::size_t>(sent));
  }
  return true;
}

void Connection::readLoop() {
  char buffer[4096];
  for (;;) {
    const ssize_t count = stream_->read(buffer, sizeof(buffer));
    if (count == 0) {
      return;
    }
    if (count < 0) {
      return;
    }
    const auto frames = framer_.append(std::string_view(buffer, static_cast<std::size_t>(count)));
    for (const Frame& frame : frames) {
      const std::string response =
          frame.kind == FrameKind::TooLarge ? dispatcher_.handleTooLarge() : dispatcher_.handleLine(frame.payload);
      if (!sendAll(response + "\n")) {
        return;
      }
    }
  }
}

}  // namespace gen
