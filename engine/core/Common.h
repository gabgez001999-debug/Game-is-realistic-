#pragma once
// Veritas Engine - common types, math aliases, logging and small helpers.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>

namespace vx {

using glm::ivec2;
using glm::ivec3;
using glm::mat3;
using glm::mat4;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;

void logInfo(const char* fmt, ...);
void logWarn(const char* fmt, ...);
void logError(const char* fmt, ...);

inline float saturate(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth01(float x) { x = saturate(x); return x * x * (3.f - 2.f * x); }
inline float smoothstepf(float a, float b, float x) { return smooth01((x - a) / (b - a)); }
inline float signf(float x) { return x < 0.f ? -1.f : 1.f; }
inline float sqr(float x) { return x * x; }

// Frame-rate independent exponential smoothing towards a target.
inline float damp(float current, float target, float rate, float dt) {
  return target + (current - target) * std::exp(-rate * dt);
}
inline vec3 damp(const vec3& current, const vec3& target, float rate, float dt) {
  return target + (current - target) * std::exp(-rate * dt);
}
inline float wrapAngle(float a) {
  while (a > PI) a -= TAU;
  while (a < -PI) a += TAU;
  return a;
}
inline float dampAngle(float current, float target, float rate, float dt) {
  return current + wrapAngle(target - current) * (1.f - std::exp(-rate * dt));
}

// Object convention: local forward = -Z, right = +X, up = +Y.
inline vec3 forwardFromYaw(float yaw) { return vec3(-std::sin(yaw), 0.f, -std::cos(yaw)); }
inline vec3 rightFromYaw(float yaw) { return vec3(std::cos(yaw), 0.f, -std::sin(yaw)); }
inline float yawFromForward(const vec3& f) { return std::atan2(-f.x, -f.z); }

inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
inline uint32_t hash2i(int x, int y) { return hash32(uint32_t(x) * 73856093u ^ hash32(uint32_t(y) * 19349663u)); }
inline float hashf(uint32_t x) { return float(hash32(x) & 0xFFFFFF) / float(0x1000000); }

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 1) {}
  uint64_t next() {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
  }
  float uniform() { return float(next() >> 40) / float(1ull << 24); }
  float range(float a, float b) { return a + (b - a) * uniform(); }
  int rangei(int a, int b) { return a + int(next() % uint64_t(b - a + 1)); }
  bool chance(float p) { return uniform() < p; }
  float normal() {
    float u1 = std::max(uniform(), 1e-7f), u2 = uniform();
    return std::sqrt(-2.f * std::log(u1)) * std::cos(TAU * u2);
  }
  vec3 unitVector() {
    float z = range(-1.f, 1.f), a = range(0.f, TAU), r = std::sqrt(1.f - z * z);
    return vec3(r * std::cos(a), r * std::sin(a), z);
  }
};

struct AABB {
  vec3 min{1e30f}, max{-1e30f};
  void expand(const vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
  vec3 center() const { return (min + max) * 0.5f; }
  vec3 extent() const { return (max - min) * 0.5f; }
  bool valid() const { return min.x <= max.x; }
};

}  // namespace vx
