#include "common.glsl"

in vec2 vUV;

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gEmissive;
uniform sampler2D gDepth;
uniform sampler2D uAO;
uniform sampler2DArrayShadow uShadowMap;
uniform samplerCube uSkyCube;
uniform sampler2D uHeightMap;

uniform mat4 uShadowMats[4];
uniform vec4 uCascadeFar;       // view distance where each cascade ends
uniform vec4 uCascadeTexel;     // world size of one shadow texel per cascade
uniform vec4 uHeightMapInfo;    // origin.xz, size.xz
uniform float uSkyMips;
uniform float uSunDiscIntensity;

const int MAX_LIGHTS = 64;
uniform int uLightCount;
uniform vec4 uLightPos[MAX_LIGHTS];    // xyz position, w range
uniform vec4 uLightColor[MAX_LIGHTS];  // rgb intensity, w spot outer cos (or -2 for point)
uniform vec4 uLightDir[MAX_LIGHTS];    // xyz direction, w spot inner cos

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oEnvSpec;

float shadowCascade(int c, vec3 P, vec3 N, float NoL) {
  vec3 offsetP = P + N * uCascadeTexel[c] * (1.5 + 2.0 * (1.0 - NoL));
  vec4 lp = uShadowMats[c] * vec4(offsetP, 1.0);
  vec3 sc = lp.xyz / lp.w * 0.5 + 0.5;
  if (sc.x <= 0.001 || sc.x >= 0.999 || sc.y <= 0.001 || sc.y >= 0.999 || sc.z >= 1.0) return -1.0;
  float bias = 0.0003 * float(c + 1);
  vec2 texel = vec2(1.0 / 2048.0);
  float sum = 0.0;
  // 16-tap rotated Poisson-ish PCF.
  float ang = hash12(gl_FragCoord.xy) * 6.2831;
  mat2 rot = mat2(cos(ang), sin(ang), -sin(ang), cos(ang));
  const vec2 taps[12] = vec2[](vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
                              vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
                              vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));
  float radius = c == 0 ? 1.6 : 1.2;
  for (int i = 0; i < 12; ++i) {
    vec2 o = rot * taps[i] * texel * radius;
    sum += texture(uShadowMap, vec4(sc.xy + o, float(c), sc.z - bias));
  }
  return sum / 12.0;
}

float sunShadow(vec3 P, vec3 N, float viewDist, float NoL) {
  for (int c = 0; c < 4; ++c) {
    if (viewDist < uCascadeFar[c]) {
      float s = shadowCascade(c, P, N, NoL);
      if (s < 0.0) continue;
      // Blend into the next cascade near the boundary.
      float edge = uCascadeFar[c] * 0.88;
      if (c < 3 && viewDist > edge) {
        float s2 = shadowCascade(c + 1, P, N, NoL);
        if (s2 >= 0.0) s = mix(s, s2, smoothstep(edge, uCascadeFar[c], viewDist));
      }
      return s;
    }
  }
  return 1.0;
}

float cloudShadow(vec3 P) {
  if (uSunDir.y < 0.05) return 1.0;
  vec2 xz = P.xz + uSunDir.xz / uSunDir.y * (kCloudHeight - P.y);
  return 1.0 - cloudDensityAt(xz) * 0.8;
}

float heightAt(vec2 xz) {
  return texture(uHeightMap, (xz - uHeightMapInfo.xy) / uHeightMapInfo.zw).r;
}

// Horizon-based sky visibility from the city heightmap (street canyons).
float skyVisibility(vec3 P, vec3 N) {
  if (uHeightMapInfo.z <= 0.0) return 1.0;
  // Step off the surface first: the heightmap texel (4 m) would otherwise contain our own wall.
  vec2 nh = N.xz;
  if (dot(nh, nh) > 0.25) P.xz += normalize(nh) * 4.5;
  P.y += 0.5;
  float vis = 0.0, wsum = 0.0;
  for (int i = 0; i < 8; ++i) {
    float a = float(i) * (PI / 4.0) + 0.3;
    vec2 dir = vec2(cos(a), sin(a));
    float maxSin = 0.0;
    for (int k = 0; k < 4; ++k) {
      float dist = 3.0 * pow(2.3, float(k));
      float dh = heightAt(P.xz + dir * dist) - P.y;
      maxSin = max(maxSin, dh / sqrt(dh * dh + dist * dist));
    }
    float w = max(0.15, 0.5 + 0.5 * dot(vec3(dir.x, 0.0, dir.y), N));
    vis += (1.0 - maxSin) * w;
    wsum += w;
  }
  return mix(vis / wsum, 1.0, max(N.y, 0.0) * 0.5);
}

