in vec2 vUV;
flat in float vMatId;

void main() {
  if (int(vMatId + 0.5) == 15) {
    float along = vUV.y;
    float across = abs(vUV.x - 0.5) * 2.0;
    float width = (1.0 - along * 0.75) * smoothstep(0.0, 0.08, along);
    if (across > width * 0.85 && across > 0.04) discard;
  }
}
