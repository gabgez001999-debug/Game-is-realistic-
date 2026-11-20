#include <SDL.h>

#include "platform/GL.h"
#include "platform/Platform.h"

namespace vx {

static Key mapScancode(SDL_Scancode sc) {
  if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return Key(uint16_t(Key::A) + uint16_t(sc - SDL_SCANCODE_A));
  if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return Key(uint16_t(Key::Num1) + uint16_t(sc - SDL_SCANCODE_1));
  if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return Key(uint16_t(Key::F1) + uint16_t(sc - SDL_SCANCODE_F1));
  switch (sc) {
    case SDL_SCANCODE_0: return Key::Num0;
    case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_LSHIFT: return Key::LShift;
    case SDL_SCANCODE_RSHIFT: return Key::RShift;
    case SDL_SCANCODE_LCTRL: return Key::LCtrl;
    case SDL_SCANCODE_RCTRL: return Key::RCtrl;
    case SDL_SCANCODE_LALT: return Key::LAlt;
    case SDL_SCANCODE_RALT: return Key::RAlt;
    case SDL_SCANCODE_TAB: return Key::Tab;
    case SDL_SCANCODE_RETURN: return Key::Enter;
    case SDL_SCANCODE_ESCAPE: return Key::Escape;
    case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
    case SDL_SCANCODE_UP: return Key::Up;
    case SDL_SCANCODE_DOWN: return Key::Down;
    case SDL_SCANCODE_LEFT: return Key::Left;
    case SDL_SCANCODE_RIGHT: return Key::Right;
    default: return Key::Unknown;
  }
}

class SdlPlatform final : public Platform {
 public:
  ~SdlPlatform() override {
    if (audioDev_) SDL_CloseAudioDevice(audioDev_);
    if (pad_) SDL_GameControllerClose(pad_);
    if (ctx_) SDL_GL_DeleteContext(ctx_);
    if (win_) SDL_DestroyWindow(win_);
    SDL_Quit();
  }

