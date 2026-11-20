#include "common.glsl"

layout(location = 0) in vec4 aPosSize;   // xyz world, w size
layout(location = 1) in vec4 aColor;     // rgba (linear, premultiplied for additive)
layout(location = 2) in vec4 aParams;    // x rotation, y type (0 smoke, 1 spark, 2 shard, 3 rain), z stretch, w emissive
layout(location = 3) in vec3 aVelocity;

out vec2 vUV;
out vec4 vColor;
out vec4 vParams;
out vec3 vWorldPos;
out vec4 vClip;

void main() {
  int corner = gl_VertexID % 6;
  vec2 offs[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
  vec2 o = offs[corner];
  vec3 right = vec3(uView[0][0], uView[1][0], uView[2][0]);
  vec3 up = vec3(uView[0][1], uView[1][1], uView[2][1]);
  float c = cos(aParams.x), s = sin(aParams.x);
  vec2 ro = vec2(c * o.x - s * o.y, s * o.x + c * o.y);
  vec3 pos = aPosSize.xyz;
  vec3 wp;
  if (aParams.z > 0.0 && length(aVelocity) > 0.01) {
    // Velocity-stretched streak (sparks, rain).
    vec3 dir = normalize(aVelocity);
    vec3 toCam = normalize(uCamPos - pos);
    vec3 side = normalize(cross(dir, toCam));
    wp = pos + side * o.x * aPosSize.w + dir * o.y * (aPosSize.w + length(aVelocity) * aParams.z);
  } else {
    wp = pos + (right * ro.x + up * ro.y) * aPosSize.w;
  }
  vUV = o * 0.5 + 0.5;
  vColor = aColor;
  vParams = aParams;
  vWorldPos = wp;
  vClip = uViewProj * vec4(wp, 1.0);
  gl_Position = vClip;
}
