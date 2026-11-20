#include "common.glsl"

in vec2 vUV;
uniform int uFace;
uniform float uSunIntensity;
uniform vec3 uGroundColor;

out vec4 oColor;

vec3 faceDir(int face, vec2 uv) {
  vec2 p = uv * 2.0 - 1.0;
  if (face == 0) return normalize(vec3(1.0, -p.y, -p.x));
  if (face == 1) return normalize(vec3(-1.0, -p.y, p.x));
  if (face == 2) return normalize(vec3(p.x, 1.0, p.y));
  if (face == 3) return normalize(vec3(p.x, -1.0, -p.y));
  if (face == 4) return normalize(vec3(p.x, -p.y, 1.0));
  return normalize(vec3(-p.x, -p.y, -1.0));
}

vec3 skyRadiance(vec3 dir) {
  vec3 d = dir;
  d.y = max(d.y, 0.0);
  d = normalize(d + vec3(0.0, 0.002, 0.0));
  vec3 sky = atmosphere(d, uTrueSunDir, uSunIntensity);
  // Moonlit night sky + orange city light pollution near the horizon.
  vec3 night = vec3(0.0012, 0.0022, 0.0045) + vec3(0.02, 0.011, 0.004) * exp(-max(dir.y, 0.0) * 9.0);
  sky += night * uNight;
  return sky;
}

void main() {
  vec3 dir = faceDir(uFace, vUV);
  vec3 col;
  if (dir.y >= 0.0) {
    col = skyRadiance(dir);
    vec4 cl = renderClouds(vec3(0.0, 0.0, 0.0), dir, skyRadiance(vec3(0, 1, 0)) * 0.7 + col * 0.3, uSunDir, uSunColor);
    col = mix(col, cl.rgb, cl.a);
    col *= mix(1.0, 0.55, uCloudCover * uRain);
  } else {
    // Lower hemisphere: ground lit by sun + sky (used for ambient bounce and reflections).
    vec3 horizon = skyRadiance(normalize(vec3(dir.x, 0.0, dir.z) + vec3(0.0, 0.001, 0.0)));
    col = mix(horizon, uGroundColor, smoothstep(0.0, -0.25, dir.y));
  }
  oColor = vec4(col, 1.0);
}
