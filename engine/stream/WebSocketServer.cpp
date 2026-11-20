#include "stream/WebSocketServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>

#include "core/FileSystem.h"

namespace vx {

// ---------------------------------------------------------------------------
//  SHA-1 + Base64 for the WebSocket handshake
// ---------------------------------------------------------------------------
static inline uint32_t rol(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

static void sha1(const std::string& in, uint8_t out[20]) {
  uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
  std::vector<uint8_t> m(in.begin(), in.end());
  uint64_t bits = uint64_t(in.size()) * 8;
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  for (int i = 7; i >= 0; --i) m.push_back(uint8_t(bits >> (i * 8)));
  for (size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
      w[i] = uint32_t(m[off + 4 * i]) << 24 | uint32_t(m[off + 4 * i + 1]) << 16 | uint32_t(m[off + 4 * i + 2]) << 8 | m[off + 4 * i + 3];
    for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6; }
      uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
  }
  uint32_t hs[5] = {h0, h1, h2, h3, h4};
  for (int i = 0; i < 5; ++i)
    for (int j = 0; j < 4; ++j) out[i * 4 + j] = uint8_t(hs[i] >> (24 - 8 * j));
}

static std::string base64(const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = uint32_t(d[i]) << 16 | (i + 1 < n ? uint32_t(d[i + 1]) << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    o += T[(v >> 18) & 63];
    o += T[(v >> 12) & 63];
    o += i + 1 < n ? T[(v >> 6) & 63] : '=';
    o += i + 2 < n ? T[v & 63] : '=';
  }
  return o;
}

static bool sendAll(int fd, const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  while (len > 0) {
    ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
    if (n <= 0) return false;
    p += n;
    len -= size_t(n);
  }
  return true;
}

static bool recvAll(int fd, void* data, size_t len) {
  uint8_t* p = static_cast<uint8_t*>(data);
  while (len > 0) {
    ssize_t n = ::recv(fd, p, len, 0);
    if (n <= 0) return false;
    p += n;
    len -= size_t(n);
  }
  return true;
}

static std::string headerValue(const std::string& req, const std::string& name) {
  std::string lower = req;
  for (auto& c : lower) c = char(std::tolower(c));
  std::string key = "\r\n" + name + ":";
  size_t p = lower.find(key);
  if (p == std::string::npos) return {};
  p += key.size();
  size_t e = req.find("\r\n", p);
  std::string v = req.substr(p, e - p);
  size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
  return a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
}

static const char* mimeType(const std::string& path) {
  auto ends = [&](const char* s) { size_t n = std::strlen(s); return path.size() >= n && path.compare(path.size() - n, n, s) == 0; };
  if (ends(".html")) return "text/html; charset=utf-8";
  if (ends(".js") || ends(".mjs")) return "text/javascript; charset=utf-8";
  if (ends(".css")) return "text/css; charset=utf-8";
  if (ends(".json")) return "application/json";
  if (ends(".png")) return "image/png";
  if (ends(".jpg")) return "image/jpeg";
  if (ends(".svg")) return "image/svg+xml";
  if (ends(".wasm")) return "application/wasm";
  return "application/octet-stream";
}

// ---------------------------------------------------------------------------

WebSocketServer::~WebSocketServer() { stop(); }

bool WebSocketServer::start(int port, const std::string& webRoot, const std::string& token) {
  webRoot_ = webRoot;
  token_ = token;
  listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) { logError("socket() failed"); return false; }
  int one = 1;
  setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(uint16_t(port));
  addr.sin_addr.s_addr = INADDR_ANY;
  if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    logError("bind(%d) failed: %s", port, std::strerror(errno));
    return false;
  }
  ::listen(listenFd_, 16);
  running_ = true;
  acceptThread_ = std::thread([this] { acceptLoop(); });
  logInfo("Stream server listening on port %d (web root: %s)", port, webRoot_.c_str());
  return true;
}

void WebSocketServer::stop() {
  if (!running_.exchange(false)) return;
  if (listenFd_ >= 0) { ::shutdown(listenFd_, SHUT_RDWR); ::close(listenFd_); listenFd_ = -1; }
  int c = client_.exchange(-1);
  if (c >= 0) ::shutdown(c, SHUT_RDWR);
  if (acceptThread_.joinable()) acceptThread_.join();
}

void WebSocketServer::acceptLoop() {
  while (running_) {
    sockaddr_in ca{};
    socklen_t cl = sizeof(ca);
    int fd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&ca), &cl);
    if (fd < 0) continue;
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    int buf = 4 << 20;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
    std::thread([this, fd] { handleConnection(fd); }).detach();
  }
}

