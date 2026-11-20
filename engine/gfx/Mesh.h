#pragma once
#include "core/Common.h"
#include "platform/GL.h"

namespace vx {

// Material IDs interpreted by shaders/gbuffer.frag.
enum Mat : uint8_t {
  MAT_DEFAULT = 0,   // color = albedo, p1 = roughness, p2 = metallic, p3 = emissive
  MAT_ASPHALT = 1,
  MAT_SIDEWALK = 2,
  MAT_CURB = 3,
  MAT_FACADE = 4,    // extra = (seed, style, floorHeight, windowWidth)
  MAT_ROOF = 5,
  MAT_TERRAIN = 6,
  MAT_CARBODY = 7,   // extra = (rest.xyz, damage), p1 = zone
  MAT_TIRE = 8,
  MAT_RIM = 9,
  MAT_GLASS = 10,
  MAT_EMISSIVE = 11, // color * p1 * 0.25 (HDR)
  MAT_SKIN = 12,
  MAT_CLOTH = 13,
  MAT_BARK = 14,
  MAT_LEAF = 15,
  MAT_METAL = 16,
  MAT_GRASS = 17,
  MAT_INTERIOR = 18,
  MAT_DIRT = 19,
};

// 56-byte vertex shared by every mesh in the engine.
struct Vertex {
  vec3 pos{0.f};
  vec3 normal{0.f, 1.f, 0.f};
  vec2 uv{0.f};
  uint8_t color[4]{200, 200, 200, 255};
  uint8_t mat[4]{0, 128, 0, 0};
  vec4 extra{0.f};
};
static_assert(sizeof(Vertex) == 56, "Vertex layout");

struct MeshData {
  std::vector<Vertex> v;
  std::vector<uint32_t> i;
  void clear() { v.clear(); i.clear(); }
  void append(const MeshData& o, const mat4& m);
  void computeNormals();
  AABB bounds() const;
};

class Mesh {
 public:
  Mesh() = default;
  ~Mesh();
  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  void upload(const MeshData& d, bool dynamic = false);
  void updateVertices(const std::vector<Vertex>& v);
  void draw() const;
  bool valid() const { return vao_ != 0 && count_ > 0; }
  const AABB& bounds() const { return bounds_; }
  uint32_t indexCount() const { return count_; }

 private:
  GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
  uint32_t count_ = 0;
  size_t vcount_ = 0;
  AABB bounds_;
};

}  // namespace vx
