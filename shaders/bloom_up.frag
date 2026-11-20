// 3x3 tent upsample, blended additively onto the next larger mip.
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uSrcTexel;
uniform float uRadius;
out vec4 oColor;

void main() {
  vec2 t = uSrcTexel * uRadius;
  vec3 s = texture(uSrc, vUV + vec2(-t.x, t.y)).rgb + texture(uSrc, vUV + vec2(0, t.y)).rgb * 2.0 + texture(uSrc, vUV + t).rgb;
  s += texture(uSrc, vUV + vec2(-t.x, 0)).rgb * 2.0 + texture(uSrc, vUV).rgb * 4.0 + texture(uSrc, vUV + vec2(t.x, 0)).rgb * 2.0;
  s += texture(uSrc, vUV - t).rgb + texture(uSrc, vUV + vec2(0, -t.y)).rgb * 2.0 + texture(uSrc, vUV + vec2(t.x, -t.y)).rgb;
  oColor = vec4(s / 16.0, 1.0);
}
