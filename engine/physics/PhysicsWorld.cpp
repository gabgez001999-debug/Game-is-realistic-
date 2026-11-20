#include "physics/PhysicsWorld.h"

namespace vx {

void RigidBody::setBoxMass(float m, const vec3& s) {
  mass = m;
  invMass = m > 0.f ? 1.f / m : 0.f;
  vec3 I(m / 12.f * (s.y * s.y + s.z * s.z), m / 12.f * (s.x * s.x + s.z * s.z), m / 12.f * (s.x * s.x + s.y * s.y));
  invInertiaLocal = vec3(1.f / I.x, 1.f / I.y, 1.f / I.z);
}

void RigidBody::updateDerived() {
  R = glm::mat3_cast(rot);
  mat3 D(0.f);
  D[0][0] = invInertiaLocal.x; D[1][1] = invInertiaLocal.y; D[2][2] = invInertiaLocal.z;
  invInertiaWorld = R * D * glm::transpose(R);
}

OBB RigidBody::worldBox() const {
  OBB o;
  o.R = glm::mat3_cast(rot);
  o.c = pos + o.R * boxCenter;
  o.h = boxHalf;
  return o;
}

void RigidBody::applyImpulse(const vec3& j, const vec3& wp) {
  if (!isDynamic()) return;
  vel += j * invMass;
  angVel += invInertiaWorld * glm::cross(wp - pos, j);
}

mat4 RigidBody::interpolatedMatrix(float alpha) const {
  vec3 p = glm::mix(prevPos, pos, alpha);
  quat q = glm::slerp(prevRot, rot, alpha);
  mat4 m = glm::mat4_cast(q);
  m[3] = vec4(p, 1.f);
  return m;
}

void PhysicsWorld::add(RigidBody* b) {
  b->id = nextId_++;
  b->solverIdx = int(bodies_.size());
  gridDirty_ = true;
  b->prevPos = b->pos;
  b->prevRot = b->rot;
  b->updateDerived();
  bodies_.push_back(b);
}

void PhysicsWorld::remove(RigidBody* b) {
  bodies_.erase(std::remove(bodies_.begin(), bodies_.end(), b), bodies_.end());
  for (size_t i = 0; i < bodies_.size(); ++i) bodies_[i]->solverIdx = int(i);
  active_.erase(std::remove(active_.begin(), active_.end(), b), active_.end());
  gridDirty_ = true;
  events.erase(std::remove_if(events.begin(), events.end(), [b](const ContactEvent& e) { return e.a == b || e.b == b; }),
               events.end());
}

static inline int64_t cellKey(int x, int z) { return (int64_t(x) << 32) ^ int64_t(uint32_t(z)); }

void PhysicsWorld::rebuildGrid() {
  grid_.clear();
  for (RigidBody* b : bodies_) {
    if (!b->enabled || active(b)) continue;
    float r = b->boundingRadius();
    int x0 = int(std::floor((b->pos.x - r) / gridCell_)), x1 = int(std::floor((b->pos.x + r) / gridCell_));
    int z0 = int(std::floor((b->pos.z - r) / gridCell_)), z1 = int(std::floor((b->pos.z + r) / gridCell_));
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) grid_[cellKey(x, z)].push_back(b);
  }
  gridDirty_ = false;
}

template <class F>
void PhysicsWorld::forGridCells(const vec3& mn, const vec3& mx, F f) const {
  if (stamp_.size() < bodies_.size() + 1) stamp_.resize(bodies_.size() + 1, 0);
  if (++stampId_ == 0) { std::fill(stamp_.begin(), stamp_.end(), 0); stampId_ = 1; }
  int x0 = int(std::floor(mn.x / gridCell_)), x1 = int(std::floor(mx.x / gridCell_));
  int z0 = int(std::floor(mn.z / gridCell_)), z1 = int(std::floor(mx.z / gridCell_));
  for (int z = z0; z <= z1; ++z)
    for (int x = x0; x <= x1; ++x) {
      auto it = grid_.find(cellKey(x, z));
      if (it == grid_.end()) continue;
      for (RigidBody* b : it->second) {
        if (b->solverIdx < 0 || size_t(b->solverIdx) >= stamp_.size()) continue;
        if (stamp_[size_t(b->solverIdx)] == stampId_) continue;
        stamp_[size_t(b->solverIdx)] = stampId_;
        f(b);
      }
    }
}

