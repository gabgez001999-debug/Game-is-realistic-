#pragma once
#include "core/Common.h"

namespace vx {

// Gradient noise in [-1, 1] and fractal sums. Deterministic for a given seed.
float perlin2(float x, float y, uint32_t seed = 0);
float perlin3(float x, float y, float z, uint32_t seed = 0);
float fbm2(float x, float y, int octaves, float lacunarity = 2.f, float gain = 0.5f, uint32_t seed = 0);
float ridged2(float x, float y, int octaves, uint32_t seed = 0);

}  // namespace vx
