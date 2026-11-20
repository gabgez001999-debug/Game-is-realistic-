#pragma once
#include "gfx/Mesh.h"

namespace vx {

// Material description stamped onto generated vertices.
struct Surface {
  uint8_t color[4]{200, 200, 200, 255};  // sRGB albedo
  uint8_t mat[4]{MAT_DEFAULT, 128, 0, 0};
  vec4 extra{0.f};
};

// color in sRGB 0..255; roughness/metallic 0..1; emissive in 0..1 (x16 HDR in shader)
Surface surf(Mat m, uint32_t rgbHex, float roughness = 0.5f, float metallic = 0.f, float p3 = 0.f);

void applySurface(Vertex& v, const Surface& s);

// Axis-aligned box in local space, then rotated by R about c. UVs are metric:
// vertical faces u = along face, v = height above uvOrigin.y.
void addBox(MeshData& md, const vec3& c, const vec3& h, const Surface& s, const mat3& R = mat3(1.f),
            const vec3* uvOrigin = nullptr, bool skipBottom = true);
void addQuad(MeshData& md, const vec3& a, const vec3& b, const vec3& c, const vec3& d, const Surface& s,
             vec2 uvScale = vec2(1.f));
// Tapered cylinder from base along axis.
void addCylinder(MeshData& md, const vec3& base, const vec3& axis, float r0, float r1, float len, int segs,
                 const Surface& s, bool caps = true);
// Capsule-ish limb from a to b with radii ra, rb (hemispherical ends).
void addLimb(MeshData& md, const vec3& a, const vec3& b, float ra, float rb, int segs, const Surface& s);
void addEllipsoid(MeshData& md, const vec3& c, const vec3& radii, int segs, int rings, const Surface& s);
// Revolves a (radius, height) profile around the local Y axis, then transforms by m.
void addRevolve(MeshData& md, const std::vector<vec2>& profile, int segs, const Surface& s, const mat4& m);
// Grid in XZ, heights from callback.
template <class F>
void addHeightGrid(MeshData& md, vec2 origin, vec2 size, int nx, int nz, F heightFn, const Surface& s) {
  uint32_t base = uint32_t(md.v.size());
  for (int z = 0; z <= nz; ++z)
    for (int x = 0; x <= nx; ++x) {
      Vertex v;
      float px = origin.x + size.x * float(x) / float(nx);
      float pz = origin.y + size.y * float(z) / float(nz);
      v.pos = vec3(px, heightFn(px, pz), pz);
      v.uv = vec2(px, pz);
      applySurface(v, s);
      md.v.push_back(v);
    }
  for (int z = 0; z < nz; ++z)
    for (int x = 0; x < nx; ++x) {
      uint32_t a = base + uint32_t(z * (nx + 1) + x), b = a + 1, c = a + uint32_t(nx + 1), d = c + 1;
      md.i.insert(md.i.end(), {a, c, b, b, c, d});
    }
}

// Rotation that maps +Y onto dir.
mat3 basisFromY(const vec3& dir);

}  // namespace vx
