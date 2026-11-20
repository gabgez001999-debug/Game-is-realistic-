#pragma once
#include "core/Common.h"

namespace vx {

struct Camera {
  vec3 pos{0.f};
  mat4 view{1.f};
  mat4 proj{1.f};
  float fovY = glm::radians(60.f);
  float aspect = 16.f / 9.f;
  float nearZ = 0.15f;
  float farZ = 6000.f;

  void setPerspective(float fovRadians, float aspectRatio, float n = 0.15f, float f = 6000.f) {
    fovY = fovRadians; aspect = aspectRatio; nearZ = n; farZ = f;
    proj = glm::perspective(fovY, aspect, nearZ, farZ);
  }
  void lookAt(const vec3& eye, const vec3& target, const vec3& up = vec3(0, 1, 0)) {
    pos = eye;
    view = glm::lookAt(eye, target, up);
  }
  mat4 viewProj() const { return proj * view; }
  vec3 forward() const { return -vec3(view[0][2], view[1][2], view[2][2]); }
  vec3 right() const { return vec3(view[0][0], view[1][0], view[2][0]); }
  vec3 up() const { return vec3(view[0][1], view[1][1], view[2][1]); }
};

// Six frustum planes (inward facing) for sphere culling.
struct Frustum {
  vec4 planes[6];
  void fromMatrix(const mat4& m) {
    mat4 t = glm::transpose(m);
    planes[0] = t[3] + t[0]; planes[1] = t[3] - t[0];
    planes[2] = t[3] + t[1]; planes[3] = t[3] - t[1];
    planes[4] = t[3] + t[2]; planes[5] = t[3] - t[2];
    for (auto& p : planes) p /= glm::length(vec3(p));
  }
  bool sphere(const vec3& c, float r) const {
    for (auto& p : planes)
      if (glm::dot(vec3(p), c) + p.w < -r) return false;
    return true;
  }
};

}  // namespace vx
