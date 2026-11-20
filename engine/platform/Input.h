#pragma once
#include <mutex>
#include <string_view>

#include "core/Common.h"

namespace vx {

enum class Key : uint16_t {
  Unknown = 0,
  A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
  Space, LShift, RShift, LCtrl, RCtrl, LAlt, RAlt, Tab, Enter, Escape, Backspace,
  Up, Down, Left, Right,
  F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
  Count
};

enum class GamepadAxis : uint8_t { LeftX, LeftY, RightX, RightY, TriggerL, TriggerR, Count };
enum class GamepadButton : uint8_t { A, B, X, Y, LB, RB, Back, Start, LStick, RStick, DUp, DDown, DLeft, DRight, Count };

// Snapshot of input for one rendered frame.
struct Input {
  bool down[size_t(Key::Count)]{};
  bool pressed[size_t(Key::Count)]{};
  bool released[size_t(Key::Count)]{};
  float mouseDX = 0.f, mouseDY = 0.f, wheel = 0.f;
  bool mouseDown[3]{}, mousePressed[3]{};
  float axis[size_t(GamepadAxis::Count)]{};
  bool button[size_t(GamepadButton::Count)]{};
  bool buttonPressed[size_t(GamepadButton::Count)]{};
  bool gamepad = false;
  bool quit = false;

  void beginFrame();
  void key(Key k, bool isDown);
  void mouseButton(int b, bool isDown);
  void gamepadButton(GamepadButton b, bool isDown);

  bool isDown(Key k) const { return down[size_t(k)]; }
  bool wasPressed(Key k) const { return pressed[size_t(k)]; }
  float ax(GamepadAxis a) const { return axis[size_t(a)]; }
  bool btn(GamepadButton b) const { return button[size_t(b)]; }
  bool btnPressed(GamepadButton b) const { return buttonPressed[size_t(b)]; }

  // Maps a browser KeyboardEvent.code ("KeyW", "ShiftLeft", ...) to a Key.
  static Key fromBrowserCode(std::string_view code);
};

// Thread-safe queue for injected events (cloud stream input).
struct InputEvent {
  enum Type : uint8_t { KeyDown, KeyUp, MouseMove, MouseDown, MouseUp, Wheel, PadAxis, PadButton } type;
  Key key = Key::Unknown;
  int button = 0;
  float x = 0.f, y = 0.f;
};

class InputQueue {
 public:
  void push(const InputEvent& e) { std::lock_guard<std::mutex> l(m_); q_.push_back(e); }
  void drainInto(Input& in);
 private:
  std::mutex m_;
  std::vector<InputEvent> q_;
};

}  // namespace vx