// True if a reflection ray leaving P along R is blocked by buildings.
float reflectionVisibility(vec3 P, vec3 N, vec3 R) {
  if (uHeightMapInfo.z <= 0.0) return 1.0;
  vec2 nh = N.xz;
  if (dot(nh, nh) > 0.25) P.xz += normalize(nh) * 4.5;
  vec2 d = normalize(R.xz + vec2(1e-5));
  float horiz = length(R.xz);
  float vis = 1.0;
  for (int k = 0; k < 5; ++k) {
    float dist = 3.0 * pow(2.2, float(k));
    float rayH = P.y + R.y / max(horiz, 1e-3) * dist;
    float bh = heightAt(P.xz + d * dist);
    vis = min(vis, smoothstep(-0.5, 2.0, rayH - bh));
  }
  return vis;
}

void main() {
  float depth = texture(gDepth, vUV).r;
  vec3 P = reconstructWorldPos(vUV, depth);
  vec3 V = normalize(uCamPos - P);
  if (depth >= 1.0) {
    vec3 dir = -V;
    vec3 sky = textureLod(uSkyCube, dir, 0.0).rgb;
    // Sun disc with limb darkening.
    float mu = dot(dir, uTrueSunDir);
    float disc = smoothstep(0.99997, 0.999985, mu);
    float limb = sqrt(max(1.0 - (1.0 - mu) / (1.0 - 0.99997), 0.0));
    sky += uSunColor * disc * uSunDiscIntensity * mix(0.6, 1.0, limb) * step(0.0, uTrueSunDir.y);
    // Full-resolution clouds over the cubemap sky.
    vec4 cl = renderClouds(vec3(0.0), dir, textureLod(uSkyCube, vec3(0, 1, 0), 5.0).rgb, uSunDir, uSunColor);
    sky = mix(sky, cl.rgb, cl.a * 0.85);
    // Stars.
    if (uNight > 0.01 && dir.y > 0.0) {
      vec2 sp = vec2(atan(dir.z, dir.x), asin(dir.y)) * 300.0;
      float st = step(0.997, hash12(floor(sp)));
      float tw = 0.6 + 0.4 * sin(uTime * 3.0 + hash12(floor(sp)) * 50.0);
      sky += vec3(st * tw * 0.05) * uNight * (1.0 - cl.a) * smoothstep(0.0, 0.2, dir.y);
      // Moon.
      float moon = smoothstep(0.99985, 0.9999, dot(dir, uSunDir));
      sky += vec3(0.9, 0.93, 1.0) * moon * 0.6 * uNight;
    }
    oColor = vec4(sky, 1.0);
    oEnvSpec = vec4(0.0);
    return;
  }

  vec4 a = texture(gAlbedo, vUV);
  vec4 n = texture(gNormal, vUV);
  vec4 e = texture(gEmissive, vUV);
  vec3 albedo = a.rgb;
  float metal = a.a;
  vec3 N = normalize(n.xyz);
  float rough = n.a;
  float clearcoat = e.a;
  float ao = texture(uAO, vUV).r;

  float NoV = max(dot(N, V), 1e-4);
  vec3 f0 = mix(vec3(0.04), albedo, metal);
  vec3 diffColor = albedo * (1.0 - metal);
  float alpha = rough * rough;
  float viewDist = -(uView * vec4(P, 1.0)).z;  // view depth selects the cascade

  vec3 color = vec3(0.0);

  // --- Sun / moon ---------------------------------------------------------
  vec3 L = uSunDir;
  float NoL = dot(N, L);
  if (NoL > 0.0) {
    float sh = sunShadow(P, N, viewDist, NoL) * cloudShadow(P);
    vec3 H = normalize(L + V);
    float NoH = max(dot(N, H), 0.0), VoH = max(dot(V, H), 0.0);
    vec3 F = F_Schlick(f0, VoH);
    vec3 spec = D_GGX(NoH, alpha) * V_SmithGGX(NoV, NoL, alpha) * F;
    vec3 diff = diffColor / PI * (1.0 - F);
    vec3 lit = (diff + spec) * NoL;
    if (clearcoat > 0.0) {
      float Fc = 0.04 + 0.96 * pow(1.0 - VoH, 5.0);
      float cc = D_GGX(NoH, 0.04 * 0.04 + 0.002) * V_SmithGGX(NoV, NoL, 0.05) * Fc * clearcoat;
      lit = lit * (1.0 - Fc * clearcoat) + cc * NoL;
    }
    color += lit * uSunColor * sh;
  }

  // --- Local lights (street lamps, headlights) -----------------------------
  for (int i = 0; i < MAX_LIGHTS; ++i) {
    if (i >= uLightCount) break;
    vec3 lv = uLightPos[i].xyz - P;
    float d2 = dot(lv, lv);
    float range = uLightPos[i].w;
    if (d2 > range * range) continue;
    float d = sqrt(d2);
    vec3 Ll = lv / d;
    float nl = dot(N, Ll);
    if (nl <= 0.0) continue;
    float win = pow(clamp(1.0 - pow(d / range, 4.0), 0.0, 1.0), 2.0);
    float att = win / (d2 + 1.0);
    float outer = uLightColor[i].w;
    if (outer > -1.5) {
      float cd = dot(-Ll, uLightDir[i].xyz);
      att *= smoothstep(outer, uLightDir[i].w, cd);
    }
    if (att <= 0.0) continue;
    vec3 H = normalize(Ll + V);
    float NoH = max(dot(N, H), 0.0), VoH = max(dot(V, H), 0.0);
    vec3 F = F_Schlick(f0, VoH);
    vec3 spec = D_GGX(NoH, max(alpha, 0.02)) * V_SmithGGX(NoV, nl, alpha) * F;
    color += (diffColor / PI * (1.0 - F) + spec) * nl * uLightColor[i].rgb * att;
  }

  // --- Ambient (sky irradiance with canyon occlusion) ----------------------
  float skyVis = skyVisibility(P, N);
  vec3 irr = textureLod(uSkyCube, N, uSkyMips - 1.5).rgb * PI;
  vec3 bounce = (uSunColor * 0.08 * max(uSunDir.y, 0.0) + textureLod(uSkyCube, vec3(0, 1, 0), uSkyMips - 1.0).rgb * 0.5) * 0.35;
  vec3 ambientIrr = irr * skyVis + bounce * (1.0 - skyVis);
  vec3 envF = envBRDFApprox(f0, rough, NoV);
  color += diffColor / PI * ambientIrr * ao * (1.0 - envF * 0.5);

  // --- Environment specular -------------------------------------------------
  vec3 R = reflect(-V, N);
  float lod = rough * (uSkyMips - 2.0);
  vec3 env = textureLod(uSkyCube, R, lod).rgb;
  float rvis = reflectionVisibility(P, N, R);
  vec3 occluder = bounce * 0.6;
  env = mix(occluder, env, mix(rvis, 1.0, rough * 0.5));
  float specOcc = clamp(pow(NoV + ao, exp2(-16.0 * rough - 1.0)) - 1.0 + ao, 0.0, 1.0);
  vec3 envSpec = env * envF * specOcc;
  if (clearcoat > 0.0) {
    vec3 ccEnv = textureLod(uSkyCube, R, 0.5).rgb;
    ccEnv = mix(occluder, ccEnv, rvis);
    float Fc = 0.04 + 0.96 * pow(1.0 - NoV, 5.0);
    envSpec = envSpec * (1.0 - Fc * clearcoat) + ccEnv * Fc * clearcoat * specOcc;
  }
  color += envSpec;
  color += e.rgb;
  oColor = vec4(color, 1.0);
  oEnvSpec = vec4(envSpec, 1.0);
}
