// ============================================================================
//  Veritas Engine - shared shader library
// ============================================================================
const float PI = 3.14159265359;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uViewProj;
uniform mat4 uInvViewProj;
uniform mat4 uPrevViewProj;
uniform mat4 uInvProj;
uniform vec3 uCamPos;
uniform vec3 uSunDir;      // direction towards the sun (or moon at night)
uniform vec3 uSunColor;    // direct light irradiance (already atmosphere-attenuated)
uniform vec3 uTrueSunDir;  // actual sun direction (may be below horizon)
uniform float uTime;
uniform vec2 uResolution;
uniform float uWetness;
uniform float uRain;
uniform float uCloudCover;
uniform float uFogDensity;
uniform float uNight;      // 0 = day, 1 = full night
uniform vec2 uWind;

// --------------------------------------------------------------------------
//  Color
// --------------------------------------------------------------------------
vec3 srgbToLinear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }
vec3 linearToSrgb(vec3 c) {
  vec3 lo = c * 12.92;
  vec3 hi = 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
  return mix(lo, hi, step(vec3(0.0031308), c));
}
float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// Approximate blackbody color for a temperature in Kelvin (1000..12000).
vec3 kelvin(float k) {
  k = clamp(k, 1000.0, 12000.0) / 100.0;
  vec3 c;
  c.r = k <= 66.0 ? 1.0 : clamp(1.292936 * pow(k - 60.0, -0.1332047), 0.0, 1.0);
  c.g = k <= 66.0 ? clamp(0.3900816 * log(k) - 0.6318414, 0.0, 1.0) : clamp(1.1298909 * pow(k - 60.0, -0.0755148), 0.0, 1.0);
  c.b = k >= 66.0 ? 1.0 : (k <= 19.0 ? 0.0 : clamp(0.5432068 * log(k - 10.0) - 1.1962541, 0.0, 1.0));
  return srgbToLinear(c);
}

// --------------------------------------------------------------------------
//  Hash / noise (Dave Hoskins hashes, value noise, fbm)
// --------------------------------------------------------------------------
float hash11(float p) { p = fract(p * .1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * .1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float hash13(vec3 p3) { p3 = fract(p3 * .1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
vec2 hash22(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
vec3 hash32(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973)); p3 += dot(p3, p3.yxz + 33.33); return fract((p3.xxy + p3.yzz) * p3.zyx); }

float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i), hash12(i + vec2(1, 0)), u.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), u.x), u.y);
}
float vnoise3(vec3 p) {
  vec3 i = floor(p), f = fract(p);
  vec3 u = f * f * (3.0 - 2.0 * f);
  float a = hash13(i), b = hash13(i + vec3(1, 0, 0)), c = hash13(i + vec3(0, 1, 0)), d = hash13(i + vec3(1, 1, 0));
  float e = hash13(i + vec3(0, 0, 1)), f1 = hash13(i + vec3(1, 0, 1)), g = hash13(i + vec3(0, 1, 1)), h = hash13(i + vec3(1, 1, 1));
  return mix(mix(mix(a, b, u.x), mix(c, d, u.x), u.y), mix(mix(e, f1, u.x), mix(g, h, u.x), u.y), u.z);
}
float fbm(vec2 p, int oct) {
  float s = 0.0, a = 0.5;
  for (int i = 0; i < 8; ++i) {
    if (i >= oct) break;
    s += a * vnoise(p);
    p = mat2(1.6, 1.2, -1.2, 1.6) * p + 17.1;
    a *= 0.5;
  }
  return s;
}
float fbm3(vec3 p, int oct) {
  float s = 0.0, a = 0.5;
  for (int i = 0; i < 6; ++i) {
    if (i >= oct) break;
    s += a * vnoise3(p);
    p = p * 2.03 + 11.7;
    a *= 0.5;
  }
  return s;
}
// Cellular (Worley) distance: x = F1, y = F2.
vec2 worley(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float d1 = 8.0, d2 = 8.0;
  for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x) {
      vec2 g = vec2(x, y);
      vec2 o = hash22(i + g);
      float d = length(g + o - f);
      if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
    }
  return vec2(d1, d2);
}
// Wraps world coordinates to keep high-frequency noise precise far from the origin.
vec2 wrapCoord(vec2 p, float period) { return p - floor(p / period) * period; }

// --------------------------------------------------------------------------
//  Physically based atmosphere (Nishita single scattering)
// --------------------------------------------------------------------------
const float kEarthR = 6360e3;
const float kAtmoR = 6420e3;
const vec3 kBetaR = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const vec3 kBetaM = vec3(21e-6);
const float kHr = 7994.0;
const float kHm = 1200.0;

vec2 raySphere(vec3 ro, vec3 rd, float r) {
  float b = dot(ro, rd), c = dot(ro, ro) - r * r;
  float d = b * b - c;
  if (d < 0.0) return vec2(1e9, -1e9);
  d = sqrt(d);
  return vec2(-b - d, -b + d);
}

