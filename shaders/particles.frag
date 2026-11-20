#include "common.glsl"

in vec2 vUV;
in vec4 vColor;
in vec4 vParams;
in vec3 vWorldPos;
in vec4 vClip;

uniform sampler2D uSceneDepth;
uniform samplerCube uSkyCube;
uniform float uSkyMips;

out vec4 oColor;

float linearDepth(float d) {
  vec4 c = uInvProj * vec4(0.0, 0.0, d * 2.0 - 1.0, 1.0);
  return -c.z / c.w;
}

void main() {
  vec2 q = vUV * 2.0 - 1.0;
  float r = length(q);
  int type = int(vParams.y + 0.5);
  vec2 suv = vClip.xy / vClip.w * 0.5 + 0.5;
  float sceneD = linearDepth(texture(uSceneDepth, suv).r);
  float myD = linearDepth(gl_FragCoord.z);
  float soft = clamp((sceneD - myD) * 2.0, 0.0, 1.0);
  if (type == 0) {
    // Smoke / dust: noisy puff lit by sun + sky.
    float n = fbm(vUV * 3.0 + vParams.x * 3.0, 4);
    float a = smoothstep(1.0, 0.2, r + (n - 0.5) * 0.7) * vColor.a * soft;
    vec3 amb = textureLod(uSkyCube, vec3(0, 1, 0), uSkyMips - 1.0).rgb * PI;
    float sunTerm = 0.4 + 0.6 * clamp(dot(normalize(vec3(q, 0.6)), vec3(0.0, 1.0, 0.3)), 0.0, 1.0);
    vec3 lit = vColor.rgb * (amb * 0.35 + uSunColor * sunTerm * 0.12);
    lit += vColor.rgb * vParams.w;  // fire glow
    oColor = vec4(lit * a, a);
  } else if (type == 1) {
    // Hot spark / ember (additive).
    float a = smoothstep(1.0, 0.0, r) * soft;
    oColor = vec4(vColor.rgb * vParams.w * a, 0.0);
  } else if (type == 2) {
    // Glass shard glint.
    float a = smoothstep(1.0, 0.3, max(abs(q.x), abs(q.y))) * vColor.a * soft;
    vec3 V = normalize(uCamPos - vWorldPos);
    float glint = pow(max(dot(reflect(-uSunDir, normalize(vec3(sin(vParams.x * 7.0), 1.0, cos(vParams.x * 5.0)))), V), 0.0), 40.0);
    vec3 c = textureLod(uSkyCube, vec3(0, 1, 0), 2.0).rgb * 0.6 + uSunColor * glint * 3.0;
    oColor = vec4(c * a, a * 0.7);
  } else {
    // Rain streak.
    float a = smoothstep(1.0, 0.0, abs(q.x)) * vColor.a * soft;
    vec3 c = textureLod(uSkyCube, vec3(0, 1, 0), uSkyMips - 1.0).rgb * 2.0 + vec3(0.02);
    oColor = vec4(c * a, a);
  }
}
