#include "platform/Input.h"

#include <cstring>

namespace vx {

void Input::beginFrame() {
  std::memset(pressed, 0, sizeof(pressed));
  std::memset(released, 0, sizeof(released));
  std::memset(mousePressed, 0, sizeof(mousePressed));
  std::memset(buttonPressed, 0, sizeof(buttonPressed));
  mouseDX = mouseDY = wheel = 0.f;
}

void Input::key(Key k, bool isDown) {
  size_t i = size_t(k);
  if (k == Key::Unknown || i >= size_t(Key::Count)) return;
  if (isDown && !down[i]) pressed[i] = true;
  if (!isDown && down[i]) released[i] = true;
  down[i] = isDown;
}

void Input::mouseButton(int b, bool isDown) {
  if (b < 0 || b > 2) return;
  if (isDown && !mouseDown[b]) mousePressed[b] = true;
  mouseDown[b] = isDown;
}

void Input::gamepadButton(GamepadButton b, bool isDown) {
  size_t i = size_t(b);
  if (i >= size_t(GamepadButton::Count)) return;
  if (isDown && !button[i]) buttonPressed[i] = true;
  button[i] = isDown;
}

Key Input::fromBrowserCode(std::string_view c) {
  if (c.size() == 4 && c.substr(0, 3) == "Key" && c[3] >= 'A' && c[3] <= 'Z')
    return Key(uint16_t(Key::A) + uint16_t(c[3] - 'A'));
  if (c.size() == 6 && c.substr(0, 5) == "Digit" && c[5] >= '0' && c[5] <= '9')
    return Key(uint16_t(Key::Num0) + uint16_t(c[5] - '0'));
  if (c.size() >= 2 && c[0] == 'F' && c[1] >= '1' && c[1] <= '9') {
    int n = 0;
    for (size_t i = 1; i < c.size(); ++i) {
      if (c[i] < '0' || c[i] > '9') return Key::Unknown;
      n = n * 10 + (c[i] - '0');
    }
    if (n >= 1 && n <= 12) return Key(uint16_t(Key::F1) + uint16_t(n - 1));
  }
  struct M { std::string_view s; Key k; };
  static const M map[] = {
      {"Space", Key::Space},       {"ShiftLeft", Key::LShift},   {"ShiftRight", Key::RShift},
      {"ControlLeft", Key::LCtrl}, {"ControlRight", Key::RCtrl}, {"AltLeft", Key::LAlt},
      {"AltRight", Key::RAlt},     {"Tab", Key::Tab},            {"Enter", Key::Enter},
      {"Escape", Key::Escape},     {"Backspace", Key::Backspace}, {"ArrowUp", Key::Up},
      {"ArrowDown", Key::Down},    {"ArrowLeft", Key::Left},     {"ArrowRight", Key::Right},
  };
  for (auto& m : map)
    if (m.s == c) return m.k;
  return Key::Unknown;
}

void InputQueue::drainInto(Input& in) {
  std::vector<InputEvent> events;
  {
    std::lock_guard<std::mutex> l(m_);
    events.swap(q_);
  }
  for (auto& e : events) {
    switch (e.type) {
      case InputEvent::KeyDown: in.key(e.key, true); break;
      case InputEvent::KeyUp: in.key(e.key, false); break;
      case InputEvent::MouseMove: in.mouseDX += e.x; in.mouseDY += e.y; break;
      case InputEvent::MouseDown: in.mouseButton(e.button, true); break;
      case InputEvent::MouseUp: in.mouseButton(e.button, false); break;
      case InputEvent::Wheel: in.wheel += e.y; break;
      case InputEvent::PadAxis:
        if (e.button >= 0 && e.button < int(GamepadAxis::Count)) { in.axis[e.button] = e.x; in.gamepad = true; }
        break;
      case InputEvent::PadButton:
        in.gamepadButton(GamepadButton(e.button), e.x > 0.5f);
        in.gamepad = true;
        break;
    }
  }
}

}  // namespace vx
