#pragma once
#include "physics/PhysicsWorld.h"

namespace vx {

enum Joint : int {
  J_PELVIS, J_SPINE, J_NECK, J_HEAD,
  J_LSHOULDER, J_LELBOW, J_LHAND,
  J_RSHOULDER, J_RELBOW, J_RHAND,
  J_LHIP, J_LKNEE, J_LFOOT,
  J_RHIP, J_RKNEE, J_RFOOT,
  J_COUNT
};

// Position-based (Verlet) ragdoll: particles at joints, distance constraints for bones and
// torso bracing, min-distance limits against folding, sphere collision with the world.
class Ragdoll {
 public:
  vec3 p[J_COUNT];
  vec3 prev[J_COUNT];
  float radius[J_COUNT];
  bool active = false;
  float restTimer = 0.f;
  float impactAccum = 0.f;  // strongest impact this step (for sounds)

  void start(const vec3 joints[J_COUNT], const vec3& velocity, const vec3& angular, float dt);
  void step(float dt, const StaticWorld& statics, PhysicsWorld& physics);
  vec3 center() const { return p[J_PELVIS]; }
  float speed(float dt) const;
  bool atRest() const { return restTimer > 1.4f; }
  bool faceDown() const;

 private:
  struct Stick { int a, b; float len; float minLen; bool equality; };
  std::vector<Stick> sticks_;
  float dt_ = 1.f / 120.f;
};

}  // namespace vx
