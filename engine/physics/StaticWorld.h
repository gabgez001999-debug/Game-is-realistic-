#pragma once
#include "physics/Collision.h"

namespace vx {

struct StaticBox {
  OBB box;
  SurfaceType surface = SurfaceType::Concrete;
};

// Immovable world geometry (terrain + buildings, curbs, walls) with a uniform grid broadphase.
class StaticWorld {
 public:
  Heightfield terrain;
  std::vector<StaticBox> boxes;

  void build(float cellSize = 16.f);
  void query(const vec3& mn, const vec3& mx, std::vector<uint32_t>& out) const;
  bool raycast(const vec3& o, const vec3& d, float tmax, RayHit& hit) const;
  // Surface under (x, z) using terrain and box tops at or below fromY.
  float groundHeight(float x, float z, float fromY, SurfaceType* surface = nullptr) const;
  // Collects sphere contacts against boxes and terrain.
  void sphereContacts(const vec3& p, float r, std::vector<ContactInfo>& out, std::vector<SurfaceType>* surfaces = nullptr) const;
  SurfaceType terrainSurface(float x, float z) const;

 private:
  vec2 gridOrigin_{0.f};
  int gw_ = 0, gh_ = 0;
  float cell_ = 16.f;
  std::vector<std::vector<uint32_t>> cells_;
  mutable std::vector<uint32_t> stamp_;
  mutable uint32_t stampId_ = 1;
};

}  // namespace vx
