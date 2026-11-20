#include "physics/StaticWorld.h"

namespace vx {

void StaticWorld::build(float cellSize) {
  cell_ = cellSize;
  AABB all;
  for (auto& b : boxes) {
    AABB a = b.box.aabb();
    all.expand(a.min);
    all.expand(a.max);
  }
  if (!all.valid()) { gw_ = gh_ = 0; return; }
  gridOrigin_ = vec2(all.min.x, all.min.z) - vec2(cell_);
  gw_ = int((all.max.x - all.min.x) / cell_) + 3;
  gh_ = int((all.max.z - all.min.z) / cell_) + 3;
  cells_.assign(size_t(gw_) * size_t(gh_), {});
  for (uint32_t i = 0; i < boxes.size(); ++i) {
    AABB a = boxes[i].box.aabb();
    int x0 = int((a.min.x - gridOrigin_.x) / cell_), x1 = int((a.max.x - gridOrigin_.x) / cell_);
    int z0 = int((a.min.z - gridOrigin_.y) / cell_), z1 = int((a.max.z - gridOrigin_.y) / cell_);
    for (int z = std::max(0, z0); z <= std::min(gh_ - 1, z1); ++z)
      for (int x = std::max(0, x0); x <= std::min(gw_ - 1, x1); ++x) cells_[size_t(z) * size_t(gw_) + size_t(x)].push_back(i);
  }
  stamp_.assign(boxes.size(), 0);
  logInfo("StaticWorld: %zu boxes, grid %dx%d", boxes.size(), gw_, gh_);
}

void StaticWorld::query(const vec3& mn, const vec3& mx, std::vector<uint32_t>& out) const {
  out.clear();
  if (gw_ == 0) return;
  if (++stampId_ == 0) { std::fill(stamp_.begin(), stamp_.end(), 0); stampId_ = 1; }
  int x0 = int(std::floor((mn.x - gridOrigin_.x) / cell_)), x1 = int(std::floor((mx.x - gridOrigin_.x) / cell_));
  int z0 = int(std::floor((mn.z - gridOrigin_.y) / cell_)), z1 = int(std::floor((mx.z - gridOrigin_.y) / cell_));
  x0 = std::max(0, x0); z0 = std::max(0, z0);
  x1 = std::min(gw_ - 1, x1); z1 = std::min(gh_ - 1, z1);
  for (int z = z0; z <= z1; ++z)
    for (int x = x0; x <= x1; ++x)
      for (uint32_t i : cells_[size_t(z) * size_t(gw_) + size_t(x)]) {
        if (stamp_[i] == stampId_) continue;
        stamp_[i] = stampId_;
        AABB a = boxes[i].box.aabb();
        if (a.max.x < mn.x || a.min.x > mx.x || a.max.y < mn.y || a.min.y > mx.y || a.max.z < mn.z || a.min.z > mx.z) continue;
        out.push_back(i);
      }
}

SurfaceType StaticWorld::terrainSurface(float x, float z) const {
  float h = terrain.height(x, z);
  if (h < 2.5f) return SurfaceType::Sand;
  vec3 n = terrain.normal(x, z);
  if (n.y < 0.75f) return SurfaceType::Dirt;
  return SurfaceType::Grass;
}

bool StaticWorld::raycast(const vec3& o, const vec3& d, float tmax, RayHit& hit) const {
  bool found = false;
  hit.t = tmax;
  vec3 e = o + d * tmax;
  static thread_local std::vector<uint32_t> cand;
  query(glm::min(o, e) - vec3(0.01f), glm::max(o, e) + vec3(0.01f), cand);
  for (uint32_t i : cand) {
    float t;
    vec3 n;
    if (rayVsOBB(o, d, hit.t, boxes[i].box, t, n) && t < hit.t) {
      hit.t = t;
      hit.normal = n;
      hit.boxIndex = int(i);
      hit.surface = boxes[i].surface;
      found = true;
    }
  }
  float tt;
  vec3 tn;
  if (terrain.raycast(o, d, hit.t, tt, tn) && tt < hit.t) {
    hit.t = tt;
    hit.normal = tn;
    hit.boxIndex = -1;
    vec3 p = o + d * tt;
    hit.surface = terrainSurface(p.x, p.z);
    found = true;
  }
  if (found) hit.point = o + d * hit.t;
  return found;
}

float StaticWorld::groundHeight(float x, float z, float fromY, SurfaceType* surface) const {
  RayHit h;
  if (raycast(vec3(x, fromY, z), vec3(0, -1, 0), 200.f, h)) {
    if (surface) *surface = h.surface;
    return h.point.y;
  }
  if (surface) *surface = terrainSurface(x, z);
  return terrain.height(x, z);
}

void StaticWorld::sphereContacts(const vec3& p, float r, std::vector<ContactInfo>& out, std::vector<SurfaceType>* surfaces) const {
  static thread_local std::vector<uint32_t> cand;
  query(p - vec3(r), p + vec3(r), cand);
  for (uint32_t i : cand) {
    ContactInfo c;
    if (sphereVsOBB(p, r, boxes[i].box, c)) {
      out.push_back(c);
      if (surfaces) surfaces->push_back(boxes[i].surface);
    }
  }
  if (!terrain.h.empty() && terrain.contains(p.x, p.z)) {
    float hgt = terrain.height(p.x, p.z);
    vec3 n = terrain.normal(p.x, p.z);
    float d = (p.y - hgt) * n.y;
    if (d < r) {
      ContactInfo c;
      c.normal = n;
      c.depth = r - d;
      c.point = p - n * d;
      out.push_back(c);
      if (surfaces) surfaces->push_back(terrainSurface(p.x, p.z));
    }
  }
}

}  // namespace vx
