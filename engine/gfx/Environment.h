#pragma once
#include "core/Common.h"

namespace vx {

// Time of day, weather and the derived sun/moon lighting.
struct Environment {
  float timeOfDay = 15.5f;     // hours, 0..24
  float timeScale = 1.f / 60.f; // game hours per real second (1 game minute per real second)
  float latitude = 34.f;       // degrees
  float declination = 12.f;    // degrees (season)
  float cloudCover = 0.35f;
  float rain = 0.f;
  float wetness = 0.f;         // surfaces dry slowly after rain
  float fogDensity = 0.00022f;
  vec2 wind{8.f, 3.f};

  // Derived each update().
  vec3 sunDir{0, 1, 0};        // true sun direction (can be below horizon)
  vec3 lightDir{0, 1, 0};      // dominant directional light (sun or moon)
  vec3 lightColor{1.f};        // its irradiance
  vec3 groundColor{0.1f};
  float night = 0.f;
  float sunIntensity = 22.f;

  void update(float dt);
  static vec3 sunTransmittance(const vec3& dir);
};

}  // namespace vx
