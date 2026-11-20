#include "Game.h"

#include <cstring>

namespace game {

bool Game::init(Platform& platform, const GameOptions& opt) {
  platform_ = &platform;
  opt_ = opt;
  if (!renderer_.init(platform.width(), platform.height())) return false;
  if (!hud_.init()) return false;
  city_.generate(1337, physics_);
  physics_.statics = &city_.statics;
  renderer_.setCity(city_.shaderParams());
  renderer_.setHeightMap(city_.heightMap, city_.heightMapRes, city_.heightMapRes, city_.heightMapOrigin, city_.heightMapSize);
  renderer_.setWater(true, 0.f);
  hud_.cityInfo = vec4(city_.L.pitch, city_.L.roadHalf, city_.L.extent(), city_.L.extent() + 60.f);
  env_.timeOfDay = opt.timeOfDay;
  env_.update(0.f);
  cam_.setPerspective(glm::radians(58.f), float(platform.width()) / float(platform.height()));
  return true;
}

void Game::updateFreeCamera(const Input& in, float dt) {
  // Look: mouse or right stick.
  float lookX = in.mouseDX * 0.0025f + in.ax(GamepadAxis::RightX) * 2.4f * dt;
  float lookY = in.mouseDY * 0.0025f + in.ax(GamepadAxis::RightY) * 2.0f * dt;
  yaw_ -= lookX;
  pitch_ = clampf(pitch_ - lookY, -1.45f, 1.45f);

  vec3 fwd = forwardFromYaw(yaw_), right = rightFromYaw(yaw_);
  vec3 move(0.f);
  if (in.isDown(Key::W) || in.isDown(Key::Up)) move += fwd;
  if (in.isDown(Key::S) || in.isDown(Key::Down)) move -= fwd;
  if (in.isDown(Key::D) || in.isDown(Key::Right)) move += right;
  if (in.isDown(Key::A) || in.isDown(Key::Left)) move -= right;
  move += fwd * -in.ax(GamepadAxis::LeftY) + right * in.ax(GamepadAxis::LeftX);
  float up = 0.f;
  if (in.isDown(Key::Space) || in.btn(GamepadButton::RB)) up += 1.f;
  if (in.isDown(Key::LCtrl) || in.btn(GamepadButton::LB)) up -= 1.f;
  up += in.ax(GamepadAxis::TriggerR) - in.ax(GamepadAxis::TriggerL);
  bool fast = in.isDown(Key::LShift) || in.btn(GamepadButton::LStick);
  float speed = fast ? 38.f : 9.f;
  vec3 target = (glm::length(move) > 1.f ? glm::normalize(move) : move) * speed + vec3(0, up * speed * 0.6f, 0);
  camVel_ = damp(camVel_, target, 5.f, dt);
  camPos_ += camVel_ * dt;

  // The camera never passes through geometry: push it out of buildings and the ground.
  std::vector<ContactInfo> cs;
  for (int it = 0; it < 3; ++it) {
    cs.clear();
    city_.statics.sphereContacts(camPos_, 0.35f, cs);
    for (auto& c : cs) {
      camPos_ += c.normal * c.depth;
      float vn = glm::dot(camVel_, c.normal);
      if (vn < 0.f) camVel_ -= c.normal * vn;
    }
  }
  float ground = city_.statics.groundHeight(camPos_.x, camPos_.z, camPos_.y + 0.5f);
  if (camPos_.y < ground + 1.0f) camPos_.y = damp(camPos_.y, ground + 1.0f, 20.f, dt);
}

void Game::update(const Input& in, float dt) {
  frameDt_ = dt;
  time_ += dt;
  fps_ = damp(fps_, dt > 0.f ? 1.f / dt : 60.f, 3.f, dt);
  if (in.wasPressed(Key::T) || in.btnPressed(GamepadButton::DUp)) env_.timeOfDay = std::fmod(env_.timeOfDay + 1.f, 24.f);
  if (in.wasPressed(Key::G) || in.btnPressed(GamepadButton::DDown)) env_.timeOfDay = std::fmod(env_.timeOfDay + 23.f, 24.f);
  if (in.wasPressed(Key::R) || in.btnPressed(GamepadButton::DRight)) env_.rain = env_.rain > 0.f ? 0.f : 1.f;
  if (in.wasPressed(Key::F5)) renderer_.reloadShaders();
  env_.update(dt);
  city_.setTime(time_);
  city_.updateProps(dt);
  updateFreeCamera(in, dt);
  vec3 dir(std::cos(pitch_) * forwardFromYaw(yaw_).x, std::sin(pitch_), std::cos(pitch_) * forwardFromYaw(yaw_).z);
  cam_.lookAt(camPos_, camPos_ + dir);
}

void Game::drawHud(int w, int h) {
  hud_.begin(w, h);
  float s = hud_.uiScale;
  hud_.text(28.f * s, 26.f * s, 30.f * s, "GAME IS REALISTIC", vec4(1.f, 1.f, 1.f, 0.85f));
  if (opt_.showStats) {
    char buf[160];
    int hh = int(env_.timeOfDay), mm = int((env_.timeOfDay - float(hh)) * 60.f);
    std::snprintf(buf, sizeof(buf), "%02d:%02d   %.0f FPS", hh, mm, double(fps_));
    hud_.text(28.f * s, 62.f * s, 18.f * s, buf, vec4(1.f, 1.f, 1.f, 0.7f));
    if (!opt_.streamInfo.empty()) hud_.text(28.f * s, 86.f * s, 15.f * s, opt_.streamInfo, vec4(0.7f, 0.9f, 1.f, 0.7f));
  }
  bool ps = platform_->padType() == PadType::PS5 || platform_->padType() == PadType::PS4;
  std::string hint = ps ? "L-STICK BEWEGEN  R-STICK SCHAUEN  R2/L2 HOCH/RUNTER  STEUERKREUZ HOCH: ZEIT +1H"
                        : "WASD BEWEGEN  MAUS SCHAUEN  SHIFT SCHNELL  LEER/STRG HOCH/RUNTER  T ZEIT  R REGEN";
  hud_.text(float(w) * 0.5f, float(h) - 40.f * s, 15.f * s, hint, vec4(1.f, 1.f, 1.f, 0.6f), 1);
}

void Game::render(unsigned targetFbo, int w, int h) {
  if (w != renderer_.width() || h != renderer_.height()) {
    renderer_.resize(w, h);
    cam_.setPerspective(cam_.fovY, float(w) / float(h));
  }
  renderer_.beginFrame(cam_, env_, time_, frameDt_);
  city_.submit(renderer_, cam_.pos, env_.night, 1.f);
  drawHud(w, h);
  renderer_.render(targetFbo, w, h, &hud_);
}

}  // namespace game
