#include "common.glsl"

in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vClip;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform samplerCube uSkyCube;
uniform float uSkyMips;

out vec4 oColor;

float linearDepth(float d) {
  vec4 c = uInvProj * vec4(0.0, 0.0, d * 2.0 - 1.0, 1.0);
  return -c.z / c.w;
}

void main() {
  vec2 uv = vClip.xy / vClip.w * 0.5 + 0.5;
  vec3 V = normalize(uCamPos - vWorldPos);
  float dist = length(uCamPos - vWorldPos);
  // Detail normals: two scrolling noise layers.
  vec2 w = wrapCoord(vWorldPos.xz, 512.0);
  float n1 = vnoise(w * 0.9 + uTime * vec2(0.3, 0.2));
  float n2 = vnoise(w * 2.3 - uTime * vec2(0.2, 0.45));
  vec3 N = normalize(vNormal + vec3(n1 - 0.5, 0.0, n2 - 0.5) * 0.35 * exp(-dist * 0.004));
  float NoV = max(dot(N, V), 0.0);
  float F = 0.02 + 0.98 * pow(1.0 - NoV, 5.0);

  vec3 R = reflect(-V, N);
  R.y = abs(R.y);
  vec3 refl = textureLod(uSkyCube, R, 1.0).rgb;
  // Screen-space planar-ish reflection: mirror the sample around the water line.
  vec2 ruv = uv + N.xz * 0.03;
  vec4 mirClip = uViewProj * vec4(vWorldPos.x + R.x * 30.0, vWorldPos.y + R.y * 30.0, vWorldPos.z + R.z * 30.0, 1.0);
  vec2 muv = mirClip.xy / mirClip.w * 0.5 + 0.5;
  if (muv.x > 0.0 && muv.x < 1.0 && muv.y > 0.0 && muv.y < 1.0 && texture(uSceneDepth, muv).r < 1.0) {
    float e = smoothstep(0.0, 0.1, muv.y) * smoothstep(1.0, 0.9, muv.y) * smoothstep(0.0, 0.1, muv.x) * smoothstep(1.0, 0.9, muv.x);
    refl = mix(refl, texture(uSceneColor, muv).rgb, e * 0.6);
  }
  // Refraction with depth-based absorption.
  float sceneD = linearDepth(texture(uSceneDepth, clamp(ruv, 0.001, 0.999)).r);
  float waterD = linearDepth(gl_FragCoord.z);
  float thick = max(sceneD - waterD, 0.0);
  vec3 below = texture(uSceneColor, clamp(ruv, 0.001, 0.999)).rgb;
  vec3 absorb = exp(-vec3(0.45, 0.09, 0.06) * thick);
  vec3 deep = vec3(0.004, 0.025, 0.035) * (luminance(uSunColor) * 0.2 + textureLod(uSkyCube, vec3(0, 1, 0), uSkyMips - 1.0).rgb * 0.8);
  vec3 refr = mix(deep, below, absorb);
  // Shore foam.
  float foam = smoothstep(1.2, 0.0, thick) * (0.6 + 0.4 * vnoise(w * 3.0 + uTime));
  foam += smoothstep(0.55, 0.85, vWorldPos.y) * 0.4 * vnoise(w * 1.5 - uTime * 0.5);
  vec3 foamCol = (uSunColor * max(uSunDir.y, 0.0) * 0.15 + textureLod(uSkyCube, vec3(0, 1, 0), uSkyMips - 1.0).rgb) * 0.8;
  // Sun glint.
  vec3 H = normalize(uSunDir + V);
  float spec = D_GGX(max(dot(N, H), 0.0), 0.04) * 0.25;
  vec3 col = mix(refr, refl, F) + uSunColor * spec * F * 4.0;
  col = mix(col, foamCol, clamp(foam, 0.0, 1.0) * 0.8);
  // Fog.
  float fog = 1.0 - exp(-dist * uFogDensity * (1.0 + uRain * 3.0));
  vec3 fogCol = textureLod(uSkyCube, normalize(vec3(-V.x, 0.05, -V.z)), uSkyMips - 3.0).rgb;
  col = mix(col, fogCol, fog);
  float edgeAlpha = smoothstep(0.0, 0.15, thick);
  oColor = vec4(col, edgeAlpha);
}
