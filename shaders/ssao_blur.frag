in vec2 vUV;
uniform sampler2D uAO;
uniform sampler2D uDepth;
uniform vec2 uTexel;

out vec4 oAO;

float linDepth(float d) { return 1.0 / (1.0 - d + 1e-5); }

void main() {
  float c = linDepth(texture(uDepth, vUV).r);
  float sum = 0.0, wsum = 0.0;
  for (int y = -2; y <= 2; ++y)
    for (int x = -2; x <= 2; ++x) {
      vec2 o = vec2(x, y) * uTexel;
      float d = linDepth(texture(uDepth, vUV + o).r);
      float w = exp(-abs(d - c) / (c * 0.02 + 1e-4));
      sum += texture(uAO, vUV + o).r * w;
      wsum += w;
    }
  oAO = vec4(sum / max(wsum, 1e-4));
}