vec3 atmosphere(vec3 dir, vec3 sunDir, float sunIntensity) {
  vec3 ro = vec3(0.0, kEarthR + 50.0, 0.0);
  vec2 ta = raySphere(ro, dir, kAtmoR);
  float tmax = ta.y;
  vec2 tg = raySphere(ro, dir, kEarthR);
  if (tg.x > 0.0) tmax = min(tmax, tg.x);
  const int N = 16, NL = 6;
  float seg = tmax / float(N);
  float mu = dot(dir, sunDir);
  float phaseR = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
  const float g = 0.76;
  float phaseM = 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + mu * mu)) / ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * mu, 1.5));
  vec3 sumR = vec3(0), sumM = vec3(0);
  float odR = 0.0, odM = 0.0;
  for (int i = 0; i < N; ++i) {
    vec3 p = ro + dir * (seg * (float(i) + 0.5));
    float h = length(p) - kEarthR;
    float hr = exp(-h / kHr) * seg, hm = exp(-h / kHm) * seg;
    odR += hr; odM += hm;
    float tl = raySphere(p, sunDir, kAtmoR).y;
    float segL = tl / float(NL);
    float odRL = 0.0, odML = 0.0;
    bool blocked = false;
    for (int j = 0; j < NL; ++j) {
      vec3 q = p + sunDir * (segL * (float(j) + 0.5));
      float hl = length(q) - kEarthR;
      if (hl < 0.0) { blocked = true; break; }
      odRL += exp(-hl / kHr) * segL;
      odML += exp(-hl / kHm) * segL;
    }
    if (!blocked) {
      vec3 tau = kBetaR * (odR + odRL) + kBetaM * 1.1 * (odM + odML);
      vec3 att = exp(-tau);
      sumR += att * hr;
      sumM += att * hm;
    }
  }
  return sunIntensity * (sumR * kBetaR * phaseR + sumM * kBetaM * phaseM);
}

// --------------------------------------------------------------------------
//  Clouds: single layer, fbm coverage, cheap self-shadowing
// --------------------------------------------------------------------------
const float kCloudHeight = 1800.0;

float cloudDensityAt(vec2 xz) {
  vec2 p = xz * 0.00028 + uWind * uTime * 0.00028;
  float base = fbm(p, 6);
  float detail = fbm(p * 4.3 + 3.1, 4);
  float cov = mix(0.62, 0.30, uCloudCover);
  float d = smoothstep(cov, cov + 0.28, base * 0.8 + detail * 0.28);
  return d;
}

// Returns rgb = cloud radiance, a = coverage.
vec4 renderClouds(vec3 ro, vec3 dir, vec3 skyCol, vec3 sunDir, vec3 sunCol) {
  if (dir.y <= 0.01) return vec4(0.0);
  float t = (kCloudHeight - ro.y) / dir.y;
  vec2 xz = ro.xz + dir.xz * t;
  float d = cloudDensityAt(xz);
  if (d <= 0.001) return vec4(0.0);
  // Light march toward the sun through the slab.
  float shadow = 0.0;
  for (int i = 1; i <= 3; ++i) shadow += cloudDensityAt(xz + sunDir.xz / max(sunDir.y, 0.15) * 90.0 * float(i));
  float trans = exp(-shadow * 0.9);
  float mu = dot(dir, sunDir);
  float silver = pow(max(mu, 0.0), 12.0) * 2.5 + 0.6;
  vec3 lit = sunCol * trans * silver * 0.22 + skyCol * 0.9;
  vec3 dark = skyCol * 0.55 + sunCol * 0.02;
  vec3 col = mix(dark, lit, trans);
  float fade = smoothstep(0.01, 0.15, dir.y) * exp(-t * 0.000025);
  return vec4(col, d * fade);
}

// --------------------------------------------------------------------------
//  BRDF
// --------------------------------------------------------------------------
float D_GGX(float NoH, float a) {
  float a2 = a * a;
  float d = NoH * NoH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d + 1e-7);
}
float V_SmithGGX(float NoV, float NoL, float a) {
  float a2 = a * a;
  float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
  float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
  return 0.5 / (gv + gl + 1e-7);
}
vec3 F_Schlick(vec3 f0, float VoH) { return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0); }
// Karis' analytic approximation of the split-sum environment BRDF.
vec3 envBRDFApprox(vec3 f0, float rough, float NoV) {
  const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
  const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
  vec4 r = rough * c0 + c1;
  float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
  vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
  return f0 * ab.x + ab.y;
}

vec3 reconstructWorldPos(vec2 uv, float depth) {
  vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
  vec4 w = uInvViewProj * clip;
  return w.xyz / w.w;
}
