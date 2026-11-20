// Converts the final RGB frame into a packed I420 image (Y plane, then U, then V)
// stored in a single-channel texture of size W x (H * 3/2), BT.709 limited range.
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uSize;  // W, H of the video frame
out vec4 oValue;

void main() {
  float W = uSize.x, H = uSize.y;
  ivec2 p = ivec2(gl_FragCoord.xy);
  int row = p.y;
  if (row < int(H)) {
    vec3 c = texture(uSrc, (vec2(p.x, H - 1.0 - float(row)) + 0.5) / uSize).rgb;  // top row first
    float y = 0.0627 + dot(c, vec3(0.1826, 0.6142, 0.0620));
    oValue = vec4(y);
    return;
  }
  // Chroma: each output row holds two chroma rows of width W/2.
  int crow = row - int(H);
  int planeRows = int(H) / 4;
  bool isV = crow >= planeRows;
  if (isV) crow -= planeRows;
  int idx = crow * int(W) + p.x;
  int cw = int(W) / 2;
  int cx = idx % cw, cy = idx / cw;
  vec2 uv = (vec2(cx, cy) * 2.0 + 1.0) / uSize;
  uv.y = 1.0 - uv.y;
  vec3 c = texture(uSrc, uv).rgb;
  float u = 0.5 + dot(c, vec3(-0.1006, -0.3386, 0.4392));
  float v = 0.5 + dot(c, vec3(0.4392, -0.3989, -0.0403));
  oValue = vec4(isV ? v : u);
}
