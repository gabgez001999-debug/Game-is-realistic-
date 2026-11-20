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
      if (SDL_IsGameController(i)) { pad_ = SDL_GameControllerOpen(i); break; }
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
          if (!pad_) pad_ = SDL_GameControllerOpen(e.cdevice.which);
          break;
        case SDL_CONTROLLERDEVICEREMOVED:
          if (pad_) { SDL_GameControllerClose(pad_); pad_ = nullptr; }
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
  SDL_Window* win_ = nullptr;
  SDL_GLContext ctx_ = nullptr;
  SDL_GameController* pad_ = nullptr;
  SDL_AudioDeviceID audioDev_ = 0;
  AudioCallback audioCb_;
  bool captured_ = false;
};

std::unique_ptr<Platform> createSdlPlatform() { return std::make_unique<SdlPlatform>(); }

}  // namespace vx
