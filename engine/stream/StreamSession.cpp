#include "stream/StreamSession.h"

#include <chrono>
#include <cstring>

namespace vx {

static int64_t nowUs() {
  using namespace std::chrono;
  return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

// --- tiny JSON field readers (flat objects from our own client) ---------------
static std::string jsonStr(const std::string& j, const char* key) {
  std::string k = std::string("\"") + key + "\":";
  size_t p = j.find(k);
  if (p == std::string::npos) return {};
  p += k.size();
  while (p < j.size() && j[p] == ' ') ++p;
  if (p >= j.size() || j[p] != '"') return {};
  size_t e = j.find('"', p + 1);
  return e == std::string::npos ? std::string() : j.substr(p + 1, e - p - 1);
}
static double jsonNum(const std::string& j, const char* key, double def = 0.0) {
  std::string k = std::string("\"") + key + "\":";
  size_t p = j.find(k);
  if (p == std::string::npos) return def;
  return std::strtod(j.c_str() + p + k.size(), nullptr);
}

StreamSession::~StreamSession() { stop(); }

bool StreamSession::start(const StreamConfig& cfg, Platform& platform, AudioCallback audio) {
  cfg_ = cfg;
  cfg_.width &= ~7;
  cfg_.height &= ~7;
  platform_ = &platform;
  audio_ = std::move(audio);
  renderer_ = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  if (!yuvShader_.load("fullscreen.vert", "yuv.frag")) return false;
  yuvTex_.create2D(cfg_.width, cfg_.height * 3 / 2, GL_R8, GL_RED, GL_UNSIGNED_BYTE, nullptr, GL_NEAREST);
  yuvFbo_.create();
  yuvFbo_.attach(GL_COLOR_ATTACHMENT0, yuvTex_);
  if (!yuvFbo_.check("yuv")) return false;
  glGenBuffers(2, pbo_);
  size_t sz = size_t(cfg_.width) * size_t(cfg_.height) * 3 / 2;
  for (int i = 0; i < 2; ++i) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[i]);
    glBufferData(GL_PIXEL_PACK_BUFFER, GLsizeiptr(sz), nullptr, GL_STREAM_READ);
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glGenVertexArrays(1, &vao_);
  pending_.resize(sz);
  working_.resize(sz);

  if (!encoder_.init(cfg_.width, cfg_.height, cfg_.fps, cfg_.bitrateKbps, cfg_.encoder)) return false;

  server_.onText = [this](const std::string& m) { handleText(m); };
  server_.onConnection = [this](bool c) {
    lastActivity_ = double(nowUs()) * 1e-6;
    if (c) {
      everConnected_ = true;
      configSent_ = false;
      forceKey_ = true;
    } else {
      platform_->setRemotePad(PadType::None);
    }
  };
  platform.remoteRumble = [this](float lo, float hi, int ms) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "{\"type\":\"rumble\",\"lo\":%.3f,\"hi\":%.3f,\"ms\":%d}", lo, hi, ms);
    server_.sendText(buf);
  };
  if (!server_.start(cfg_.port, cfg_.webRoot, cfg_.token)) return false;
  startUs_ = nowUs();
  lastActivity_ = double(startUs_) * 1e-6;
  running_ = true;
  encThread_ = std::thread([this] { encoderLoop(); });
  if (audio_) audioThread_ = std::thread([this] { audioLoop(); });
  logInfo("Streaming ready: renderer '%s', encoder %s", renderer_.c_str(), encoder_.name().c_str());
  return true;
}

void StreamSession::stop() {
  if (!running_.exchange(false)) return;
  frameCv_.notify_all();
  if (encThread_.joinable()) encThread_.join();
  if (audioThread_.joinable()) audioThread_.join();
  server_.stop();
}

bool StreamSession::shouldExit() const {
  if (cfg_.idleExitSeconds <= 0.f || server_.connected()) return false;
  double idle = double(nowUs()) * 1e-6 - lastActivity_.load();
  if (!everConnected_) return idle > double(std::max(cfg_.idleExitSeconds, cfg_.firstClientWait));
  return idle > double(cfg_.idleExitSeconds);
}

void StreamSession::sendConfig() {
  if (encoder_.codecString().empty()) return;
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "{\"type\":\"config\",\"codec\":\"%s\",\"width\":%d,\"height\":%d,\"fps\":%d,\"encoder\":\"%s\",\"renderer\":\"%s\"}",
                encoder_.codecString().c_str(), cfg_.width, cfg_.height, cfg_.fps, encoder_.name().c_str(), renderer_.c_str());
  if (server_.sendText(buf)) configSent_ = true;
}