void WebSocketServer::handleConnection(int fd) {
  std::string req;
  char buf[4096];
  while (req.find("\r\n\r\n") == std::string::npos && req.size() < 65536) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) { ::close(fd); return; }
    req.append(buf, size_t(n));
  }
  bool upgrade = !headerValue(req, "sec-websocket-key").empty();
  if (!upgrade) {
    serveHttp(fd, req);
    ::close(fd);
    return;
  }
  // Token check: /ws?token=...
  size_t sp = req.find(' '), sp2 = req.find(' ', sp + 1);
  std::string target = req.substr(sp + 1, sp2 - sp - 1);
  if (!token_.empty() && target.find("token=" + token_) == std::string::npos) {
    const char* r = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    sendAll(fd, r, std::strlen(r));
    ::close(fd);
    return;
  }
  std::string key = headerValue(req, "sec-websocket-key");
  uint8_t digest[20];
  sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", digest);
  std::string resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
                     base64(digest, 20) + "\r\n\r\n";
  sendAll(fd, resp.data(), resp.size());
  int expected = -1;
  if (!client_.compare_exchange_strong(expected, fd)) {
    // Someone is already playing on this machine.
    const std::string msg = "{\"type\":\"busy\"}";
    std::vector<uint8_t> f = {0x81, uint8_t(msg.size())};
    f.insert(f.end(), msg.begin(), msg.end());
    sendAll(fd, f.data(), f.size());
    ::close(fd);
    return;
  }
  logInfo("Client connected");
  if (onConnection) onConnection(true);
  wsLoop(fd);
  {
    std::lock_guard<std::mutex> l(sendMutex_);
    client_ = -1;
  }
  ::close(fd);
  logInfo("Client disconnected");
  if (onConnection) onConnection(false);
}

bool WebSocketServer::serveHttp(int fd, const std::string& req) {
  size_t sp = req.find(' '), sp2 = req.find(' ', sp + 1);
  std::string target = req.substr(sp + 1, sp2 - sp - 1);
  size_t q = target.find('?');
  std::string path = q == std::string::npos ? target : target.substr(0, q);
  if (path == "/health") {
    const char* r = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
    return sendAll(fd, r, std::strlen(r));
  }
  if (path.empty() || path == "/") path = "/index.html";
  if (path.find("..") != std::string::npos || webRoot_.empty()) {
    const char* r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    return sendAll(fd, r, std::strlen(r));
  }
  std::vector<uint8_t> body;
  std::string file = webRoot_ + path;
  if (!readBinaryFile(file, body)) {
    const char* r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    return sendAll(fd, r, std::strlen(r));
  }
  std::string head = "HTTP/1.1 200 OK\r\nContent-Type: " + std::string(mimeType(path)) +
                     "\r\nCache-Control: no-cache\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " +
                     std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
  return sendAll(fd, head.data(), head.size()) && sendAll(fd, body.data(), body.size());
}

void WebSocketServer::wsLoop(int fd) {
  std::string message;
  int msgOpcode = 0;
  while (running_) {
    uint8_t h[2];
    if (!recvAll(fd, h, 2)) return;
    bool fin = h[0] & 0x80;
    int opcode = h[0] & 0x0F;
    bool masked = h[1] & 0x80;
    uint64_t len = h[1] & 0x7F;
    if (len == 126) {
      uint8_t e[2];
      if (!recvAll(fd, e, 2)) return;
      len = uint64_t(e[0]) << 8 | e[1];
    } else if (len == 127) {
      uint8_t e[8];
      if (!recvAll(fd, e, 8)) return;
      len = 0;
      for (int i = 0; i < 8; ++i) len = len << 8 | e[i];
    }
    if (len > (16u << 20)) return;
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked && !recvAll(fd, mask, 4)) return;
    std::string payload(size_t(len), '\0');
    if (len && !recvAll(fd, payload.data(), size_t(len))) return;
    if (masked)
      for (size_t i = 0; i < payload.size(); ++i) payload[i] = char(payload[i] ^ mask[i & 3]);
    if (opcode == 8) {  // close
      sendFrame(8, nullptr, 0, nullptr, 0);
      return;
    }
    if (opcode == 9) {  // ping -> pong
      sendFrame(10, reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), nullptr, 0);
      continue;
    }
    if (opcode == 10) continue;
    if (opcode != 0) { message.clear(); msgOpcode = opcode; }
    message += payload;
    if (fin) {
      if (msgOpcode == 1 && onText) onText(message);
      message.clear();
    }
  }
}

bool WebSocketServer::sendFrame(int opcode, const uint8_t* a, size_t alen, const uint8_t* b, size_t blen) {
  std::lock_guard<std::mutex> l(sendMutex_);
  int fd = client_.load();
  if (fd < 0) return false;
  size_t len = alen + blen;
  uint8_t h[10];
  size_t hl;
  h[0] = uint8_t(0x80 | opcode);
  if (len < 126) { h[1] = uint8_t(len); hl = 2; }
  else if (len < 65536) { h[1] = 126; h[2] = uint8_t(len >> 8); h[3] = uint8_t(len); hl = 4; }
  else {
    h[1] = 127;
    for (int i = 0; i < 8; ++i) h[2 + i] = uint8_t(uint64_t(len) >> (56 - 8 * i));
    hl = 10;
  }
  bool ok = sendAll(fd, h, hl) && (alen == 0 || sendAll(fd, a, alen)) && (blen == 0 || sendAll(fd, b, blen));
  if (ok) bytesSent += hl + len;
  return ok;
}

bool WebSocketServer::sendBinary(const uint8_t* data, size_t len) { return sendFrame(2, data, len, nullptr, 0); }
bool WebSocketServer::sendBinary2(const uint8_t* head, size_t hl, const uint8_t* body, size_t bl) { return sendFrame(2, head, hl, body, bl); }
bool WebSocketServer::sendText(const std::string& s) {
  return sendFrame(1, reinterpret_cast<const uint8_t*>(s.data()), s.size(), nullptr, 0);
}

}  // namespace vx