  bool init(const PlatformConfig& cfg) override {
    // DualSense: enable the enhanced HID report so rumble, triggers and lightbar work over Bluetooth too.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
      logError("SDL_Init failed: %s", SDL_GetError());
      return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    win_ = SDL_CreateWindow(cfg.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cfg.width, cfg.height, flags);
    if (!win_) { logError("SDL_CreateWindow failed: %s", SDL_GetError()); return false; }
    ctx_ = SDL_GL_CreateContext(win_);
    if (!ctx_) { logError("OpenGL 4.1 core context failed: %s", SDL_GetError()); return false; }
    SDL_GL_MakeCurrent(win_, ctx_);
    if (!gl::load([](const char* n) { return SDL_GL_GetProcAddress(n); })) return false;
    SDL_GL_SetSwapInterval(cfg.vsync ? 1 : 0);
    logInfo("OpenGL: %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
    setMouseCaptured(true);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
      if (SDL_IsGameController(i)) { openPad(i); break; }
    return true;
  }

  void pollEvents(Input& in) override {
    in.beginFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: in.quit = true; break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          if (!e.key.repeat) in.key(mapScancode(e.key.keysym.scancode), e.type == SDL_KEYDOWN);
          break;
        case SDL_MOUSEMOTION:
          if (captured_) { in.mouseDX += float(e.motion.xrel); in.mouseDY += float(e.motion.yrel); }
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
          int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : 2;
          in.mouseButton(b, e.type == SDL_MOUSEBUTTONDOWN);
          if (e.type == SDL_MOUSEBUTTONDOWN && !captured_) setMouseCaptured(true);
          break;
        }
        case SDL_MOUSEWHEEL: in.wheel += float(e.wheel.y); break;
        case SDL_CONTROLLERDEVICEADDED:
          if (!pad_) openPad(e.cdevice.which);
          break;
        case SDL_CONTROLLERDEVICEREMOVED:
          if (pad_ && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad_)) == e.cdevice.which) {
            SDL_GameControllerClose(pad_);
            pad_ = nullptr;
            type_ = PadType::None;
            logInfo("Controller disconnected");
          }
          break;
        default: break;
      }
    }
    if (pad_) {
      auto ax = [&](SDL_GameControllerAxis a) {
        float v = float(SDL_GameControllerGetAxis(pad_, a)) / 32767.f;
        return std::fabs(v) < 0.12f ? 0.f : clampf(v, -1.f, 1.f);
      };
      in.gamepad = true;
      in.axis[size_t(GamepadAxis::LeftX)] = ax(SDL_CONTROLLER_AXIS_LEFTX);
      in.axis[size_t(GamepadAxis::LeftY)] = ax(SDL_CONTROLLER_AXIS_LEFTY);
      in.axis[size_t(GamepadAxis::RightX)] = ax(SDL_CONTROLLER_AXIS_RIGHTX);
      in.axis[size_t(GamepadAxis::RightY)] = ax(SDL_CONTROLLER_AXIS_RIGHTY);
      in.axis[size_t(GamepadAxis::TriggerL)] = ax(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
      in.axis[size_t(GamepadAxis::TriggerR)] = ax(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
      static const SDL_GameControllerButton map[] = {
          SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
          SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK,
          SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
          SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
          SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
      for (size_t i = 0; i < size_t(GamepadButton::Count); ++i)
        in.gamepadButton(GamepadButton(i), SDL_GameControllerGetButton(pad_, map[i]) != 0);
    } else {
      in.gamepad = false;
    }
    queue_.drainInto(in);
    if (in.wasPressed(Key::Escape) && captured_) setMouseCaptured(false);
  }

  void present() override { SDL_GL_SwapWindow(win_); }
  int width() const override { int w, h; SDL_GL_GetDrawableSize(win_, &w, &h); return w; }
  int height() const override { int w, h; SDL_GL_GetDrawableSize(win_, &w, &h); return h; }
  unsigned outputFramebuffer() const override { return 0; }
  bool isHeadless() const override { return false; }
  bool mouseCaptured() const override { return captured_; }

  void setMouseCaptured(bool c) override {
    captured_ = c;
    SDL_SetRelativeMouseMode(c ? SDL_TRUE : SDL_FALSE);
  }

  PadType padType() const override { return pad_ ? type_ : remotePad_; }

  void rumble(float low, float high, int ms) override {
    if (pad_) SDL_GameControllerRumble(pad_, Uint16(saturate(low) * 65535.f), Uint16(saturate(high) * 65535.f), Uint32(ms));
    else if (remoteRumble) remoteRumble(low, high, ms);
  }

  void triggerEffect(int trigger, TriggerMode mode, float strength, float start) override {
    if (!pad_ || type_ != PadType::PS5) return;
    // Skip redundant writes (the controller is updated over HID).
    int key = int(mode) * 1000000 + int(strength * 255.f) * 1000 + int(start * 255.f);
    if (trigLast_[trigger & 1] == key) return;
    trigLast_[trigger & 1] = key;
    uint8_t state[47] = {};
    state[0] = trigger == 0 ? 0x08 : 0x04;  // modify left / right trigger effect
    uint8_t* fx = trigger == 0 ? &state[21] : &state[10];
    if (mode == TriggerMode::Off) {
      fx[0] = 0x05;
    } else if (mode == TriggerMode::Resistance) {
      fx[0] = 0x01;  // continuous resistance: start position, force
      fx[1] = uint8_t(saturate(start) * 255.f);
      fx[2] = uint8_t(saturate(strength) * 255.f);
    } else {
      fx[0] = 0x06;  // vibration: frequency, amplitude, start
      fx[1] = 22;
      fx[2] = uint8_t(saturate(strength) * 63.f);
      fx[3] = uint8_t(saturate(start) * 255.f);
    }
    SDL_GameControllerSendEffect(pad_, state, sizeof(state));
  }

  void lightbar(const vec3& c) override {
    if (!pad_) return;
    uint32_t key = (uint32_t(saturate(c.r) * 255.f) << 16) | (uint32_t(saturate(c.g) * 255.f) << 8) | uint32_t(saturate(c.b) * 255.f);
    if (key == ledLast_) return;
    ledLast_ = key;
    SDL_GameControllerSetLED(pad_, Uint8(key >> 16), Uint8(key >> 8), Uint8(key));
  }

  bool startAudio(AudioCallback cb) override {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { logWarn("SDL audio unavailable: %s", SDL_GetError()); return false; }
    audioCb_ = std::move(cb);
    SDL_AudioSpec want{}, have{};
    want.freq = kAudioRate;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 512;
    want.userdata = this;
    want.callback = [](void* ud, Uint8* stream, int len) {
      auto* self = static_cast<SdlPlatform*>(ud);
      self->audioCb_(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
    };
    audioDev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!audioDev_) { logWarn("No audio device: %s", SDL_GetError()); return false; }
    SDL_PauseAudioDevice(audioDev_, 0);
    logInfo("Audio: %d Hz, %d ch, %d frames", have.freq, have.channels, have.samples);
    return true;
  }

 private:
  void openPad(int index) {
    pad_ = SDL_GameControllerOpen(index);
    if (!pad_) return;
    switch (SDL_GameControllerGetType(pad_)) {
      case SDL_CONTROLLER_TYPE_PS5: type_ = PadType::PS5; break;
      case SDL_CONTROLLER_TYPE_PS4: type_ = PadType::PS4; break;
      case SDL_CONTROLLER_TYPE_XBOX360:
      case SDL_CONTROLLER_TYPE_XBOXONE: type_ = PadType::Xbox; break;
      case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: type_ = PadType::Switch; break;
      default: type_ = PadType::Other; break;
    }
    trigLast_[0] = trigLast_[1] = -1;
    ledLast_ = 0xFFFFFFFFu;
    logInfo("Controller connected: %s%s", SDL_GameControllerName(pad_), type_ == PadType::PS5 ? " (DualSense: rumble, adaptive triggers, lightbar)" : "");
  }

  SDL_Window* win_ = nullptr;
  PadType type_ = PadType::None;
  int trigLast_[2] = {-1, -1};
  uint32_t ledLast_ = 0xFFFFFFFFu;
  SDL_GLContext ctx_ = nullptr;
  SDL_GameController* pad_ = nullptr;
  SDL_AudioDeviceID audioDev_ = 0;
  AudioCallback audioCb_;
  bool captured_ = false;
};

std::unique_ptr<Platform> createSdlPlatform() { return std::make_unique<SdlPlatform>(); }

}  // namespace vx
