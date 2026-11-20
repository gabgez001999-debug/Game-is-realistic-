#include "common.glsl"

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aMat;
layout(location = 5) in vec4 aExtra;

uniform mat4 uModel;
uniform mat4 uPrevModel;
uniform mat4 uNormalMat;
uniform float uWindSway;   // vegetation sway amplitude

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;
flat out vec4 vMat;
out vec4 vExtra;
out vec3 vLocalPos;
out vec4 vCurClip;
out vec4 vPrevClip;

void main() {
  vec3 p = aPos;
  if (uWindSway > 0.0) {
    // Height-weighted sway for palms and foliage.
    float h = max(p.y, 0.0);
    vec2 sway = vec2(sin(uTime * 1.3 + uModel[3].x * 0.1), cos(uTime * 1.1 + uModel[3].z * 0.1)) * uWindSway * h * h * 0.0015;
    sway += vec2(sin(uTime * 4.7 + p.x * 3.0 + p.z * 2.0)) * uWindSway * 0.012 * h * 0.1;
    p.xz += sway;
  }
  vec4 world = uModel * vec4(p, 1.0);
  vec4 prevWorld = uPrevModel * vec4(p, 1.0);
  vWorldPos = world.xyz;
  vNormal = normalize(mat3(uNormalMat) * aNormal);
  vUV = aUV;
  vColor = aColor;
  vMat = aMat;
  vExtra = aExtra;
  vLocalPos = aPos;
  vCurClip = uViewProj * world;
  vPrevClip = uPrevViewProj * prevWorld;
  gl_Position = vCurClip;
}