void StreamSession::handleText(const std::string& m) {
  std::string t = jsonStr(m, "t");
  InputQueue& q = platform_->injected();
  InputEvent e{};
  if (t == "kd" || t == "ku") {
    e.type = t == "kd" ? InputEvent::KeyDown : InputEvent::KeyUp;
    e.key = Input::fromBrowserCode(jsonStr(m, "c"));
    if (e.key != Key::Unknown) q.push(e);
  } else if (t == "mm") {
    e.type = InputEvent::MouseMove;
    e.x = float(jsonNum(m, "x"));
    e.y = float(jsonNum(m, "y"));
    q.push(e);
  } else if (t == "md" || t == "mu") {
    e.type = t == "md" ? InputEvent::MouseDown : InputEvent::MouseUp;
    e.button = int(jsonNum(m, "b"));
    q.push(e);
  } else if (t == "wh") {
    e.type = InputEvent::Wheel;
    e.y = float(jsonNum(m, "y"));
    q.push(e);
  } else if (t == "pa") {
    e.type = InputEvent::PadAxis;
    e.button = int(jsonNum(m, "i"));
    e.x = float(jsonNum(m, "v"));
    q.push(e);
  } else if (t == "pb") {
    e.type = InputEvent::PadButton;
    e.button = int(jsonNum(m, "i"));
    e.x = float(jsonNum(m, "v"));
    q.push(e);
  } else if (t == "hello") {
    std::string pad = jsonStr(m, "pad");
    platform_->setRemotePad(pad == "ps5" ? PadType::PS5 : pad == "ps4" ? PadType::PS4 : pad == "xbox" ? PadType::Xbox
                                                                                      : pad.empty() ? PadType::None : PadType::Other);
    forceKey_ = true;
    configSent_ = false;
  } else if (t == "ping") {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "{\"type\":\"pong\",\"ts\":%.3f,\"mbps\":%.2f}", jsonNum(m, "ts"), double(mbps_));
    server_.sendText(buf);
  } else if (t == "kf") {
    forceKey_ = true;
  }
  lastActivity_ = double(nowUs()) * 1e-6;
}

void StreamSession::captureFrame(unsigned srcTexture) {
  const int W = cfg_.width, H = cfg_.height;
  // 1) RGB -> packed I420 on the GPU.
  yuvFbo_.bind();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  yuvShader_.use();
  yuvShader_.tex("uSrc", 0, GL_TEXTURE_2D, srcTexture);
  yuvShader_.set("uSize", vec2(float(W), float(H)));
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // 2) Async readback into the current PBO; consume the previous one.
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[pboIndex_]);
  glReadPixels(0, 0, W, H * 3 / 2, GL_RED, GL_UNSIGNED_BYTE, nullptr);
  pboPending_[pboIndex_] = true;
  int prev = 1 - pboIndex_;
  if (pboPending_[prev]) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[prev]);
    size_t sz = pending_.size();
    void* p = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, GLsizeiptr(sz), GL_MAP_READ_BIT);
    if (p) {
      if (server_.connected()) {
        std::lock_guard<std::mutex> l(frameMutex_);
        std::memcpy(pending_.data(), p, sz);
        hasFrame_ = true;
      }
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    pboPending_[prev] = false;
    frameCv_.notify_one();
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  pboIndex_ = prev;
}

void StreamSession::encoderLoop() {
  uint64_t lastBytes = 0;
  int64_t lastStat = nowUs();
  while (running_) {
    {
      std::unique_lock<std::mutex> l(frameMutex_);
      frameCv_.wait_for(l, std::chrono::milliseconds(100), [&] { return hasFrame_ || !running_; });
      if (!running_) break;
      if (!hasFrame_) continue;
      std::swap(pending_, working_);
      hasFrame_ = false;
    }
    if (!server_.connected()) continue;
    int64_t pts = nowUs() - startUs_;
    bool key = forceKey_.exchange(false);
    encoder_.encode(working_.data(), pts, key, [&](const uint8_t* d, size_t n, bool isKey, int64_t p) {
      if (!configSent_ && isKey) sendConfig();
      if (!configSent_) { forceKey_ = true; return; }
      uint8_t head[10];
      head[0] = 0x01;
      head[1] = isKey ? 1 : 0;
      for (int i = 0; i < 8; ++i) head[2 + i] = uint8_t(uint64_t(p) >> (8 * i));
      server_.sendBinary2(head, sizeof(head), d, n);
    });
    int64_t now = nowUs();
    if (now - lastStat > 1000000) {
      uint64_t b = server_.bytesSent.load();
      mbps_ = float(double(b - lastBytes) * 8.0 / double(now - lastStat));
      lastBytes = b;
      lastStat = now;
    }
  }
}

void StreamSession::audioLoop() {
  const int frames = kAudioRate / 100;  // 10 ms
  std::vector<float> f(size_t(frames) * 2);
  std::vector<uint8_t> msg(8 + size_t(frames) * 4);
  int64_t next = nowUs();
  while (running_) {
    next += 10000;
    int64_t wait = next - nowUs();
    if (wait > 0) std::this_thread::sleep_for(std::chrono::microseconds(wait));
    else if (wait < -100000) next = nowUs();  // fell behind badly: resync
    std::fill(f.begin(), f.end(), 0.f);
    audio_(f.data(), frames);
    if (!server_.connected()) continue;
    msg[0] = 0x02;
    msg[1] = 2;
    msg[2] = msg[3] = 0;
    uint32_t rate = kAudioRate;
    std::memcpy(&msg[4], &rate, 4);
    int16_t* out = reinterpret_cast<int16_t*>(&msg[8]);
    for (size_t i = 0; i < f.size(); ++i) out[i] = int16_t(clampf(f[i], -1.f, 1.f) * 32767.f);
    server_.sendBinary(msg.data(), msg.size());
  }
}

}  // namespace vx
