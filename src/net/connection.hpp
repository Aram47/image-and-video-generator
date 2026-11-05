#pragma once

#include "net/byte_stream.hpp"
#include "net/ndjson_framer.hpp"
#include "net/request_dispatcher.hpp"

#include <cstddef>
#include <memory>
#include <mutex>

namespace gen {

class Connection {
 public:
  Connection(std::unique_ptr<ByteStream> stream, RequestDispatcher& dispatcher, std::size_t maxMessageBytes);

  void run();
  void requestStop();

 private:
  void readLoop();
  bool sendAll(std::string_view data);
  void closeStream();

  std::unique_ptr<ByteStream> stream_;
  RequestDispatcher& dispatcher_;
  NdjsonFramer framer_;
  std::mutex mutex_;
};

}  // namespace gen
