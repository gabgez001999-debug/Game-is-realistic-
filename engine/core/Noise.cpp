#include "core/Noise.h"

namespace vx {

static inline float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

static inline float grad2(uint32_t h, float x, float y) {
  float a = float(h & 0xFFFF) * (TAU / 65536.f);
  return std::cos(a) * x + std::sin(a) * y;
}

float perlin2(float x, float y, uint32_t seed) {
  int xi = int(std::floor(x)), yi = int(std::floor(y));
  float xf = x - float(xi), yf = y - float(yi);
  auto h = [&](int i, int j) { return hash32(hash2i(i, j) ^ seed * 0x9E3779B9u); };
  float n00 = grad2(h(xi, yi), xf, yf);
  float n10 = grad2(h(xi + 1, yi), xf - 1.f, yf);
  float n01 = grad2(h(xi, yi + 1), xf, yf - 1.f);
  float n11 = grad2(h(xi + 1, yi + 1), xf - 1.f, yf - 1.f);
  float u = fade(xf), v = fade(yf);
  return lerpf(lerpf(n00, n10, u), lerpf(n01, n11, u), v) * 1.41f;
}

static inline float grad3(uint32_t h, float x, float y, float z) {
  switch (h % 12u) {
    case 0: return x + y; case 1: return -x + y; case 2: return x - y; case 3: return -x - y;
    case 4: return x + z; case 5: return -x + z; case 6: return x - z; case 7: return -x - z;
    case 8: return y + z; case 9: return -y + z; case 10: return y - z; default: return -y - z;
  }
}

float perlin3(float x, float y, float z, uint32_t seed) {
  int xi = int(std::floor(x)), yi = int(std::floor(y)), zi = int(std::floor(z));
  float xf = x - xi, yf = y - yi, zf = z - zi;
  auto h = [&](int i, int j, int k) { return hash32(hash2i(i, j) ^ hash32(uint32_t(k) * 83492791u) ^ seed); };
  float u = fade(xf), v = fade(yf), w = fade(zf);
  float c[2][2][2];
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
      for (int k = 0; k < 2; ++k)
        c[i][j][k] = grad3(h(xi + i, yi + j, zi + k), xf - i, yf - j, zf - k);
  float x00 = lerpf(c[0][0][0], c[1][0][0], u), x10 = lerpf(c[0][1][0], c[1][1][0], u);
  float x01 = lerpf(c[0][0][1], c[1][0][1], u), x11 = lerpf(c[0][1][1], c[1][1][1], u);
  return lerpf(lerpf(x00, x10, v), lerpf(x01, x11, v), w);
}

float fbm2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed) {
  float sum = 0.f, amp = 1.f, norm = 0.f;
  for (int i = 0; i < octaves; ++i) {
    sum += amp * perlin2(x, y, seed + uint32_t(i) * 131u);
    norm += amp;
    x *= lacunarity; y *= lacunarity; amp *= gain;
  }
  return sum / norm;
}

float ridged2(float x, float y, int octaves, uint32_t seed) {
  float sum = 0.f, amp = 0.5f, freq = 1.f, weight = 1.f;
  for (int i = 0; i < octaves; ++i) {
    float n = 1.f - std::fabs(perlin2(x * freq, y * freq, seed + uint32_t(i) * 977u));
    n *= n * weight;
    weight = saturate(n * 2.f);
    sum += n * amp;
    freq *= 2.f; amp *= 0.5f;
  }
  return sum;
}

}  // namespace vx
