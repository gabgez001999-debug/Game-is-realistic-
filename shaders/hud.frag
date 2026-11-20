in vec2 vUV;
in vec4 vColor;
flat in vec4 vParams;
in vec2 vPix;

uniform sampler2D uFont;     // distance-field glyph atlas (see Hud.cpp)
uniform vec4 uRadar;         // player x, z, heading, scale (m per unit)
uniform vec4 uCityInfo;      // pitch, road half width, city extent, 0

out vec4 oColor;

void main() {
  int type = int(vParams.x + 0.5);
  if (type == 0) {
    oColor = vColor;
  } else if (type == 1) {
    float d = texture(uFont, vUV).r;
    float aa = max(fwidth(d) * 0.75, 0.02);
    float a = smoothstep(0.5 - aa, 0.5 + aa, d);
    oColor = vec4(vColor.rgb, vColor.a * a);
  } else if (type == 2) {
    // Ring / disc with soft edge: params.y inner radius fraction.
    vec2 q = vUV * 2.0 - 1.0;
    float r = length(q);
    float aa = fwidth(r) * 1.5;
    float a = smoothstep(1.0, 1.0 - aa, r) * smoothstep(vParams.y - aa, vParams.y, r);
    oColor = vec4(vColor.rgb, vColor.a * a);
  } else {
    // Radar: roads drawn analytically from the city grid, rotated with the heading.
    vec2 q = vUV * 2.0 - 1.0;
    float r = length(q);
    if (r > 1.0) discard;
    float h = uRadar.z;
    vec2 rq = vec2(q.x * cos(h) - q.y * sin(h), q.x * sin(h) + q.y * cos(h));
    vec2 world = uRadar.xy + rq * uRadar.w;
    float P = uCityInfo.x, W = uCityInfo.y;
    vec2 d = abs(world - floor(world / P + 0.5) * P);
    float inCity = step(max(abs(world.x), abs(world.y)), uCityInfo.z + W);
    float road = (1.0 - step(W, min(d.x, d.y))) * inCity;
    vec3 col = mix(vec3(0.06, 0.09, 0.07), vec3(0.42, 0.45, 0.48), road);
    float water = step(uCityInfo.w, world.y);
    col = mix(col, vec3(0.08, 0.16, 0.26), water);
    float edge = smoothstep(0.94, 0.97, r);
    col = mix(col, vec3(0.0), edge * 0.8);
    oColor = vec4(col, 0.82);
  }
}
