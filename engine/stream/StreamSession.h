#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "gfx/Shader.h"
#include "gfx/Texture.h"
#include "platform/Platform.h"
#include "stream/VideoEncoder.h"
#include "stream/WebSocketServer.h"

namespace vx {

struct StreamConfig {
  int port = 8080;
  int width = 1280;
  int height = 720;
  int fps = 60;
  int bitrateKbps = 10000;
  std::string webRoot;
  std::string token;        // required in /ws?token=... when non-empty
  float idleExitSeconds = 0; // exit when no client for this long (0 = never)
  float firstClientWait = 1800; // before the first player arrives, wait at most this long
  std::string encoder;      // force "libx264" / "h264_nvenc"
};

// Cloud gaming session: captures the rendered frame on the GPU (RGB -> I420 in a
// shader), reads it back asynchronously, encodes H.264 on a worker thread and sends
// it over WebSocket. Audio is streamed as 16-bit PCM. Browser input is injected
// into the platform's input queue.
class StreamSession {
 public:
  ~StreamSession();
  bool start(const StreamConfig& cfg, Platform& platform, AudioCallback audio);
  void captureFrame(unsigned srcTexture);
  bool shouldExit() const;
  bool clientConnected() const { return server_.connected(); }
  float bitrateMbps() const { return mbps_; }
  void stop();

 private:
  void handleText(const std::string& msg);
  void encoderLoop();
  void audioLoop();
  void sendConfig();

  StreamConfig cfg_;
  Platform* platform_ = nullptr;
  AudioCallback audio_;
  WebSocketServer server_;
  VideoEncoder encoder_;
  Shader yuvShader_;
  Texture yuvTex_;
  Framebuffer yuvFbo_;
  unsigned pbo_[2] = {0, 0};
  int pboIndex_ = 0;
  bool pboPending_[2] = {false, false};
  unsigned vao_ = 0;

  std::thread encThread_, audioThread_;
  std::mutex frameMutex_;
  std::condition_variable frameCv_;
  std::vector<uint8_t> pending_, working_;
  bool hasFrame_ = false;
  std::atomic<bool> running_{false};
  std::atomic<bool> forceKey_{true};
  std::atomic<bool> configSent_{false};
  std::atomic<double> lastActivity_{0.0};
  std::atomic<bool> everConnected_{false};
  int64_t startUs_ = 0;
  float mbps_ = 0.f;
  std::string renderer_;
};

}  // namespace vx
