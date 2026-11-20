#include "common.glsl"

// 13-tap downsample (Jimenez, CoD:AW) with Karis average on the first mip.
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uSrcTexel;
uniform int uFirst;
out vec4 oColor;

vec3 karis(vec3 a, vec3 b, vec3 c, vec3 d) {
  float wa = 1.0 / (1.0 + luminance(a)), wb = 1.0 / (1.0 + luminance(b));
  float wc = 1.0 / (1.0 + luminance(c)), wd = 1.0 / (1.0 + luminance(d));
  return (a * wa + b * wb + c * wc + d * wd) / (wa + wb + wc + wd);
}

void main() {
  vec2 t = uSrcTexel;
  vec3 a = texture(uSrc, vUV + t * vec2(-2, 2)).rgb, b = texture(uSrc, vUV + t * vec2(0, 2)).rgb, c = texture(uSrc, vUV + t * vec2(2, 2)).rgb;
  vec3 d = texture(uSrc, vUV + t * vec2(-2, 0)).rgb, e = texture(uSrc, vUV).rgb, f = texture(uSrc, vUV + t * vec2(2, 0)).rgb;
  vec3 g = texture(uSrc, vUV + t * vec2(-2, -2)).rgb, h = texture(uSrc, vUV + t * vec2(0, -2)).rgb, i = texture(uSrc, vUV + t * vec2(2, -2)).rgb;
  vec3 j = texture(uSrc, vUV + t * vec2(-1, 1)).rgb, k = texture(uSrc, vUV + t * vec2(1, 1)).rgb;
  vec3 l = texture(uSrc, vUV + t * vec2(-1, -1)).rgb, m = texture(uSrc, vUV + t * vec2(1, -1)).rgb;
  vec3 col;
  if (uFirst == 1) {
    col = karis(j, k, l, m) * 0.5 + karis(a, b, d, e) * 0.125 + karis(b, c, e, f) * 0.125 + karis(d, e, g, h) * 0.125 + karis(e, f, h, i) * 0.125;
  } else {
    col = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
  }
  oColor = vec4(min(col, vec3(6e4)), 1.0);
}
