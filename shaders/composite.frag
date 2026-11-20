#include "common.glsl"

// Screen-space reflections + aerial perspective / height fog.
in vec2 vUV;

uniform sampler2D uLit;
uniform sampler2D uEnvSpec;
uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gDepth;
uniform samplerCube uSkyCube;
uniform float uSkyMips;

out vec4 oColor;

vec3 viewPosAt(vec2 uv, float d) {
  vec4 c = uInvProj * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
  return c.xyz / c.w;
}

vec2 projectView(vec3 vp) {
  vec4 c = uProj * vec4(vp, 1.0);
  return c.xy / c.w * 0.5 + 0.5;
}

vec3 applyFog(vec3 col, vec3 P, vec3 V, float dist) {
  float h = max(P.y, 0.0);
  float camH = max(uCamPos.y, 0.0);
  float heightFactor = exp(-min(h, camH) * 0.004);
  float density = uFogDensity * (1.0 + uRain * 3.0);
  float fog = 1.0 - exp(-dist * density * heightFactor);
  vec3 dir = -V;
  vec3 fogCol = textureLod(uSkyCube, normalize(vec3(dir.x, max(dir.y, 0.02), dir.z)), uSkyMips - 3.0).rgb;
  float mu = max(dot(dir, uSunDir), 0.0);
  fogCol += uSunColor * pow(mu, 8.0) * 0.06;
  return mix(col, fogCol, clamp(fog, 0.0, 1.0));
}

void main() {
  float depth = texture(gDepth, vUV).r;
  vec3 lit = texture(uLit, vUV).rgb;
  if (depth >= 1.0) { oColor = vec4(lit, 1.0); return; }
  vec3 P = reconstructWorldPos(vUV, depth);
  vec3 V = normalize(uCamPos - P);
  float dist = length(uCamPos - P);
  vec4 n = texture(gNormal, vUV);
  float rough = n.a;
  vec3 color = lit;

  if (rough < 0.45 && dist < 400.0) {
    vec3 N = normalize(n.xyz);
    vec3 vp = viewPosAt(vUV, depth);
    vec3 vn = normalize(mat3(uView) * N);
    vec3 rd = normalize(reflect(normalize(vp), vn));
    if (rd.z < 0.6) {
      float stepLen = 0.25 + dist * 0.01;
      float jitter = hash12(gl_FragCoord.xy + fract(uTime) * 100.0);
      vec3 ray = vp + vn * 0.05 + rd * stepLen * jitter;
      vec2 hitUV = vec2(-1.0);
      float thickness = 0.6 + dist * 0.02;
      for (int i = 0; i < 40; ++i) {
        ray += rd * stepLen;
        stepLen *= 1.08;
        vec2 uv = projectView(ray);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || ray.z > -0.05) break;
        float sd = texture(gDepth, uv).r;
        if (sd >= 1.0) continue;
        float sz = viewPosAt(uv, sd).z;
        float diff = sz - ray.z;
        if (diff > 0.0 && diff < thickness + stepLen) {
          // Binary refinement.
          vec3 lo = ray - rd * stepLen, hi = ray;
          for (int j = 0; j < 6; ++j) {
            vec3 mid = (lo + hi) * 0.5;
            vec2 muv = projectView(mid);
            float mz = viewPosAt(muv, texture(gDepth, muv).r).z;
            if (mz - mid.z > 0.0) hi = mid; else lo = mid;
          }
          hitUV = projectView(hi);
          break;
        }
      }
      if (hitUV.x >= 0.0) {
        vec2 edge = smoothstep(vec2(0.0), vec2(0.08), hitUV) * smoothstep(vec2(1.0), vec2(0.92), hitUV);
        float fade = edge.x * edge.y * smoothstep(0.45, 0.15, rough) * smoothstep(0.6, 0.2, rd.z);
        vec3 hitCol = textureLod(uLit, hitUV, 0.0).rgb;
        vec4 a = texture(gAlbedo, vUV);
        vec3 f0 = mix(vec3(0.04), a.rgb, a.a);
        vec3 F = envBRDFApprox(f0, rough, max(dot(N, V), 1e-3));
        vec3 envSpec = texture(uEnvSpec, vUV).rgb;
        color = color - envSpec * fade + hitCol * F * fade;
      }
    }
  }
  color = applyFog(color, P, V, dist);
  oColor = vec4(max(color, vec3(0.0)), 1.0);
}