float PhysicsWorld::effMass(RigidBody* a, RigidBody* b, const vec3& rA, const vec3& rB, const vec3& n) const {
  float k = a->invMass + glm::dot(glm::cross(a->invInertiaWorld * glm::cross(rA, n), rA), n);
  if (b && b->isDynamic()) k += b->invMass + glm::dot(glm::cross(b->invInertiaWorld * glm::cross(rB, n), rB), n);
  return k > 1e-9f ? 1.f / k : 0.f;
}

void PhysicsWorld::addContact(RigidBody* a, RigidBody* b, const vec3& p, const vec3& n, float depth, SurfaceType surf) {
  Contact c{};
  c.a = a;
  c.b = (b && b->isDynamic()) ? b : nullptr;  // pinned/static obstacles act as infinite mass
  c.p = p;
  c.n = n;
  c.depth = depth;
  c.surf = surf;
  c.friction = std::sqrt(a->friction * (b ? b->friction : 0.8f));
  contacts_.push_back(c);
}

void PhysicsWorld::collide(float dt) {
  contacts_.clear();
  std::vector<ContactInfo> tmp;
  std::vector<SurfaceType> surfs;
  std::vector<uint32_t> cand;
  // Bodies vs static world.
  for (RigidBody* a : bodies_) {
    if (!a->isDynamic() || a->sleeping || a->hull.empty() || !statics) continue;
    AABB box;
    for (auto& s : a->hull) {
      vec3 w = a->pos + a->R * s.local;
      box.expand(w - vec3(s.r));
      box.expand(w + vec3(s.r));
    }
    statics->query(box.min - vec3(0.05f), box.max + vec3(0.05f), cand);
    bool nearTerrain = !statics->terrain.h.empty();
    for (auto& s : a->hull) {
      vec3 w = a->pos + a->R * s.local;
      for (uint32_t i : cand) {
        ContactInfo c;
        if (sphereVsOBB(w, s.r, statics->boxes[i].box, c)) addContact(a, nullptr, c.point, c.normal, c.depth, statics->boxes[i].surface);
      }
      if (nearTerrain && statics->terrain.contains(w.x, w.z)) {
        float hgt = statics->terrain.height(w.x, w.z);
        if (w.y - s.r < hgt + 0.5f) {
          vec3 n = statics->terrain.normal(w.x, w.z);
          float d = (w.y - hgt) * n.y;
          if (d < s.r) addContact(a, nullptr, w - n * d, n, s.r - d, statics->terrainSurface(w.x, w.z));
        }
      }
    }
  }
  // Body pairs: moving vs moving (brute force, few) and moving vs resting (grid).
  auto test = [&](RigidBody* S, RigidBody* O) {
    if (!S->isDynamic() || S->hull.empty()) return;
    OBB ob = O->worldBox();
    for (auto& s : S->hull) {
      vec3 w = S->pos + S->R * s.local;
      ContactInfo c;
      if (!sphereVsOBB(w, s.r, ob, c)) continue;
      // Wake sleepers and break loose pinned props on hard hits.
      if (O->sleeping) { O->wake(); gridDirty_ = true; }
      if (O->pinned) {
        float approach = -glm::dot(S->pointVelocity(c.point), c.normal);
        if (approach * S->mass * 0.5f > O->breakImpulse) {
          O->pinned = false;
          O->wake();
          O->updateDerived();
          gridDirty_ = true;
        }
      }
      if (S->sleeping) S->wake();
      addContact(S, O, c.point, c.normal, c.depth, O->surface);
    }
  };
  for (size_t i = 0; i < active_.size(); ++i) {
    RigidBody* A = active_[i];
    for (size_t j = i + 1; j < active_.size(); ++j) {
      RigidBody* B = active_[j];
      float rr = A->boundingRadius() + B->boundingRadius();
      if (glm::length2(A->pos - B->pos) > rr * rr) continue;
      test(A, B);
      test(B, A);
    }
    float r = A->boundingRadius();
    std::vector<RigidBody*> near;
    forGridCells(A->pos - vec3(r), A->pos + vec3(r), [&](RigidBody* B) {
      if (B == A || !B->enabled) return;
      float rr = r + B->boundingRadius();
      if (glm::length2(A->pos - B->pos) <= rr * rr) near.push_back(B);
    });
    for (RigidBody* B : near) {
      test(A, B);
      if (B->isDynamic()) test(B, A);
    }
  }
  (void)dt;
}

