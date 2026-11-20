#include "physics/Collision.h"

namespace vx {

AABB OBB::aabb() const {
  vec3 e = glm::abs(R[0]) * h.x + glm::abs(R[1]) * h.y + glm::abs(R[2]) * h.z;
  AABB a;
  a.min = c - e;
  a.max = c + e;
  return a;
}

vec3 closestPointOBB(const vec3& p, const OBB& b) {
  vec3 d = p - b.c;
  vec3 q = b.c;
  for (int i = 0; i < 3; ++i) {
    float dist = glm::clamp(glm::dot(d, b.R[i]), -b.h[i], b.h[i]);
    q += b.R[i] * dist;
  }
  return q;
}

bool sphereVsOBB(const vec3& p, float r, const OBB& b, ContactInfo& out) {
  vec3 d = p - b.c;
  vec3 local(glm::dot(d, b.R[0]), glm::dot(d, b.R[1]), glm::dot(d, b.R[2]));
  vec3 cl = glm::clamp(local, -b.h, b.h);
  vec3 diff = local - cl;
  float dist2 = glm::dot(diff, diff);
  if (dist2 > r * r) return false;
  if (dist2 > 1e-10f) {
    float dist = std::sqrt(dist2);
    vec3 nl = diff / dist;
    out.normal = b.R * nl;
    out.depth = r - dist;
    out.point = b.c + b.R * cl;
    return true;
  }
  // Center inside the box: push out along the axis of least penetration.
  vec3 pen = b.h - glm::abs(local);
  int axis = 0;
  if (pen.y < pen[axis]) axis = 1;
  if (pen.z < pen[axis]) axis = 2;
  vec3 nl(0.f);
  nl[axis] = local[axis] >= 0.f ? 1.f : -1.f;
  out.normal = b.R * nl;
  out.depth = pen[axis] + r;
  vec3 surf = local;
  surf[axis] = nl[axis] * b.h[axis];
  out.point = b.c + b.R * surf;
  return true;
}

bool rayVsOBB(const vec3& o, const vec3& d, float tmax, const OBB& b, float& tOut, vec3& nOut) {
  vec3 p = b.c - o;
  float tmin = 0.f, tfar = tmax;
  int axisNear = -1;
  float signNear = 1.f;
  for (int i = 0; i < 3; ++i) {
    float e = glm::dot(b.R[i], p);
    float f = glm::dot(b.R[i], d);
    if (std::fabs(f) > 1e-8f) {
      float t1 = (e + b.h[i]) / f, t2 = (e - b.h[i]) / f;
      float s = -1.f;
      if (t1 > t2) { std::swap(t1, t2); s = 1.f; }
      if (t1 > tmin) { tmin = t1; axisNear = i; signNear = s; }
      if (t2 < tfar) tfar = t2;
      if (tmin > tfar) return false;
    } else if (-e - b.h[i] > 0.f || -e + b.h[i] < 0.f) {
      return false;
    }
  }
  if (axisNear < 0) return false;  // origin inside
  tOut = tmin;
  nOut = b.R[axisNear] * signNear;
  if (glm::dot(nOut, d) > 0.f) nOut = -nOut;
  return true;
}

bool Heightfield::contains(float x, float z) const {
  float fx = (x - origin.x) / cell, fz = (z - origin.y) / cell;
  return fx >= 0.f && fz >= 0.f && fx <= float(nx) && fz <= float(nz);
}

float Heightfield::height(float x, float z) const {
  if (h.empty()) return -1000.f;
  float fx = (x - origin.x) / cell, fz = (z - origin.y) / cell;
  fx = clampf(fx, 0.f, float(nx) - 1e-4f);
  fz = clampf(fz, 0.f, float(nz) - 1e-4f);
  int i = int(fx), j = int(fz);
  float u = fx - float(i), v = fz - float(j);
  float ha = at(i, j), hb = at(i + 1, j), hc = at(i, j + 1), hd = at(i + 1, j + 1);
  if (u + v <= 1.f) return ha + (hb - ha) * u + (hc - ha) * v;
  return hd + (hc - hd) * (1.f - u) + (hb - hd) * (1.f - v);
}

vec3 Heightfield::normal(float x, float z) const {
  float fx = (x - origin.x) / cell, fz = (z - origin.y) / cell;
  fx = clampf(fx, 0.f, float(nx) - 1e-4f);
  fz = clampf(fz, 0.f, float(nz) - 1e-4f);
  int i = int(fx), j = int(fz);
  float u = fx - float(i), v = fz - float(j);
  float ha = at(i, j), hb = at(i + 1, j), hc = at(i, j + 1), hd = at(i + 1, j + 1);
  vec3 n;
  if (u + v <= 1.f) n = glm::cross(vec3(0, hc - ha, cell), vec3(cell, hb - ha, 0));
  else n = glm::cross(vec3(0, hb - hd, -cell), vec3(-cell, hc - hd, 0));
  return glm::normalize(n);
}

bool Heightfield::raycast(const vec3& o, const vec3& d, float tmax, float& t, vec3& n) const {
  if (h.empty()) return false;
  float step = cell * 0.35f;
  float prevT = 0.f;
  float prevDiff = o.y - height(o.x, o.z);
  if (prevDiff < 0.f) return false;
  for (float tt = step; tt <= tmax + step; tt += step) {
    float tc = std::min(tt, tmax);
    vec3 p = o + d * tc;
    float diff = p.y - height(p.x, p.z);
    if (diff <= 0.f) {
      float lo = prevT, hi = tc;
      for (int k = 0; k < 10; ++k) {
        float mid = (lo + hi) * 0.5f;
        vec3 q = o + d * mid;
        if (q.y - height(q.x, q.z) > 0.f) lo = mid; else hi = mid;
      }
      t = hi;
      vec3 q = o + d * t;
      n = normal(q.x, q.z);
      return true;
    }
    prevT = tc;
    prevDiff = diff;
    if (tc >= tmax) break;
  }
  return false;
}

}  // namespace vx
