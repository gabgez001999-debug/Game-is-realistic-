#include "physics/Ragdoll.h"

namespace vx {

void Ragdoll::start(const vec3 joints[J_COUNT], const vec3& velocity, const vec3& angular, float dt) {
  dt_ = dt;
  vec3 c = joints[J_PELVIS];
  for (int i = 0; i < J_COUNT; ++i) {
    p[i] = joints[i];
    vec3 v = velocity + glm::cross(angular, joints[i] - c);
    prev[i] = p[i] - v * dt;
    radius[i] = 0.09f;
  }
  radius[J_HEAD] = 0.12f;
  radius[J_PELVIS] = 0.14f;
  radius[J_SPINE] = 0.15f;
  radius[J_LFOOT] = radius[J_RFOOT] = 0.07f;
  radius[J_LHAND] = radius[J_RHAND] = 0.06f;

  sticks_.clear();
  auto stick = [&](int a, int b, bool eq = true, float minFrac = 0.f) {
    float l = glm::length(joints[a] - joints[b]);
    sticks_.push_back({a, b, l, l * minFrac, eq});
  };
  // Bones.
  stick(J_PELVIS, J_SPINE); stick(J_SPINE, J_NECK); stick(J_NECK, J_HEAD);
  stick(J_NECK, J_LSHOULDER); stick(J_LSHOULDER, J_LELBOW); stick(J_LELBOW, J_LHAND);
  stick(J_NECK, J_RSHOULDER); stick(J_RSHOULDER, J_RELBOW); stick(J_RELBOW, J_RHAND);
  stick(J_PELVIS, J_LHIP); stick(J_LHIP, J_LKNEE); stick(J_LKNEE, J_LFOOT);
  stick(J_PELVIS, J_RHIP); stick(J_RHIP, J_RKNEE); stick(J_RKNEE, J_RFOOT);
  // Torso bracing (keeps chest and hips rigid).
  stick(J_LSHOULDER, J_RSHOULDER); stick(J_LHIP, J_RHIP);
  stick(J_LSHOULDER, J_SPINE); stick(J_RSHOULDER, J_SPINE);
  stick(J_LHIP, J_SPINE); stick(J_RHIP, J_SPINE);
  stick(J_LSHOULDER, J_PELVIS); stick(J_RSHOULDER, J_PELVIS);
  stick(J_LSHOULDER, J_LHIP); stick(J_RSHOULDER, J_RHIP);
  stick(J_HEAD, J_LSHOULDER, false, 0.85f); stick(J_HEAD, J_RSHOULDER, false, 0.85f);
  stick(J_HEAD, J_SPINE, false, 0.8f);
  // Joint limits approximated by minimum distances (no full folding).
  stick(J_LHIP, J_LFOOT, false, 0.45f); stick(J_RHIP, J_RFOOT, false, 0.45f);
  stick(J_LSHOULDER, J_LHAND, false, 0.3f); stick(J_RSHOULDER, J_RHAND, false, 0.3f);
  stick(J_PELVIS, J_LKNEE, false, 0.7f); stick(J_PELVIS, J_RKNEE, false, 0.7f);
  stick(J_LFOOT, J_RFOOT, false, 0.0f);
  active = true;
  restTimer = 0.f;
}

float Ragdoll::speed(float dt) const {
  float s = 0.f;
  for (int i = 0; i < J_COUNT; ++i) s = std::max(s, glm::length(p[i] - prev[i]) / dt);
  return s;
}

bool Ragdoll::faceDown() const {
  vec3 across = p[J_RSHOULDER] - p[J_LSHOULDER];
  vec3 up = p[J_NECK] - p[J_PELVIS];
  vec3 front = glm::cross(across, up);  // chest normal
  return front.y < 0.f;
}

void Ragdoll::step(float dt, const StaticWorld& statics, PhysicsWorld& physics) {
  if (!active) return;
  impactAccum = 0.f;
  const vec3 g(0.f, -9.81f, 0.f);
  for (int i = 0; i < J_COUNT; ++i) {
    vec3 v = (p[i] - prev[i]) * 0.995f;
    prev[i] = p[i];
    p[i] += v + g * dt * dt;
  }
  std::vector<ContactInfo> cs;
  std::vector<RigidBody*> nearby;
  physics.queryBodies(p[J_PELVIS], 2.f, nearby);
  for (int iter = 0; iter < 12; ++iter) {
    for (auto& s : sticks_) {
      vec3 d = p[s.b] - p[s.a];
      float l = glm::length(d);
      if (l < 1e-6f) continue;
      float target = s.len;
      if (!s.equality) {
        if (l >= s.minLen) continue;
        target = s.minLen;
      }
      vec3 corr = d * ((l - target) / l * 0.5f);
      p[s.a] += corr;
      p[s.b] -= corr;
    }
    // Collisions.
    for (int i = 0; i < J_COUNT; ++i) {
      cs.clear();
      statics.sphereContacts(p[i], radius[i], cs);
      for (auto& c : cs) {
        p[i] += c.normal * c.depth;
        // Friction: kill tangential motion proportionally.
        vec3 v = p[i] - prev[i];
        vec3 vn = c.normal * glm::dot(v, c.normal);
        vec3 vt = v - vn;
        prev[i] = p[i] - (vt * 0.55f + vn * -0.1f);
        impactAccum = std::max(impactAccum, glm::length(vn) / dt);
      }
      for (RigidBody* b : nearby) {
        ContactInfo c;
        if (!sphereVsOBB(p[i], radius[i], b->worldBox(), c)) continue;
        p[i] += c.normal * c.depth;
        vec3 bv = b->pointVelocity(c.point) * dt;
        vec3 v = p[i] - prev[i];
        vec3 rel = v - bv;
        vec3 vn = c.normal * glm::dot(rel, c.normal);
        vec3 vt = rel - vn;
        prev[i] = p[i] - (bv + vt * 0.7f);
        impactAccum = std::max(impactAccum, glm::length(vn) / dt);
      }
    }
  }
  if (speed(dt) < 0.35f) restTimer += dt;
  else restTimer = 0.f;
}

}  // namespace vx
