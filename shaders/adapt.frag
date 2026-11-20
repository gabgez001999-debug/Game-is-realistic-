in vec2 vUV;
uniform sampler2D uLum;
uniform sampler2D uPrev;
uniform float uLumMip;
uniform float uDt;
uniform float uReset;
out vec4 oValue;

void main() {
  // Average of log-luminance from the smallest mip, weighted towards the screen center.
  float avg = textureLod(uLum, vec2(0.5), uLumMip).r * 0.6 + textureLod(uLum, vec2(0.5), uLumMip - 2.0).r * 0.4;
  float cur = exp(avg);
  float prev = texture(uPrev, vec2(0.5)).r;
  if (uReset > 0.5 || prev <= 0.0 || isnan(prev)) prev = cur;
  float speed = cur > prev ? 1.8 : 1.1;  // adapt to bright faster than to dark (like the eye)
  float v = prev + (cur - prev) * (1.0 - exp(-uDt * speed));
  oValue = vec4(v, 0.0, 0.0, 1.0);
}
