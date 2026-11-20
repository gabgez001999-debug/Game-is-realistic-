// FXAA 3.11 (quality preset ~12), luma in alpha.
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;
out vec4 oColor;

void main() {
  vec4 rgbM = texture(uSrc, vUV);
  float lumaM = rgbM.a;
  float lumaN = textureOffset(uSrc, vUV, ivec2(0, 1)).a;
  float lumaS = textureOffset(uSrc, vUV, ivec2(0, -1)).a;
  float lumaE = textureOffset(uSrc, vUV, ivec2(1, 0)).a;
  float lumaW = textureOffset(uSrc, vUV, ivec2(-1, 0)).a;
  float rangeMax = max(max(lumaN, lumaS), max(max(lumaE, lumaW), lumaM));
  float rangeMin = min(min(lumaN, lumaS), min(min(lumaE, lumaW), lumaM));
  float range = rangeMax - rangeMin;
  if (range < max(0.0312, rangeMax * 0.125)) { oColor = vec4(rgbM.rgb, 1.0); return; }
  float lumaNW = textureOffset(uSrc, vUV, ivec2(-1, 1)).a;
  float lumaNE = textureOffset(uSrc, vUV, ivec2(1, 1)).a;
  float lumaSW = textureOffset(uSrc, vUV, ivec2(-1, -1)).a;
  float lumaSE = textureOffset(uSrc, vUV, ivec2(1, -1)).a;
  float edgeH = abs(lumaNW + lumaNE - 2.0 * lumaN) + 2.0 * abs(lumaW + lumaE - 2.0 * lumaM) + abs(lumaSW + lumaSE - 2.0 * lumaS);
  float edgeV = abs(lumaNW + lumaSW - 2.0 * lumaW) + 2.0 * abs(lumaN + lumaS - 2.0 * lumaM) + abs(lumaNE + lumaSE - 2.0 * lumaE);
  bool horz = edgeH >= edgeV;
  float l1 = horz ? lumaS : lumaW, l2 = horz ? lumaN : lumaE;
  float g1 = abs(l1 - lumaM), g2 = abs(l2 - lumaM);
  bool steep1 = g1 >= g2;
  float gradScaled = 0.25 * max(g1, g2);
  float stepLen = horz ? uTexel.y : uTexel.x;
  float lumaLocal;
  if (steep1) { stepLen = -stepLen; lumaLocal = 0.5 * (l1 + lumaM); } else lumaLocal = 0.5 * (l2 + lumaM);
  vec2 uv = vUV;
  if (horz) uv.y += stepLen * 0.5; else uv.x += stepLen * 0.5;
  vec2 off = horz ? vec2(uTexel.x, 0.0) : vec2(0.0, uTexel.y);
  vec2 uv1 = uv - off, uv2 = uv + off;
  float e1 = texture(uSrc, uv1).a - lumaLocal, e2 = texture(uSrc, uv2).a - lumaLocal;
  bool r1 = abs(e1) >= gradScaled, r2 = abs(e2) >= gradScaled;
  const float Q[10] = float[](1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
  for (int i = 0; i < 10 && !(r1 && r2); ++i) {
    if (!r1) { uv1 -= off * Q[i]; e1 = texture(uSrc, uv1).a - lumaLocal; r1 = abs(e1) >= gradScaled; }
    if (!r2) { uv2 += off * Q[i]; e2 = texture(uSrc, uv2).a - lumaLocal; r2 = abs(e2) >= gradScaled; }
  }
  float d1 = horz ? vUV.x - uv1.x : vUV.y - uv1.y;
  float d2 = horz ? uv2.x - vUV.x : uv2.y - vUV.y;
  bool closer1 = d1 < d2;
  float dmin = min(d1, d2);
  float edgeLen = d1 + d2;
  float pixOff = -dmin / edgeLen + 0.5;
  bool lumaMSmaller = lumaM < lumaLocal;
  bool correct = ((closer1 ? e1 : e2) < 0.0) != lumaMSmaller;
  float finalOff = correct ? pixOff : 0.0;
  float lumaAvg = (1.0 / 12.0) * (2.0 * (lumaN + lumaS + lumaE + lumaW) + lumaNW + lumaNE + lumaSW + lumaSE);
  float sub = clamp(abs(lumaAvg - lumaM) / range, 0.0, 1.0);
  float subF = (-2.0 * sub + 3.0) * sub * sub;
  finalOff = max(finalOff, subF * subF * 0.75);
  vec2 fuv = vUV;
  if (horz) fuv.y += finalOff * stepLen; else fuv.x += finalOff * stepLen;
  oColor = vec4(texture(uSrc, fuv).rgb, 1.0);
}