void PhysicsWorld::step(float dt) {
  active_.clear();
  for (size_t i = 0; i < bodies_.size(); ++i) {
    bodies_[i]->solverIdx = int(i);
    if (bodies_[i]->enabled && active(bodies_[i])) active_.push_back(bodies_[i]);
  }
  if (gridDirty_) rebuildGrid();
  // 1. Integrate velocities.
  for (size_t i = 0; i < bodies_.size(); ++i) {
    RigidBody* b = bodies_[i];
    b->solverIdx = int(i);
    b->prevPos = b->pos;
    b->prevRot = b->rot;
    b->updateDerived();
    if (!b->isDynamic() || b->sleeping) {
      b->force = b->torque = vec3(0.f);
      continue;
    }
    b->vel += (gravity * b->gravityScale + b->force * b->invMass) * dt;
    b->angVel += b->invInertiaWorld * b->torque * dt;
    b->vel *= 1.f / (1.f + b->linDamping * dt);
    b->angVel *= 1.f / (1.f + b->angDamping * dt);
    b->force = b->torque = vec3(0.f);
  }

  // 2. Contacts.
  collide(dt);

  // 3. Prepare.
  for (Contact& c : contacts_) {
    c.rA = c.p - c.a->pos;
    c.rB = c.b ? c.p - c.b->pos : vec3(0.f);
    vec3 vrel = c.a->pointVelocity(c.p) - (c.b ? c.b->pointVelocity(c.p) : vec3(0.f));
    float vn = glm::dot(vrel, c.n);
    c.approach = -vn;
    float e = std::max(c.a->restitution, c.b ? c.b->restitution : 0.05f);
    c.restBias = vn < -1.5f ? -e * vn : 0.f;
    vec3 vt = vrel - c.n * vn;
    c.slide = glm::length(vt);
    if (c.slide > 1e-4f) c.t1 = vt / c.slide;
    else c.t1 = glm::normalize(std::fabs(c.n.y) < 0.9f ? glm::cross(c.n, vec3(0, 1, 0)) : glm::cross(c.n, vec3(1, 0, 0)));
    c.t2 = glm::cross(c.n, c.t1);
    c.mN = effMass(c.a, c.b, c.rA, c.rB, c.n);
    c.mT1 = effMass(c.a, c.b, c.rA, c.rB, c.t1);
    c.mT2 = effMass(c.a, c.b, c.rA, c.rB, c.t2);
  }

  auto applyPair = [](Contact& c, const vec3& J) {
    c.a->vel += J * c.a->invMass;
    c.a->angVel += c.a->invInertiaWorld * glm::cross(c.rA, J);
    if (c.b) {
      c.b->vel -= J * c.b->invMass;
      c.b->angVel -= c.b->invInertiaWorld * glm::cross(c.rB, J);
    }
  };

  // 4. Velocity iterations (sequential impulses with friction).
  for (int it = 0; it < velocityIterations; ++it) {
    for (Contact& c : contacts_) {
      vec3 vrel = c.a->pointVelocity(c.p) - (c.b ? c.b->pointVelocity(c.p) : vec3(0.f));
      float vn = glm::dot(vrel, c.n);
      float dj = c.mN * (c.restBias - vn);
      float jn0 = c.jn;
      c.jn = std::max(jn0 + dj, 0.f);
      applyPair(c, c.n * (c.jn - jn0));

      float maxF = c.friction * c.jn;
      vrel = c.a->pointVelocity(c.p) - (c.b ? c.b->pointVelocity(c.p) : vec3(0.f));
      float d1 = -c.mT1 * glm::dot(vrel, c.t1);
      float j10 = c.jt1;
      c.jt1 = clampf(j10 + d1, -maxF, maxF);
      float d2 = -c.mT2 * glm::dot(vrel, c.t2);
      float j20 = c.jt2;
      c.jt2 = clampf(j20 + d2, -maxF, maxF);
      applyPair(c, c.t1 * (c.jt1 - j10) + c.t2 * (c.jt2 - j20));
    }
  }

  // 5. Split-impulse position correction (does not add energy).
  biasV_.assign(bodies_.size(), vec3(0.f));
  biasW_.assign(bodies_.size(), vec3(0.f));
  const float beta = 0.25f, slop = 0.004f;
  for (int it = 0; it < positionIterations; ++it) {
    for (Contact& c : contacts_) {
      int ia = c.a->solverIdx, ib = c.b ? c.b->solverIdx : -1;
      vec3 va = biasV_[size_t(ia)] + glm::cross(biasW_[size_t(ia)], c.rA);
      vec3 vb = ib >= 0 ? biasV_[size_t(ib)] + glm::cross(biasW_[size_t(ib)], c.rB) : vec3(0.f);
      float vn = glm::dot(va - vb, c.n);
      float target = beta * std::max(c.depth - slop, 0.f) / dt;
      float dj = c.mN * (target - vn);
      float j0 = c.jb;
      c.jb = std::max(j0 + dj, 0.f);
      vec3 J = c.n * (c.jb - j0);
      biasV_[size_t(ia)] += J * c.a->invMass;
      biasW_[size_t(ia)] += c.a->invInertiaWorld * glm::cross(c.rA, J);
      if (ib >= 0) {
        biasV_[size_t(ib)] -= J * c.b->invMass;
        biasW_[size_t(ib)] -= c.b->invInertiaWorld * glm::cross(c.rB, J);
      }
    }
  }

  // 6. Integrate positions.
  for (size_t i = 0; i < bodies_.size(); ++i) {
    RigidBody* b = bodies_[i];
    if (!b->isDynamic() || b->sleeping) continue;
    vec3 v = b->vel + biasV_[i];
    vec3 w = b->angVel + biasW_[i];
    b->pos += v * dt;
    quat dq(0.f, w.x, w.y, w.z);
    b->rot = glm::normalize(b->rot + (dq * b->rot) * (0.5f * dt));
    if (b->canSleep) {
      if (glm::length2(b->vel) < 0.015f && glm::length2(b->angVel) < 0.025f) {
        b->sleepTimer += dt;
        if (b->sleepTimer > 0.8f) { b->sleeping = true; b->vel = b->angVel = vec3(0.f); gridDirty_ = true; }
      } else {
        b->sleepTimer = 0.f;
      }
    }
  }

  // 7. Events: aggregate per (a, b) pair.
  struct Agg { ContactEvent e; vec3 wp{0.f}; float wsum = 0.f; };
  std::unordered_map<uint64_t, Agg> agg;
  for (Contact& c : contacts_) {
    if (c.jn < 1e-3f && c.approach < 0.5f) continue;
    uint64_t key = (uint64_t(c.a->id) << 32) | uint64_t(c.b ? c.b->id : 0u);
    Agg& g = agg[key];
    g.e.a = c.a;
    g.e.b = c.b;
    g.e.impulse += c.jn;
    float w = c.jn + 1e-4f;
    g.wp += c.p * w;
    g.wsum += w;
    if (c.approach > g.e.normalSpeed || glm::length2(g.e.normal) == 0.f) { g.e.normal = c.n; g.e.otherSurface = c.surf; }
    g.e.normalSpeed = std::max(g.e.normalSpeed, c.approach);
    g.e.tangentSpeed = std::max(g.e.tangentSpeed, c.slide);
  }
  for (auto& [k, g] : agg) {
    g.e.point = g.wp / g.wsum;
    events.push_back(g.e);
  }
  if (events.size() > 4096) events.erase(events.begin(), events.begin() + long(events.size() - 4096));
}

