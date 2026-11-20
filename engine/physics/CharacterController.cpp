#include "physics/CharacterController.h"

namespace vx {

static const float kSphereHeights[3] = {0.0f, 0.0f, 0.0f};

bool CharacterController::probeGround(const StaticWorld& statics, PhysicsWorld& physics, const RigidBody* ignore,
                                      float& outY, vec3& outN, SurfaceType& outS, RigidBody*& outBody) const {
  const vec2 offs[5] = {{0, 0}, {0.7f, 0}, {-0.7f, 0}, {0, 0.7f}, {0, -0.7f}};
  bool found = false;
  outY = -1e9f;
  outBody = nullptr;
  float top = pos.y + stepHeight + 0.05f;
  float len = stepHeight + 0.05f + 0.6f;
  for (auto& o : offs) {
    vec3 start(pos.x + o.x * radius, top, pos.z + o.y * radius);
    RayHit h;
    bool hit = statics.raycast(start, vec3(0, -1, 0), len, h);
    RayHit hb;
    if (physics.raycastBodies(start, vec3(0, -1, 0), hit ? h.t : len, hb, ignore)) { h = hb; hit = true; }
    if (!hit) continue;
    // Only walkable surfaces count; the center ray decides the normal.
    if (h.normal.y < maxSlopeCos) continue;
    if (h.point.y > outY) {
      outY = h.point.y;
      outN = h.normal;
      outS = h.surface;
      outBody = h.body;
      found = true;
    }
  }
  return found;
}

void CharacterController::move(const vec3& desiredVel, bool jump, float dt, const StaticWorld& statics, PhysicsWorld& physics,
                               const RigidBody* ignore) {
  (void)kSphereHeights;
  prevPos = pos;
  hitSpeed = 0.f;
  hitBody = nullptr;
  visualYOffset = damp(visualYOffset, 0.f, 14.f, dt);

  // Horizontal acceleration towards the desired velocity.
  vec2 cur(vel.x, vel.z), want(desiredVel.x, desiredVel.z);
  float accel = grounded ? 14.f : 2.5f;
  vec2 dv = want - cur;
  float dl = glm::length(dv);
  float maxDv = accel * dt;
  if (dl > maxDv) dv *= maxDv / dl;
  cur += dv;
  vel.x = cur.x;
  vel.z = cur.y;

  if (jump && grounded) {
    vel.y = 4.4f;
    grounded = false;
  }
  if (!grounded) {
    vel.y -= 9.81f * dt;
    airTime += dt;
  } else {
    vel.y = 0.f;
    airTime = 0.f;
  }
  // Ride moving platforms (standing on a car roof).
  if (grounded && groundBody) pos += groundBody->pointVelocity(pos) * dt;

  pos += vel * dt;

  // Collide & slide: spheres stacked above the step height.
  const float hs[3] = {stepHeight + radius, (stepHeight + radius + height - radius) * 0.5f, height - radius};
  std::vector<ContactInfo> cs;
  std::vector<RigidBody*> nearby;
  physics.queryBodies(pos + vec3(0, height * 0.5f, 0), height, nearby);
  for (int iter = 0; iter < 4; ++iter) {
    bool any = false;
    for (float h : hs) {
      vec3 p = pos + vec3(0.f, h, 0.f);
      cs.clear();
      statics.sphereContacts(p, radius, cs);
      for (auto& c : cs) {
        // Walkable terrain is handled by the ground probe, everything else blocks.
        bool isTerrainFloor = c.normal.y >= maxSlopeCos;
        if (isTerrainFloor) continue;
        vec3 n = c.normal;
        if (n.y < -0.7f) {  // ceiling
          pos.y -= c.depth;
          if (vel.y > 0.f) vel.y = 0.f;
          any = true;
          continue;
        }
        n.y = 0.f;
        float l = glm::length(n);
        if (l < 1e-4f) continue;
        n /= l;
        pos += n * c.depth;
        float vn = glm::dot(vel, n);
        if (vn < 0.f) vel -= n * vn;
        any = true;
      }
      for (RigidBody* b : nearby) {
        if (b == ignore) continue;
        ContactInfo c;
        if (!sphereVsOBB(p, radius, b->worldBox(), c)) continue;
        if (c.normal.y >= maxSlopeCos) continue;  // standing on top
        vec3 n = c.normal;
        n.y = 0.f;
        float l = glm::length(n);
        if (l < 1e-4f) continue;
        n /= l;
        // Was it the body ramming us?
        vec3 bv = b->pointVelocity(c.point);
        float rel = glm::dot(bv - vel, n);
        if (rel > hitSpeed && b->isDynamic()) {
          hitSpeed = rel;
          hitVelocity = bv;
          hitBody = b;
        }
        pos += n * c.depth;
        float vn = glm::dot(vel - bv, n);
        if (vn < 0.f) vel -= n * vn;
        // Light shove onto the body (pushing a prop or a parked car slightly).
        if (b->isDynamic() && b->mass < 400.f) b->applyImpulse(-n * 8.f * dt * 60.f * 0.05f, c.point);
        any = true;
      }
    }
    if (!any) break;
  }

  // Ground.
  float gy;
  vec3 gn;
  SurfaceType gs;
  RigidBody* gb;
  bool hasGround = probeGround(statics, physics, ignore, gy, gn, gs, gb);
  float snap = grounded ? 0.35f : 0.04f;
  if (hasGround && vel.y <= 0.5f && pos.y - gy <= snap && gy - pos.y <= stepHeight + 0.02f) {
    float delta = gy - pos.y;
    if (!grounded) landingSpeed = -vel.y;
    if (delta > 0.02f) visualYOffset -= delta;  // step up: physics snaps, the visual follows smoothly
    else if (delta < -0.02f && grounded) visualYOffset -= delta;  // step down smoothing
    visualYOffset = clampf(visualYOffset, -0.5f, 0.5f);
    pos.y = gy;
    vel.y = 0.f;
    grounded = true;
    groundNormal = gn;
    groundSurface = gs;
    groundBody = gb;
  } else {
    grounded = false;
    groundBody = nullptr;
    // Never fall through the terrain.
    if (!statics.terrain.h.empty() && statics.terrain.contains(pos.x, pos.z)) {
      float th = statics.terrain.height(pos.x, pos.z);
      if (pos.y < th) {
        pos.y = th;
        if (vel.y < 0.f) { landingSpeed = -vel.y; vel.y = 0.f; }
        grounded = true;
      }
    }
  }
}

bool CharacterController::fitsAt(const vec3& feet, const StaticWorld& statics, PhysicsWorld& physics, const RigidBody* ignore) const {
  const float hs[3] = {stepHeight + radius, (stepHeight + height) * 0.5f, height - radius};
  std::vector<ContactInfo> cs;
  std::vector<RigidBody*> nearby;
  physics.queryBodies(feet + vec3(0, height * 0.5f, 0), height, nearby);
  for (float h : hs) {
    vec3 p = feet + vec3(0.f, h, 0.f);
    cs.clear();
    statics.sphereContacts(p, radius * 0.95f, cs);
    for (auto& c : cs)
      if (c.normal.y < maxSlopeCos && c.depth > 0.01f) return false;
    for (RigidBody* b : nearby) {
      if (b == ignore) continue;
      ContactInfo c;
      if (sphereVsOBB(p, radius * 0.95f, b->worldBox(), c) && c.depth > 0.01f) return false;
    }
  }
  return true;
}

void CharacterController::resolveOverlaps(const StaticWorld& statics, PhysicsWorld& physics, const RigidBody* ignore) {
  vec3 saved = vel;
  vel = vec3(0.f);
  bool g = grounded;
  move(vec3(0.f), false, 0.f, statics, physics, ignore);
  grounded = g || grounded;
  vel = saved;
  prevPos = pos;
}

}  // namespace vx
