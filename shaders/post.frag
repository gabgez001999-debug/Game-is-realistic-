#include "common.glsl"

in vec2 vUV;
uniform sampler2D uHDR;
uniform sampler2D uBloom;
uniform sampler2D uVelocity;
uniform sampler2D uDepth;
uniform sampler2D uExposure;
uniform float uBloomStrength;
uniform float uBloomNorm;      // the upsample chain sums all mips
uniform float uMotionBlur;      // shutter fraction (0 = off)
uniform float uExposureBias;
uniform float uVignette;
uniform float uGrain;
uniform float uDamageFlash;     // red/desaturated hit feedback

out vec4 oColor;

// ACES fitted (Stephen Hill).
vec3 RRTAndODTFit(vec3 v) {
  vec3 a = v * (v + 0.0245786) - 0.000090537;
  vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
  return a / b;
}
vec3 acesFitted(vec3 color) {
  const mat3 inputM = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
  const mat3 outputM = mat3(1.60475, -0.10208, -0.00327, -0.53108, 1.10813, -0.07276, -0.07367, -0.00605, 1.07602);
  color = inputM * color;
  color = RRTAndODTFit(color);
  return clamp(outputM * color, 0.0, 1.0);
}

void main() {
  vec2 vel = texture(uVelocity, vUV).xy;
  float depth = texture(uDepth, vUV).r;
  if (depth >= 1.0) {
    // Camera-only motion for the sky.
    vec3 P = reconstructWorldPos(vUV, 0.9999);
    vec4 prev = uPrevViewProj * vec4(P, 1.0);
    vel = (vUV * 2.0 - 1.0 - prev.xy / prev.w) * 0.5;
  }
  vec3 col = texture(uHDR, vUV).rgb;
  if (uMotionBlur > 0.0) {
    vec2 v = vel * uMotionBlur;
    float len = length(v * uResolution);
    if (len > 0.5) {
      v *= min(1.0, 40.0 / len);
      const int S = 10;
      vec3 acc = col;
      float jitter = hash12(gl_FragCoord.xy);
      for (int i = 1; i < S; ++i) {
        float t = (float(i) + jitter) / float(S) - 0.5;
        acc += texture(uHDR, vUV - v * t).rgb;
      }
      col = acc / float(S);
    }
  }
  vec3 bloom = texture(uBloom, vUV).rgb * uBloomNorm;
  col = mix(col, bloom, uBloomStrength);

  float avgLum = texture(uExposure, vec2(0.5)).r;
  float key = 1.03 - 2.0 / (2.0 + log(avgLum + 1.0) / log(10.0));
  key = clamp(key, 0.12, 0.2);
  float exposure = key / max(avgLum, 1e-4) * exp2(uExposureBias);
  exposure = clamp(exposure, 0.02, 60.0);
  col *= exposure;

  // White balance + grading: subtle warm highlights, cool shadows, filmic contrast.
  float l = luminance(col);
  col = mix(col, col * vec3(0.96, 1.0, 1.06), smoothstep(0.4, 0.0, l) * 0.35);
  col = mix(col, col * vec3(1.04, 1.0, 0.95), smoothstep(0.3, 1.5, l) * 0.3);
  col = acesFitted(col * 1.05);
  float sat = 1.06;
  col = mix(vec3(luminance(col)), col, sat);

  if (uDamageFlash > 0.0) {
    float g = luminance(col);
    col = mix(col, vec3(g) * vec3(1.15, 0.85, 0.85), uDamageFlash * 0.6);
  }
  // Vignette.
  vec2 q = vUV - 0.5;
  col *= 1.0 - dot(q, q) * uVignette;
  col = linearToSrgb(col);
  // Film grain.
  float n = hash12(gl_FragCoord.xy + fract(uTime * 13.7) * 1000.0) - 0.5;
  col += n * uGrain;
  oColor = vec4(clamp(col, 0.0, 1.0), dot(clamp(col, 0.0, 1.0), vec3(0.299, 0.587, 0.114)));
}
