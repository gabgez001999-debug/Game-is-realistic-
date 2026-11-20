#pragma once
#include "core/Common.h"

namespace vx {

enum class SurfaceType : uint8_t { Asphalt, Concrete, Grass, Dirt, Sand, Metal, Glass, Wood, Water, Flesh };

struct OBB {
  vec3 c{0.f};
  vec3 h{0.5f};
  mat3 R{1.f};  // columns = local axes in world space
  AABB aabb() const;
};

struct ContactInfo {
  vec3 point{0.f};
  vec3 normal{0, 1, 0};  // points from the obstacle towards the query shape
  float depth = 0.f;
};

struct RayHit {
  float t = 1e30f;
  vec3 point{0.f};
  vec3 normal{0, 1, 0};
  SurfaceType surface = SurfaceType::Concrete;
  int boxIndex = -1;
  struct RigidBody* body = nullptr;
};

vec3 closestPointOBB(const vec3& p, const OBB& b);
bool sphereVsOBB(const vec3& p, float r, const OBB& b, ContactInfo& out);
bool rayVsOBB(const vec3& o, const vec3& d, float tmax, const OBB& b, float& tOut, vec3& nOut);

// Regular height grid. Height queries use the exact triangulation of the render mesh
// (diagonal from (x+1,z) to (x,z+1)) so feet and wheels sit precisely on the drawn surface.
struct Heightfield {
  int nx = 0, nz = 0;  // number of cells
  float cell = 1.f;
  vec2 origin{0.f};
  std::vector<float> h;  // (nx+1)*(nz+1)

  float at(int i, int j) const {
    i = std::clamp(i, 0, nx); j = std::clamp(j, 0, nz);
    return h[size_t(j) * size_t(nx + 1) + size_t(i)];
  }
  bool contains(float x, float z) const;
  float height(float x, float z) const;
  vec3 normal(float x, float z) const;
  bool raycast(const vec3& o, const vec3& d, float tmax, float& t, vec3& n) const;
};

}  // namespace vx
