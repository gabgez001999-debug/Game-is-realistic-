#include "common.glsl"

in vec2 vUV;
uniform sampler2D uSrc;
out vec4 oLum;

void main() {
  // Center-weighted metering.
  vec3 c = texture(uSrc, vUV).rgb;
  float l = log(max(luminance(c), 1e-5));
  oLum = vec4(l, 0.0, 0.0, 1.0);
}
