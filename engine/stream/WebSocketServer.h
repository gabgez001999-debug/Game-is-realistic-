#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

#include "core/Common.h"

namespace vx {

// Minimal HTTP/1.1 static file server + RFC 6455 WebSocket endpoint (/ws).
// One active game client at a time; additional clients get "busy".
class WebSocketServer {
 public:
  using TextHandler = std::function<void(const std::string&)>;
  using EventHandler = std::function<void(bool connected)>;

  ~WebSocketServer();
  bool start(int port, const std::string& webRoot, const std::string& token);
  void stop();

  bool connected() const { return client_.load() >= 0; }
  bool sendBinary(const uint8_t* data, size_t len);
  bool sendBinary2(const uint8_t* head, size_t headLen, const uint8_t* body, size_t bodyLen);
  bool sendText(const std::string& s);

  TextHandler onText;
  EventHandler onConnection;
  std::atomic<uint64_t> bytesSent{0};

 private:
  void acceptLoop();
  void handleConnection(int fd);
  bool serveHttp(int fd, const std::string& request);
  void wsLoop(int fd);
  bool sendFrame(int opcode, const uint8_t* a, size_t alen, const uint8_t* b, size_t blen);

  int listenFd_ = -1;
  std::atomic<int> client_{-1};
  std::atomic<bool> running_{false};
  std::thread acceptThread_;
  std::mutex sendMutex_;
  std::string webRoot_, token_;
};

}  // namespace vx
