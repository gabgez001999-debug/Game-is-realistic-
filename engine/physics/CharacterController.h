#pragma once
#include "physics/PhysicsWorld.h"

namespace vx {

// Kinematic capsule controller. Collide-and-slide against static boxes, steep terrain
// and dynamic bodies, ground snapping, smooth step-up (visual offset decays, no popping).
class CharacterController {
 public:
  vec3 pos{0.f};   // feet position
  vec3 prevPos{0.f};
  vec3 vel{0.f};
  float radius = 0.3f;
  float height = 1.8f;
  float stepHeight = 0.45f;
  float maxSlopeCos = 0.64f;  // ~50 degrees
  bool grounded = false;
  vec3 groundNormal{0, 1, 0};
  SurfaceType groundSurface = SurfaceType::Concrete;
  RigidBody* groundBody = nullptr;
  float visualYOffset = 0.f;  // added to the render position, decays to 0
  float airTime = 0.f;
  float landingSpeed = 0.f;   // vertical speed at the last landing (for fall damage / ragdoll)

  // Highest relative speed of a body that hit us this step (cars).
  float hitSpeed = 0.f;
  vec3 hitVelocity{0.f};
  RigidBody* hitBody = nullptr;

  void move(const vec3& desiredVel, bool jump, float dt, const StaticWorld& statics, PhysicsWorld& physics,
            const RigidBody* ignore = nullptr);
  // Smoothly pushes the capsule out of any overlap without moving it (used after transitions).
  void resolveOverlaps(const StaticWorld& statics, PhysicsWorld& physics, const RigidBody* ignore = nullptr);
  bool fitsAt(const vec3& feet, const StaticWorld& statics, PhysicsWorld& physics, const RigidBody* ignore = nullptr) const;
  vec3 renderPos(float alpha) const { return glm::mix(prevPos, pos, alpha) + vec3(0.f, visualYOffset, 0.f); }

 private:
  bool probeGround(const StaticWorld& statics, PhysicsWorld& physics, const RigidBody* ignore, float& outY, vec3& outN,
                   SurfaceType& outS, RigidBody*& outBody) const;
};

}  // namespace vx
