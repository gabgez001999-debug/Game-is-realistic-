#include "gfx/Environment.h"

namespace vx {

static constexpr float kEarthR = 6360e3f, kAtmoR = 6420e3f, kHr = 7994.f, kHm = 1200.f;
static const vec3 kBetaR(5.8e-6f, 13.5e-6f, 33.1e-6f);
static const vec3 kBetaM(21e-6f);

vec3 Environment::sunTransmittance(const vec3& dir) {
  vec3 ro(0.f, kEarthR + 50.f, 0.f);
  float b = glm::dot(ro, dir), c = glm::dot(ro, ro) - kAtmoR * kAtmoR;
  float d = b * b - c;
  if (d < 0.f) return vec3(1.f);
  float t = -b + std::sqrt(d);
  // Ground occlusion.
  float cg = glm::dot(ro, ro) - kEarthR * kEarthR, dg = b * b - cg;
  if (dg > 0.f && (-b - std::sqrt(dg)) > 0.f) return vec3(0.f);
  const int N = 32;
  float seg = t / N, odR = 0.f, odM = 0.f;
  for (int i = 0; i < N; ++i) {
    vec3 p = ro + dir * (seg * (float(i) + 0.5f));
    float h = glm::length(p) - kEarthR;
    odR += std::exp(-h / kHr) * seg;
    odM += std::exp(-h / kHm) * seg;
  }
  vec3 tau = kBetaR * odR + kBetaM * 1.1f * odM;
  return glm::exp(-tau);
}

void Environment::update(float dt) {
  timeOfDay = std::fmod(timeOfDay + dt * timeScale + 24.f, 24.f);
  // Wetness follows rain; dries slowly.
  if (rain > wetness) wetness = std::min(rain, wetness + dt * 0.08f);
  else wetness = std::max(rain, wetness - dt * 0.004f);

  float lat = glm::radians(latitude), dec = glm::radians(declination);
  float H = glm::radians((timeOfDay - 12.f) * 15.f);
  float up = std::sin(lat) * std::sin(dec) + std::cos(lat) * std::cos(dec) * std::cos(H);
  float east = -std::cos(dec) * std::sin(H);
  float north = std::cos(lat) * std::sin(dec) - std::sin(lat) * std::cos(dec) * std::cos(H);
  sunDir = glm::normalize(vec3(east, up, -north));

  vec3 sunT = sunTransmittance(glm::normalize(vec3(sunDir.x, std::max(sunDir.y, 0.0f) + 0.002f, sunDir.z)));
  float horizonFade = smoothstepf(-0.04f, 0.04f, sunDir.y);
  vec3 sunIrr = sunT * 7.5f * horizonFade * (1.f - cloudCover * 0.35f) * (1.f - rain * 0.7f);
  night = smoothstepf(0.02f, -0.16f, sunDir.y);

  vec3 moonDir = glm::normalize(vec3(-sunDir.x * 0.6f + 0.2f, std::max(-sunDir.y, 0.25f), -sunDir.z * 0.6f + 0.3f));
  vec3 moonIrr = vec3(0.055f, 0.07f, 0.11f) * night * (1.f - cloudCover * 0.5f);
  if (sunDir.y > -0.03f) {
    lightDir = glm::normalize(vec3(sunDir.x, std::max(sunDir.y, 0.02f), sunDir.z));
    lightColor = sunIrr;
  } else {
    lightDir = moonDir;
    lightColor = moonIrr;
  }
  // Average ground radiance for the lower sky hemisphere (albedo ~0.15).
  float skyAmb = 0.35f * smoothstepf(-0.15f, 0.3f, sunDir.y) + 0.0015f;
  groundColor = (sunIrr * std::max(sunDir.y, 0.f) + vec3(skyAmb * 2.5f)) * (0.15f / PI);
}

}  // namespace vx
