layout(location = 0) in vec2 aPos;    // pixels
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aParams;  // x = type (0 solid, 1 glyph, 2 circle, 3 radar), yzw = shape params

uniform vec2 uScreen;

out vec2 vUV;
out vec4 vColor;
flat out vec4 vParams;
out vec2 vPix;

void main() {
  vUV = aUV;
  vColor = aColor;
  vParams = aParams;
  vPix = aPos;
  gl_Position = vec4(aPos / uScreen * 2.0 - 1.0, 0.0, 1.0);
  gl_Position.y = -gl_Position.y;
}
