#pragma once
#include "City.h"
#include "gfx/Hud.h"
#include "gfx/Renderer.h"
#include "platform/Platform.h"

namespace game {

struct GameOptions {
  float timeOfDay = 16.5f;
  bool showStats = true;
  std::string streamInfo;  // shown in the HUD when streaming
};

// Top-level game: owns world, physics, rendering and the camera.
// Current state: free exploration camera through the city (player character,
// vehicles and missions are layered on top of this loop).
class Game {
 public:
  bool init(Platform& platform, const GameOptions& opt);
  void update(const Input& in, float dt);
  void render(unsigned targetFbo, int w, int h);
  void setStreamInfo(const std::string& s) { opt_.streamInfo = s; }
  Camera& camera() { return cam_; }
  Environment& environment() { return env_; }
  Renderer& renderer() { return renderer_; }
  void setCameraPose(const vec3& pos, float yaw, float pitch) { camPos_ = pos; yaw_ = yaw; pitch_ = pitch; }

 private:
  void updateFreeCamera(const Input& in, float dt);
  void drawHud(int w, int h);

  Platform* platform_ = nullptr;
  GameOptions opt_;
  Renderer renderer_;
  Hud hud_;
  PhysicsWorld physics_;
  City city_;
  Environment env_;
  Camera cam_;
  vec3 camPos_{9.f, 3.8f, 50.f};
  vec3 camVel_{0.f};
  float yaw_ = 0.f, pitch_ = -0.05f;
  float time_ = 0.f;
  float fps_ = 60.f;
  float frameDt_ = 1.f / 60.f;
};

}  // namespace game