bool PhysicsWorld::raycastBodies(const vec3& o, const vec3& d, float tmax, RayHit& hit, const RigidBody* ignore) const {
  bool found = false;
  hit.t = tmax;
  auto testBody = [&](RigidBody* b) {
    if (!b->enabled || b == ignore) return;
    vec3 toB = b->pos - o;
    float proj = glm::dot(toB, d);
    float rad = b->boundingRadius();
    if (proj < -rad || proj > hit.t + rad) return;
    if (glm::length2(toB - d * proj) > rad * rad) return;
    float t;
    vec3 n;
    if (rayVsOBB(o, d, hit.t, b->worldBox(), t, n) && t < hit.t) {
      hit.t = t;
      hit.normal = n;
      hit.body = b;
      hit.surface = b->surface;
      found = true;
    }
  };
  for (RigidBody* b : active_) testBody(b);
  vec3 e = o + d * tmax;
  forGridCells(glm::min(o, e), glm::max(o, e), testBody);
  if (found) hit.point = o + d * hit.t;
  return found;
}

void PhysicsWorld::queryBodies(const vec3& c, float radius, std::vector<RigidBody*>& out) const {
  out.clear();
  auto testBody = [&](RigidBody* b) {
    if (!b->enabled) return;
    float r = radius + b->boundingRadius();
    if (glm::length2(b->pos - c) <= r * r) out.push_back(b);
  };
  for (RigidBody* b : active_) testBody(b);
  forGridCells(c - vec3(radius + 6.f), c + vec3(radius + 6.f), testBody);
}

}  // namespace vx
