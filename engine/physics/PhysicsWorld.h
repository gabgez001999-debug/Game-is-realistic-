#pragma once
#include <unordered_map>

#include "physics/StaticWorld.h"

namespace vx {

struct HullSphere {
  vec3 local{0.f};
  float r = 0.2f;
};

enum class BodyKind : uint8_t { Generic, Vehicle, Prop, Debris };

struct RigidBody {
  uint32_t id = 0;
  BodyKind kind = BodyKind::Generic;
  void* user = nullptr;

  float mass = 1.f, invMass = 1.f;
  vec3 invInertiaLocal{1.f};
  vec3 pos{0.f};
  quat rot{1.f, 0.f, 0.f, 0.f};
  vec3 vel{0.f}, angVel{0.f};
  vec3 force{0.f}, torque{0.f};
  vec3 prevPos{0.f};
  quat prevRot{1.f, 0.f, 0.f, 0.f};

  std::vector<HullSphere> hull;  // collision against the world and other bodies' boxes
  vec3 boxCenter{0.f};           // local OBB proxy that others collide against
  vec3 boxHalf{0.5f};

  float friction = 0.7f;
  float restitution = 0.1f;
  float linDamping = 0.01f;
  float angDamping = 0.05f;
  float gravityScale = 1.f;
  SurfaceType surface = SurfaceType::Metal;

  bool pinned = false;        // behaves as static until hit with more than breakImpulse
  float breakImpulse = 0.f;
  bool sleeping = false;
  bool canSleep = true;
  float sleepTimer = 0.f;
  bool enabled = true;
  int solverIdx = -1;

  // Derived per step.
  mat3 R{1.f};
  mat3 invInertiaWorld{1.f};

  void setBoxMass(float m, const vec3& fullSize);
  void updateDerived();
  bool isDynamic() const { return enabled && !pinned && invMass > 0.f; }
  OBB worldBox() const;
  vec3 toWorld(const vec3& local) const { return pos + rot * local; }
  vec3 toLocal(const vec3& world) const { return glm::inverse(rot) * (world - pos); }
  vec3 dirToWorld(const vec3& d) const { return rot * d; }
  vec3 pointVelocity(const vec3& wp) const { return vel + glm::cross(angVel, wp - pos); }
  void applyImpulse(const vec3& j, const vec3& wp);
  void applyForce(const vec3& f, const vec3& wp) { force += f; torque += glm::cross(wp - pos, f); }
  void wake() { sleeping = false; sleepTimer = 0.f; }
  float boundingRadius() const { return glm::length(boxHalf) + glm::length(boxCenter) + 0.3f; }
  // Render-time interpolation between the previous and current fixed step.
  mat4 interpolatedMatrix(float alpha) const;
};

struct ContactEvent {
  RigidBody* a = nullptr;   // always a dynamic body
  RigidBody* b = nullptr;   // other body, or null for static world
  vec3 point{0.f};
  vec3 normal{0.f};         // pushes a away from the obstacle
  float impulse = 0.f;      // summed normal impulse (N*s)
  float normalSpeed = 0.f;  // approach speed along the normal before solving (m/s)
  float tangentSpeed = 0.f; // sliding speed (scraping)
  SurfaceType otherSurface = SurfaceType::Concrete;
};

class PhysicsWorld {
 public:
  StaticWorld* statics = nullptr;
  vec3 gravity{0.f, -9.81f, 0.f};
  int velocityIterations = 10;
  int positionIterations = 4;

  void add(RigidBody* b);
  void remove(RigidBody* b);
  void step(float dt);
  void invalidate() { gridDirty_ = true; }  // call after waking/unpinning bodies from game code
  const std::vector<RigidBody*>& bodies() const { return bodies_; }
  std::vector<ContactEvent> events;  // appended every step, consumer clears

  bool raycastBodies(const vec3& o, const vec3& d, float tmax, RayHit& hit, const RigidBody* ignore = nullptr) const;
  void queryBodies(const vec3& c, float radius, std::vector<RigidBody*>& out) const;

 private:
  struct Contact {
    RigidBody* a;
    RigidBody* b;
    vec3 p, n;
    float depth;
    vec3 rA, rB, t1, t2;
    float mN, mT1, mT2;
    float jn = 0.f, jt1 = 0.f, jt2 = 0.f, jb = 0.f;
    float restBias = 0.f;
    float friction = 0.7f;
    float approach = 0.f;
    float slide = 0.f;
    SurfaceType surf;
  };
  void collide(float dt);
  void addContact(RigidBody* a, RigidBody* b, const vec3& p, const vec3& n, float depth, SurfaceType surf);
  float effMass(RigidBody* a, RigidBody* b, const vec3& rA, const vec3& rB, const vec3& n) const;

  // Broadphase: resting/pinned bodies live in a uniform grid (rebuilt only when something
  // wakes, sleeps or breaks loose); moving bodies are tested against it and each other.
  void rebuildGrid();
  template <class F> void forGridCells(const vec3& mn, const vec3& mx, F f) const;
  bool active(const RigidBody* b) const { return b->isDynamic() && !b->sleeping; }
  float gridCell_ = 12.f;
  std::unordered_map<int64_t, std::vector<RigidBody*>> grid_;
  std::vector<RigidBody*> active_;
  bool gridDirty_ = true;
  mutable std::vector<uint32_t> stamp_;
  mutable uint32_t stampId_ = 1;

  std::vector<RigidBody*> bodies_;
  std::vector<Contact> contacts_;
  std::vector<vec3> biasV_, biasW_;
  uint32_t nextId_ = 1;
};

}  // namespace vx
