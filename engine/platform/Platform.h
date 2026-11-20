#pragma once
#include <functional>

#include "core/Common.h"
#include "platform/Input.h"

namespace vx {

struct PlatformConfig {
  int width = 1600;
  int height = 900;
  bool fullscreen = false;
  bool vsync = true;
  std::string title = "Game-is-realistic";
};

enum class PadType : uint8_t { None, Xbox, PS4, PS5, Switch, Other };

// DualSense adaptive trigger modes.
enum class TriggerMode : uint8_t { Off, Resistance, Vibration };

// Audio callback: fill `frames` stereo-interleaved float samples at 48 kHz.
using AudioCallback = std::function<void(float* out, int frames)>;
constexpr int kAudioRate = 48000;

class Platform {
 public:
  virtual ~Platform() = default;
  virtual bool init(const PlatformConfig& cfg) = 0;
  virtual void pollEvents(Input& in) = 0;
  virtual void present() = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;
  // Framebuffer the final image is rendered into (0 = window back buffer).
  virtual unsigned outputFramebuffer() const = 0;
  virtual void setMouseCaptured(bool) {}
  virtual bool mouseCaptured() const { return true; }
  virtual bool startAudio(AudioCallback) { return false; }
  virtual bool isHeadless() const = 0;

  // Gamepad feedback (DualSense: rumble, adaptive triggers, lightbar).
  virtual PadType padType() const { return remotePad_; }
  virtual void rumble(float low, float high, int ms) { if (remoteRumble) remoteRumble(low, high, ms); }
  // trigger: 0 = L2, 1 = R2; strength 0..1; start 0..1 position where resistance begins.
  virtual void triggerEffect(int trigger, TriggerMode mode, float strength, float start) {}
  virtual void lightbar(const vec3& rgb) {}
  // Cloud stream: client reports its pad, and receives rumble requests.
  void setRemotePad(PadType t) { remotePad_ = t; }
  std::function<void(float, float, int)> remoteRumble;
  // Events injected from outside the OS event loop (cloud stream input).
  InputQueue& injected() { return queue_; }

 protected:
  InputQueue queue_;
  PadType remotePad_ = PadType::None;
};

std::unique_ptr<Platform> createSdlPlatform();
#if defined(VERITAS_HAS_STREAMING)
std::unique_ptr<Platform> createHeadlessPlatform();
#endif

}  // namespace vx
