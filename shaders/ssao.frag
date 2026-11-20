#include "common.glsl"

in vec2 vUV;
uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform float uRadius;

out vec4 oAO;

vec3 viewPos(vec2 uv) {
  float d = texture(uDepth, uv).r;
  vec4 c = uInvProj * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
  return c.xyz / c.w;
}

void main() {
  float depth = texture(uDepth, vUV).r;
  if (depth >= 1.0) { oAO = vec4(1.0); return; }
  vec3 P = viewPos(vUV);
  vec3 N = normalize(mat3(uView) * texture(uNormal, vUV).xyz);
  float rnd = hash12(gl_FragCoord.xy) * 6.2831;
  vec3 rv = vec3(cos(rnd), sin(rnd), 0.0);
  vec3 T = normalize(rv - N * dot(rv, N));
  vec3 B = cross(N, T);
  mat3 TBN = mat3(T, B, N);
  float radius = uRadius * clamp(-P.z * 0.04 + 0.6, 0.6, 3.0);
  float occ = 0.0;
  const int S = 12;
  for (int i = 0; i < S; ++i) {
    float fi = float(i);
    vec3 h = vec3(hash12(vec2(fi, 1.3)) * 2.0 - 1.0, hash12(vec2(fi, 7.1)) * 2.0 - 1.0, hash12(vec2(fi, 3.7)) * 0.9 + 0.1);
    h = normalize(h) * mix(0.1, 1.0, pow((fi + 1.0) / float(S), 2.0));
    vec3 sp = P + TBN * h * radius;
    vec4 clip = uProj * vec4(sp, 1.0);
    vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
    if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
    float sz = viewPos(suv).z;
    float range = smoothstep(0.0, 1.0, radius / abs(P.z - sz));
    occ += (sz >= sp.z + 0.03 ? 1.0 : 0.0) * range;
  }
  float ao = 1.0 - occ / float(S);
  oAO = vec4(pow(clamp(ao, 0.0, 1.0), 1.6));
}
