#include "common.glsl"

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;
layout(location = 4) in vec4 aMat;

uniform mat4 uModel;
uniform mat4 uLightViewProj;
uniform float uWindSway;

out vec2 vUV;
flat out float vMatId;

void main() {
  vec3 p = aPos;
  if (uWindSway > 0.0) {
    float h = max(p.y, 0.0);
    p.xz += vec2(sin(uTime * 1.3 + uModel[3].x * 0.1), cos(uTime * 1.1 + uModel[3].z * 0.1)) * uWindSway * h * h * 0.0015;
  }
  vUV = aUV;
  vMatId = aMat.x;
  gl_Position = uLightViewProj * uModel * vec4(p, 1.0);
}
