#include "gfx/MeshBuilder.h"

namespace vx {

Surface surf(Mat m, uint32_t rgbHex, float roughness, float metallic, float p3) {
  Surface s;
  s.color[0] = uint8_t(rgbHex >> 16);
  s.color[1] = uint8_t(rgbHex >> 8);
  s.color[2] = uint8_t(rgbHex);
  s.color[3] = 255;
  s.mat[0] = m;
  s.mat[1] = uint8_t(saturate(roughness) * 255.f);
  s.mat[2] = uint8_t(saturate(metallic) * 255.f);
  s.mat[3] = uint8_t(saturate(p3) * 255.f);
  return s;
}

void applySurface(Vertex& v, const Surface& s) {
  for (int k = 0; k < 4; ++k) { v.color[k] = s.color[k]; v.mat[k] = s.mat[k]; }
  v.extra = s.extra;
}

mat3 basisFromY(const vec3& dir) {
  vec3 y = glm::normalize(dir);
  vec3 ref = std::fabs(y.y) < 0.95f ? vec3(0, 1, 0) : vec3(1, 0, 0);
  vec3 x = glm::normalize(glm::cross(ref, y));
  if (std::fabs(y.y) >= 0.95f) x = glm::normalize(glm::cross(y, vec3(0, 0, 1)));
  vec3 z = glm::cross(x, y);
  return mat3(x, y, z);
}

void addQuad(MeshData& md, const vec3& a, const vec3& b, const vec3& c, const vec3& d, const Surface& s, vec2 uvScale) {
  vec3 n = glm::normalize(glm::cross(b - a, d - a));
  uint32_t base = uint32_t(md.v.size());
  const vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  const vec3 ps[4] = {a, b, c, d};
  for (int k = 0; k < 4; ++k) {
    Vertex v;
    v.pos = ps[k];
    v.normal = n;
    v.uv = uvs[k] * uvScale;
    applySurface(v, s);
    md.v.push_back(v);
  }
  md.i.insert(md.i.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void addBox(MeshData& md, const vec3& c, const vec3& h, const Surface& s, const mat3& R, const vec3* uvOrigin,
            bool skipBottom) {
  // Faces: +X -X +Y -Y +Z -Z ; corners in local coordinates.
  const vec3 N[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  vec3 org = uvOrigin ? *uvOrigin : vec3(-h.x, -h.y, -h.z);
  for (int f = 0; f < 6; ++f) {
    if (skipBottom && f == 3) continue;
    vec3 n = N[f];
    vec3 t, b;
    if (f == 2 || f == 3) { t = vec3(1, 0, 0); b = vec3(0, 0, f == 2 ? -1.f : 1.f); }
    else { b = vec3(0, 1, 0); t = glm::cross(b, n); }
    vec3 ext = glm::abs(t) * h, bext = glm::abs(b) * h, next = glm::abs(n) * h;
    float ht = glm::dot(ext, vec3(1)), hb = glm::dot(bext, vec3(1)), hn = glm::dot(next, vec3(1));
    vec3 corners[4] = {n * hn - t * ht - b * hb, n * hn + t * ht - b * hb, n * hn + t * ht + b * hb, n * hn - t * ht + b * hb};
    uint32_t base = uint32_t(md.v.size());
    for (auto& p : corners) {
      Vertex v;
      v.pos = c + R * p;
      v.normal = R * n;
      vec3 rel = p - org;
      // Metric UVs: top/bottom use XZ, sides use (distance from left edge, height above origin).
      if (f == 2 || f == 3) v.uv = vec2(rel.x, rel.z);
      else v.uv = vec2(glm::dot(p, t) + ht, rel.y);
      applySurface(v, s);
      md.v.push_back(v);
    }
    md.i.insert(md.i.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }
}

void addCylinder(MeshData& md, const vec3& base, const vec3& axis, float r0, float r1, float len, int segs,
                 const Surface& s, bool caps) {
  mat3 B = basisFromY(axis);
  uint32_t start = uint32_t(md.v.size());
  float slope = (r0 - r1) / len;
  for (int i = 0; i <= segs; ++i) {
    float a = TAU * float(i) / float(segs);
    vec3 dir(std::cos(a), 0.f, std::sin(a));
    vec3 n = glm::normalize(vec3(dir.x, slope, dir.z));
    for (int e = 0; e < 2; ++e) {
      Vertex v;
      float r = e ? r1 : r0;
      v.pos = base + B * (dir * r + vec3(0, e ? len : 0.f, 0));
      v.normal = B * n;
      v.uv = vec2(float(i) / float(segs) * TAU * std::max(r0, r1), e ? len : 0.f);
      applySurface(v, s);
      md.v.push_back(v);
    }
  }
  for (int i = 0; i < segs; ++i) {
    uint32_t a = start + uint32_t(i * 2), b = a + 1, c = a + 2, d = a + 3;
    md.i.insert(md.i.end(), {a, b, c, c, b, d});
  }
  if (!caps) return;
  for (int e = 0; e < 2; ++e) {
    uint32_t center = uint32_t(md.v.size());
    Vertex cv;
    float r = e ? r1 : r0;
    cv.pos = base + B * vec3(0, e ? len : 0.f, 0);
    cv.normal = B * vec3(0, e ? 1.f : -1.f, 0);
    applySurface(cv, s);
    md.v.push_back(cv);
    for (int i = 0; i <= segs; ++i) {
      float a = TAU * float(i) / float(segs);
      Vertex v = cv;
      v.pos = base + B * vec3(std::cos(a) * r, e ? len : 0.f, std::sin(a) * r);
      v.uv = vec2(std::cos(a), std::sin(a)) * r;
      md.v.push_back(v);
    }
    for (int i = 0; i < segs; ++i) {
      uint32_t a = center + 1 + uint32_t(i), b = a + 1;
      if (e) md.i.insert(md.i.end(), {center, b, a});
      else md.i.insert(md.i.end(), {center, a, b});
    }
  }
}

void addEllipsoid(MeshData& md, const vec3& c, const vec3& radii, int segs, int rings, const Surface& s) {
  uint32_t start = uint32_t(md.v.size());
  for (int r = 0; r <= rings; ++r) {
    float phi = PI * float(r) / float(rings);
    for (int i = 0; i <= segs; ++i) {
      float th = TAU * float(i) / float(segs);
      vec3 u(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
      Vertex v;
      v.pos = c + u * radii;
      v.normal = glm::normalize(u / radii);
      v.uv = vec2(float(i) / float(segs), float(r) / float(rings));
      applySurface(v, s);
      md.v.push_back(v);
    }
  }
  for (int r = 0; r < rings; ++r)
    for (int i = 0; i < segs; ++i) {
      uint32_t a = start + uint32_t(r * (segs + 1) + i), b = a + 1, cc = a + uint32_t(segs + 1), d = cc + 1;
      md.i.insert(md.i.end(), {a, b, cc, b, d, cc});
    }
}

void addLimb(MeshData& md, const vec3& a, const vec3& b, float ra, float rb, int segs, const Surface& s) {
  vec3 d = b - a;
  float len = glm::length(d);
  if (len < 1e-4f) return;
  // Profile: hemisphere at a (radius ra), tapered body, hemisphere at b (radius rb).
  std::vector<vec2> prof;
  const int hs = 4;
  for (int k = 0; k <= hs; ++k) {
    float t = float(k) / float(hs) * (PI * 0.5f);
    prof.push_back(vec2(std::sin(t) * ra, -std::cos(t) * ra));
  }
  for (int k = 0; k <= hs; ++k) {
    float t = float(k) / float(hs) * (PI * 0.5f);
    prof.push_back(vec2(std::cos(t) * rb, len + std::sin(t) * rb));
  }
  mat3 B = basisFromY(d);
  mat4 m(1.f);
  m[0] = vec4(B[0], 0); m[1] = vec4(B[1], 0); m[2] = vec4(B[2], 0); m[3] = vec4(a, 1);
  addRevolve(md, prof, segs, s, m);
}

void addRevolve(MeshData& md, const std::vector<vec2>& profile, int segs, const Surface& s, const mat4& m) {
  uint32_t start = uint32_t(md.v.size());
  int np = int(profile.size());
  mat3 nm = glm::transpose(glm::inverse(mat3(m)));
  for (int i = 0; i <= segs; ++i) {
    float a = TAU * float(i) / float(segs);
    float ca = std::cos(a), sa = std::sin(a);
    for (int k = 0; k < np; ++k) {
      vec2 p = profile[size_t(k)];
      vec2 prev = profile[size_t(std::max(0, k - 1))], next = profile[size_t(std::min(np - 1, k + 1))];
      vec2 tng = next - prev;
      vec2 n2 = glm::normalize(vec2(tng.y, -tng.x) + vec2(1e-6f, 0.f));
      Vertex v;
      v.pos = vec3(m * vec4(ca * p.x, p.y, sa * p.x, 1.f));
      v.normal = glm::normalize(nm * vec3(ca * n2.x, n2.y, sa * n2.x));
      v.uv = vec2(float(i) / float(segs), float(k) / float(np - 1));
      applySurface(v, s);
      md.v.push_back(v);
    }
  }
  for (int i = 0; i < segs; ++i)
    for (int k = 0; k < np - 1; ++k) {
      uint32_t a = start + uint32_t(i * np + k), b = a + 1, c = a + uint32_t(np), d = c + 1;
      md.i.insert(md.i.end(), {a, b, c, b, d, c});
    }
}

}  // namespace vx
