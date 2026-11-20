#include "common.glsl"

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;
flat in vec4 vMat;
in vec4 vExtra;
in vec3 vLocalPos;
in vec4 vCurClip;
in vec4 vPrevClip;

uniform vec4 uParams[8];
uniform vec4 uCity;        // x = block pitch, y = road half width, z = sidewalk width, w = city half extent
uniform float uShadowPass; // unused here, shared layout

layout(location = 0) out vec4 oAlbedo;   // rgb albedo, a metallic
layout(location = 1) out vec4 oNormal;   // xyz normal, a roughness
layout(location = 2) out vec4 oEmissive; // rgb emissive, a clearcoat
layout(location = 3) out vec4 oVelocity; // xy screen velocity

// Material IDs (must match engine/gfx/Mesh.h)
const int MAT_DEFAULT = 0, MAT_ASPHALT = 1, MAT_SIDEWALK = 2, MAT_CURB = 3, MAT_FACADE = 4, MAT_ROOF = 5,
          MAT_TERRAIN = 6, MAT_CARBODY = 7, MAT_TIRE = 8, MAT_RIM = 9, MAT_GLASS = 10, MAT_EMISSIVE = 11,
          MAT_SKIN = 12, MAT_CLOTH = 13, MAT_BARK = 14, MAT_LEAF = 15, MAT_METAL = 16, MAT_GRASS = 17,
          MAT_INTERIOR = 18, MAT_DIRT = 19;

struct Surf {
  vec3 albedo;
  float rough;
  float metal;
  vec3 emissive;
  vec3 N;
  float clearcoat;
};

// Perturbs the normal using the screen-space gradient of a height function value.
vec3 bumpNormal(vec3 N, float height, float strength) {
  vec3 dpdx = dFdx(vWorldPos), dpdy = dFdy(vWorldPos);
  float dhdx = dFdx(height), dhdy = dFdy(height);
  vec3 r1 = cross(dpdy, N), r2 = cross(N, dpdx);
  float det = dot(dpdx, r1);
  vec3 grad = sign(det) * (dhdx * r1 + dhdy * r2);
  return normalize(abs(det) * N - strength * grad);
}

// Wet surfaces: darker albedo, glossy puddles, flattened normals, rain ripples.
void applyWetness(inout Surf s, float porosity, float puddleMask) {
  float wet = uWetness;
  if (wet <= 0.001) return;
  s.albedo *= mix(1.0, mix(1.0, 0.45, porosity), wet);
  s.rough = mix(s.rough, mix(s.rough * 0.45, 0.06, puddleMask), wet);
  if (puddleMask > 0.5 && uRain > 0.01) {
    vec2 p = wrapCoord(vWorldPos.xz, 512.0) * 3.0;
    vec2 cell = floor(p);
    vec2 f = fract(p) - 0.5;
    float t = fract(uTime * 1.3 + hash12(cell));
    float r = length(f - (hash22(cell) - 0.5) * 0.6);
    float ring = sin((r - t * 0.6) * 60.0) * smoothstep(0.0, 0.05, r) * (1.0 - t) * smoothstep(0.35, 0.0, r);
    s.N = normalize(s.N + vec3(ring * 0.15 * uRain, 0.0, ring * 0.1 * uRain));
  }
  s.N = normalize(mix(s.N, vec3(0, 1, 0), puddleMask * wet * step(0.7, s.N.y)));
}

// ----------------------------------------------------------------------------
//  Asphalt with analytic lane markings derived from the city grid.
// ----------------------------------------------------------------------------
float markingCoverage(vec2 xz, out vec3 paint) {
  float P = uCity.x, W = uCity.y;
  vec2 cell = floor(xz / P + 0.5);
  vec2 d = xz - cell * P;
  vec2 ad = abs(d);
  paint = vec3(0.75);
  float aaw = max(fwidth(xz.x), fwidth(xz.y)) * 1.5;
  float cov = 0.0;
  bool roadX = ad.x < W, roadZ = ad.y < W;
  if (roadX && roadZ) return 0.0;  // intersection box: no lines
  // Express everything in road-local coords: lat = lateral offset, lon = along road, ext = offset to intersection
  float lat, lon, ext, side;
  if (roadX) { lat = d.x; lon = xz.y; ext = ad.y - W; side = sign(d.x) * sign(d.y); }
  else if (roadZ) { lat = d.y; lon = xz.x; ext = ad.x - W; side = -sign(d.x) * sign(d.y); }
  else return 0.0;
  float alat = abs(lat);
  // Double yellow center line.
  float yellow = 1.0 - smoothstep(0.06 - aaw, 0.06 + aaw, abs(alat - 0.16));
  // Dashed lane divider.
  float dash = step(fract(lon / 9.0), 0.36) * (1.0 - smoothstep(0.07 - aaw, 0.07 + aaw, abs(alat - 3.6)));
  // Solid edge lines.
  float edge = 1.0 - smoothstep(0.08 - aaw, 0.08 + aaw, abs(alat - (W - 0.55)));
  // Zebra crossing near the intersection.
  float zebra = 0.0;
  if (ext < 4.2 && ext > 0.6 && alat < W - 0.4) zebra = step(fract(lat / 1.1), 0.5);
  // Stop line on the approaching side, before the crosswalk.
  float stop = (ext > 4.6 && ext < 5.1 && side > 0.0 && alat < W - 0.4 && alat > 0.3) ? 1.0 : 0.0;
  // Fade dashes and center near crosswalks.
  float nearX = step(5.2, ext);
  yellow *= nearX; dash *= nearX;
  cov = max(max(dash, edge * nearX), max(zebra, stop));
  if (yellow > cov) { paint = vec3(0.62, 0.45, 0.08); cov = yellow; }
  return cov;
}

void matAsphalt(inout Surf s) {
  vec2 xz = vWorldPos.xz;
  vec2 w = wrapCoord(xz, 256.0);
  float big = fbm(xz * 0.05, 4);
  float grain = vnoise(w * 38.0) * 0.6 + vnoise(w * 91.0) * 0.4;
  float speck = step(0.86, hash12(floor(w * 60.0)));
  vec3 base = mix(vec3(0.052, 0.052, 0.055), vec3(0.085, 0.083, 0.08), big);
  base *= 0.82 + grain * 0.36;
  base += speck * 0.035;
  // Repair patches (darker, smoother).
  vec2 pc = floor(xz / vec2(7.0, 11.0));
  float patchSel = step(0.83, hash12(pc));
  vec2 pf = fract(xz / vec2(7.0, 11.0));
  float inPatch = patchSel * step(0.12, pf.x) * step(pf.x, 0.75) * step(0.2, pf.y) * step(pf.y, 0.9);
  base = mix(base, vec3(0.035, 0.035, 0.038), inPatch * 0.85);
  // Tyre polish in lane centers & oil drips.
  float P = uCity.x;
  vec2 d = xz - floor(xz / P + 0.5) * P;
  float lat = abs(d.x) < uCity.y ? abs(d.x) : abs(d.y);
  float lane = fract((lat) / 3.6);
  float track = smoothstep(0.18, 0.0, abs(lane - 0.28)) + smoothstep(0.18, 0.0, abs(lane - 0.72));
  float oil = smoothstep(0.12, 0.0, abs(lane - 0.5)) * fbm(w * 1.3, 3);
  base *= 1.0 - oil * 0.35;
  // Cracks.
  float cr = abs(fbm(w * 0.9 + 3.0, 5) - 0.5);
  float crack = smoothstep(0.012, 0.0, cr) * smoothstep(0.45, 0.65, big);
  base *= 1.0 - crack * 0.6;
  // Manholes in the outer lanes every 40 m.
  float rough = 0.88 - track * 0.12 - inPatch * 0.05;
  float metal = 0.0;
  {
    vec2 cell = floor(xz / P + 0.5);
    vec2 dd = xz - cell * P;
    vec2 lp = abs(dd.x) < uCity.y ? vec2(dd.x, xz.y) : vec2(dd.y, xz.x);
    float lonc = (fract(lp.y / 40.0) - 0.5) * 40.0;
    float r = length(vec2(abs(lp.x) - 5.4, lonc));
    if (r < 0.36 && abs(abs(lp.x) - 5.4) < 0.4) {
      float ringv = smoothstep(0.36, 0.33, r);
      float pattern = step(0.5, fract((lp.x + lonc) * 6.0)) * 0.3 + 0.7;
      base = mix(base, vec3(0.07, 0.065, 0.06) * pattern, ringv);
      rough = mix(rough, 0.55, ringv);
      metal = ringv * 0.8;
    }
  }
  vec3 paint;
  float cov = markingCoverage(xz, paint);
  float wear = smoothstep(0.25, 0.7, fbm(w * 2.3, 4) + grain * 0.3);
  cov *= mix(1.0, 0.35, wear * 0.8);
  base = mix(base, paint * (0.85 + grain * 0.15), cov);
  rough = mix(rough, 0.6, cov);
  s.albedo = base;
  s.rough = rough;
  s.metal = metal;
  s.N = bumpNormal(s.N, grain * 0.004 - crack * 0.003, 1.0);
  float puddle = smoothstep(0.58, 0.68, fbm(xz * 0.11 + 5.0, 4)) * (1.0 - cov);
  applyWetness(s, 0.8, puddle);
}

void matSidewalk(inout Surf s) {
  vec2 xz = vWorldPos.xz;
  vec2 w = wrapCoord(xz, 256.0);
  vec2 tile = xz / 1.5;
  vec2 tf = fract(tile);
  float aa = fwidth(tile.x) * 1.5 + 0.01;
  float groove = max(1.0 - smoothstep(0.0, 0.02 + aa, min(tf.x, 1.0 - tf.x)), 1.0 - smoothstep(0.0, 0.02 + aa, min(tf.y, 1.0 - tf.y)));
  float tv = hash12(floor(tile));
  vec3 base = mix(vec3(0.42, 0.41, 0.39), vec3(0.52, 0.5, 0.47), tv);
  float n = fbm(w * 3.0, 4);
  base *= 0.8 + n * 0.35;
  float stain = smoothstep(0.62, 0.8, fbm(w * 0.35 + 9.0, 4));
  base *= 1.0 - stain * 0.35;
  vec2 gq = fract(w * 3.0) - 0.5;
  float gum = step(0.93, hash12(floor(w * 3.0))) * smoothstep(0.06, 0.04, length(gq)) * 0.35;
  base *= 1.0 - gum;
  base *= 1.0 - groove * 0.45;
  s.albedo = base;
  s.rough = 0.82 + groove * 0.1;
  s.metal = 0.0;
  s.N = bumpNormal(s.N, -groove * 0.004 + vnoise(w * 40.0) * 0.0015, 1.0);
  applyWetness(s, 0.6, smoothstep(0.65, 0.75, fbm(xz * 0.3, 3)) * 0.6);
}

// ----------------------------------------------------------------------------
//  Building facades with parallax interior mapping.
// ----------------------------------------------------------------------------
vec3 interiorRoom(vec2 roomUV, vec3 dirTS, vec2 roomSize, float depth, float seed, float litChance, out float lit) {
  vec3 pos = vec3(roomUV, 0.0);
  vec3 d = dirTS / vec3(roomSize, depth);
  vec3 tp;
  tp.x = ((d.x > 0.0 ? 1.0 : 0.0) - pos.x) / (abs(d.x) > 1e-5 ? d.x : 1e-5);
  tp.y = ((d.y > 0.0 ? 1.0 : 0.0) - pos.y) / (abs(d.y) > 1e-5 ? d.y : 1e-5);
  tp.z = (1.0 - pos.z) / max(d.z, 1e-4);
  float t = min(min(tp.x, tp.y), tp.z);
  vec3 h = pos + d * t;
  float r1 = hash11(seed * 13.7), r2 = hash11(seed * 7.1 + 3.0), r3 = hash11(seed * 3.3 + 9.0);
  vec3 wall = mix(vec3(0.75, 0.72, 0.66), vec3(0.62, 0.66, 0.7), r1) * mix(0.7, 1.0, r2);
  vec3 floorC = mix(vec3(0.32, 0.2, 0.12), vec3(0.3, 0.3, 0.32), step(0.5, r3));
  vec3 ceil = vec3(0.85);
  vec3 c;
  float lightFall = 1.0;
  if (t == tp.z) {
    c = wall * 0.9;
    // Furniture silhouette & a picture/monitor on the back wall.
    if (h.y < 0.32 && abs(h.x - mix(0.3, 0.7, r2)) < 0.22) c = mix(vec3(0.15, 0.12, 0.1), vec3(0.35, 0.3, 0.25), r3);
    if (h.y > 0.45 && h.y < 0.7 && abs(h.x - 0.5) < 0.15) c *= mix(0.4, 1.4, r1);
    lightFall = 0.7;
  } else if (t == tp.y) {
    if (d.y < 0.0) { c = floorC; lightFall = 0.55; }
    else {
      c = ceil;
      float lamp = smoothstep(0.12, 0.08, length(h.xz - vec2(0.5, 0.45)));
      c += lamp * 4.0 * step(0.5, litChance);
      lightFall = 1.0;
    }
  } else {
    c = wall * 0.75;
    lightFall = 0.75;
  }
  lit = step(1.0 - litChance, hash11(seed * 91.3));
  return c * lightFall * mix(1.0, 0.65, h.z);
}

void matFacade(inout Surf s) {
  float seed = vExtra.x;
  int style = int(vExtra.y + 0.5);
  float floorH = vExtra.z;
  float colW = vExtra.w;
  vec2 uv = vUV;  // u = metres along facade, v = metres above building ground
  vec3 N = normalize(s.N);
  vec3 T = normalize(cross(vec3(0, 1, 0), N));
  vec3 Bt = vec3(0, 1, 0);
  vec3 V = normalize(vWorldPos - uCamPos);
  vec3 dirTS = vec3(dot(V, T), dot(V, Bt), dot(V, -N));

  float r1 = hash11(seed * 1.31), r2 = hash11(seed * 2.73), r3 = hash11(seed * 5.17);
  // Wall palette per style.
  vec3 wallCol;
  if (style == 0) wallCol = mix(vec3(0.2, 0.22, 0.24), vec3(0.5, 0.52, 0.55), r1);
  else if (style == 1) wallCol = mix(vec3(0.55, 0.52, 0.46), vec3(0.7, 0.68, 0.62), r1);
  else if (style == 2) wallCol = mix(vec3(0.36, 0.14, 0.08), vec3(0.45, 0.25, 0.16), r1);
  else if (style == 3) {
    vec3 pastel[4] = vec3[](vec3(0.85, 0.78, 0.62), vec3(0.85, 0.6, 0.5), vec3(0.6, 0.78, 0.7), vec3(0.72, 0.75, 0.82));
    wallCol = pastel[int(r1 * 3.99)];
  } else wallCol = mix(vec3(0.1, 0.1, 0.11), vec3(0.25, 0.24, 0.23), r1);

  float u = uv.x, v = uv.y;
  float aa = max(fwidth(u), fwidth(v));
  vec2 w = wrapCoord(vWorldPos.xz + vWorldPos.y, 128.0);

  // Ground floor storefront.
  float groundH = max(floorH * 1.25, 4.2);
  bool ground = v < groundH && style != 0;
  float col = floor(u / colW);
  float row = floor((v - (ground ? 0.0 : groundH)) / floorH);
  vec2 cellUV = vec2(fract(u / colW), fract((v - groundH) / floorH));

  // Window opening inside a cell.
  float winW = style == 0 ? 0.97 : (style == 4 ? 0.92 : mix(0.45, 0.62, r2));
  float sill = style == 0 ? 0.02 : (style == 4 ? 0.3 : 0.28);
  float head = style == 0 ? 0.98 : (style == 4 ? 0.78 : 0.8);
  float inWin = step(abs(cellUV.x - 0.5), winW * 0.5) * step(sill, cellUV.y) * step(cellUV.y, head);
  if (v < groundH) inWin = 0.0;

  vec3 albedo = wallCol;
  float rough = 0.85, metal = 0.0;
  vec3 emis = vec3(0.0);

  // Wall detail: brick, concrete, stucco.
  if (style == 2) {
    vec2 b = vec2(u / 0.25, v / 0.075);
    b.x += step(1.0, mod(floor(b.y), 2.0)) * 0.5;
    vec2 bf = fract(b);
    float mortar = 1.0 - (step(0.06, bf.x) * step(bf.x, 0.94) * step(0.12, bf.y) * step(bf.y, 0.88));
    float fade = smoothstep(0.02, 0.0, aa * 6.0);
    albedo *= mix(1.0, 0.75 + 0.5 * hash12(floor(b)), fade);
    albedo = mix(albedo, vec3(0.5, 0.48, 0.44), mortar * fade * 0.8);
  } else if (style == 1 || style == 3) {
    float n = fbm(w * 1.5, 4);
    albedo *= 0.85 + n * 0.25;
    // Floor slab bands.
    float band = step(fract((v - groundH) / floorH), 0.06) * step(groundH, v);
    albedo *= 1.0 - band * 0.15;
  } else if (style == 4) {
    float seam = step(fract(u / 1.5), 0.02) + step(fract(v / floorH), 0.03);
    albedo *= 1.0 - min(seam, 1.0) * 0.3;
    rough = 0.45; metal = 0.3;
  }
  // Rain streaks under windows & grime at the base.
  float streak = fbm(vec2(u * 6.0, v * 0.25) + seed, 4);
  float underWin = smoothstep(0.4, 0.0, abs(cellUV.x - 0.5) - winW * 0.45) * step(cellUV.y, sill);
  albedo *= 1.0 - smoothstep(0.5, 0.8, streak) * 0.25 * (0.5 + underWin);
  albedo *= mix(0.6, 1.0, smoothstep(0.0, 2.5, v));

  float glassMask = 0.0;
  vec3 glassTint = vec3(1.0);
  if (style == 0) {
    // Curtain wall: mullions + spandrel bands.
    vec2 m = vec2(fract(u / colW), fract(v / floorH));
    float mull = 1.0 - step(0.025 + aa / colW, m.x) * step(m.x, 0.975 - aa / colW);
    float spandrel = step(m.y, 0.18);
    glassTint = mix(vec3(0.35, 0.5, 0.55), vec3(0.45, 0.48, 0.5), r2);
    glassTint = mix(glassTint, vec3(0.55, 0.45, 0.3), step(0.8, r3));
    glassMask = (1.0 - mull) * (1.0 - spandrel);
    albedo = mix(vec3(0.3, 0.31, 0.33), glassTint * 0.3, spandrel);
    rough = 0.35; metal = 0.6;
    // One room spans four mullion bays.
    float roomW = colW * 4.0;
    cellUV = vec2(fract(u / roomW), m.y);
    col = floor(u / roomW);
    colW = roomW;
    row = floor(v / floorH);
  } else {
    glassMask = inWin;
    glassTint = vec3(0.8, 0.85, 0.85);
    // Window frame & sill.
    float fx = abs(cellUV.x - 0.5) - winW * 0.5;
    float frame = smoothstep(0.0, -0.015, fx) * (1.0 - smoothstep(-0.03, -0.045, fx));
    float fy = min(cellUV.y - sill, head - cellUV.y);
    frame = max(frame, (1.0 - smoothstep(0.015, 0.03, fy)) * inWin);
    vec3 frameCol = style == 3 ? vec3(0.85) : vec3(0.12);
    albedo = mix(albedo, frameCol, frame * inWin);
    glassMask *= 1.0 - frame;
    float sillBand = step(abs(cellUV.x - 0.5), winW * 0.55) * step(sill - 0.025, cellUV.y) * step(cellUV.y, sill) * step(groundH, v);
    albedo = mix(albedo, albedo * 1.3 + 0.05, sillBand);
    // Recess shadow around opening.
    albedo *= 1.0 - (1.0 - smoothstep(0.0, 0.06, abs(fx))) * 0.3 * step(groundH, v);
  }

  // Storefront.
  if (ground) {
    float shopW = colW * 2.0;
    vec2 sc = vec2(fract(u / shopW), v / groundH);
    float glassArea = step(0.06, sc.x) * step(sc.x, 0.94) * step(0.08, sc.y) * step(sc.y, 0.72);
    float signBand = step(0.78, sc.y) * step(sc.y, 0.95) * step(0.05, sc.x) * step(sc.x, 0.95);
    float shopSeed = hash11(floor(u / shopW) + seed * 17.0);
    vec3 signCol = 0.5 + 0.5 * cos(6.2831 * (shopSeed + vec3(0.0, 0.33, 0.67)));
    albedo = mix(albedo, signCol * 0.6, signBand);
    emis += signCol * signBand * mix(0.15, 3.0, uNight) * step(0.4, shopSeed);
    if (glassArea > 0.5) {
      float litS;
      vec3 room = interiorRoom(vec2(sc.x, sc.y / 0.72), dirTS, vec2(shopW, groundH * 0.72), 7.0, shopSeed * 100.0, 1.0, litS);
      room *= mix(vec3(1.0), signCol * 0.6 + 0.6, 0.3);
      float NoV = max(dot(N, -V), 0.0);
      float transmit = (1.0 - pow(1.0 - NoV, 4.0)) * 0.85;
      emis += room * mix(0.35, 1.1, uNight) * transmit;
      albedo = vec3(0.01);
      rough = 0.04; metal = 0.0;
    }
  } else if (glassMask > 0.01) {
    float roomSeed = hash12(vec2(col, row) + seed * 31.0);
    float litChance = mix(0.25, 0.55, r3);
    float lit;
    vec3 room = interiorRoom(cellUV, dirTS, vec2(colW, floorH), 6.0, roomSeed * 1000.0, litChance, lit);
    // Blinds.
    float blind = hash11(roomSeed * 77.0);
    float blindLevel = mix(1.2, 0.35, blind * step(0.55, blind));
    float yIn = (cellUV.y - sill) / (head - sill);
    float blinds = step(blindLevel, yIn);
    room = mix(room, vec3(0.75, 0.72, 0.66) * (0.85 + 0.15 * step(0.5, fract(yIn * 40.0))), blinds);
    float warm = hash11(roomSeed * 13.0);
    vec3 lightCol = kelvin(mix(2700.0, 4500.0, warm));
    float ambient = mix(0.10, 0.004, uNight) * (style == 0 ? 0.5 : 1.0);
    vec3 interior = room * (ambient + lit * lightCol * mix(0.25, 0.9, uNight));
    float NoV = max(dot(N, -V), 0.0);
    float transmit = (1.0 - pow(1.0 - NoV, 4.0)) * (style == 0 ? 0.45 : 0.85);
    vec3 glassEmis = interior * transmit * glassTint;
    emis = mix(emis, glassEmis, glassMask);
    albedo = mix(albedo, style == 0 ? glassTint * 0.08 : vec3(0.012), glassMask);
    rough = mix(rough, style == 0 ? 0.03 : 0.05 + hash11(roomSeed) * 0.08, glassMask);
    metal = mix(metal, style == 0 ? 0.55 : 0.0, glassMask);
  }
  s.albedo = albedo;
  s.rough = rough;
  s.metal = metal;
  s.emissive = emis;
  applyWetness(s, 0.3, 0.0);
}

void matRoof(inout Surf s) {
  vec2 w = wrapCoord(vWorldPos.xz, 256.0);
  float n = fbm(w * 0.7, 4), g = vnoise(w * 40.0);
  vec3 c = mix(vec3(0.16, 0.16, 0.17), vec3(0.32, 0.31, 0.3), n);
  c *= 0.85 + g * 0.3;
  float membrane = smoothstep(0.55, 0.62, fbm(w * 0.08 + 4.0, 3));
  c = mix(c, vec3(0.36, 0.36, 0.35), membrane * 0.5);
  s.albedo = c;
  s.rough = 0.92;
  s.metal = 0.0;
  applyWetness(s, 0.4, smoothstep(0.6, 0.7, n) * 0.8);
}

void matTerrain(inout Surf s) {
  vec2 xz = vWorldPos.xz;
  vec2 w = wrapCoord(xz, 512.0);
  float h = vWorldPos.y;
  float slope = 1.0 - clamp(s.N.y, 0.0, 1.0);
  float n1 = fbm(xz * 0.02, 5), n2 = fbm(w * 0.3, 4), n3 = vnoise(w * 6.0);
  vec3 lush = vec3(0.07, 0.11, 0.035), dry = vec3(0.19, 0.18, 0.08), dirt = vec3(0.2, 0.15, 0.1);
  vec3 grass = mix(lush, dry, smoothstep(0.35, 0.7, n1));
  grass *= 0.75 + n3 * 0.45;
  float dirtM = smoothstep(0.55, 0.7, n2);
  vec3 c = mix(grass, dirt * (0.8 + n3 * 0.3), dirtM);
  float rough = 0.95;
  // Rock on slopes (triplanar-ish via world xz+y).
  float rockM = smoothstep(0.28, 0.45, slope + (n2 - 0.5) * 0.2);
  vec3 rock = mix(vec3(0.28, 0.27, 0.25), vec3(0.42, 0.4, 0.37), fbm(vec2(xz.x + vWorldPos.y, xz.y - vWorldPos.y) * 0.5, 5));
  c = mix(c, rock, rockM);
  // Beach sand.
  float sandM = 1.0 - smoothstep(1.8, 3.5, h + (n2 - 0.5) * 1.5);
  vec3 sand = mix(vec3(0.55, 0.49, 0.38), vec3(0.62, 0.56, 0.44), n3);
  float wetSand = 1.0 - smoothstep(0.1, 0.8, h);
  sand = mix(sand, sand * 0.55, wetSand);
  c = mix(c, sand, sandM);
  rough = mix(rough, mix(0.9, 0.35, wetSand), sandM);
  rough = mix(rough, 0.8, rockM);
  s.albedo = c;
  s.rough = rough;
  s.metal = 0.0;
  s.N = bumpNormal(s.N, n3 * 0.03 * (1.0 - sandM) + rockM * fbm(w * 2.0, 3) * 0.15, 1.0);
  applyWetness(s, 0.7, 0.0);
}

void matGrass(inout Surf s) {
  vec2 xz = vWorldPos.xz;
  vec2 w = wrapCoord(xz, 256.0);
  float stripes = step(0.5, fract(xz.x / 4.0));
  float n = fbm(w * 0.5, 4), d = vnoise(w * 12.0);
  vec3 c = mix(vec3(0.06, 0.12, 0.03), vec3(0.11, 0.16, 0.05), n);
  c *= 0.85 + stripes * 0.12 + d * 0.25;
  s.albedo = c;
  s.rough = 0.92;
  s.metal = 0.0;
  s.N = bumpNormal(s.N, d * 0.02, 1.0);
  applyWetness(s, 0.6, 0.0);
}

// ----------------------------------------------------------------------------
//  Vehicles
// ----------------------------------------------------------------------------
float crackPattern(vec2 p, vec2 center) {
  vec2 q = p - center;
  float r = length(q);
  float a = atan(q.y, q.x);
  float radial = smoothstep(0.012, 0.0, abs(fract(a * 3.0 + hash11(floor(a * 3.0)) * 0.3) - 0.5) * r * 2.0) * step(r, 0.6);
  float rings = smoothstep(0.008, 0.0, abs(fract(r * 9.0 + vnoise(q * 20.0) * 0.4) - 0.5) * 0.12) * step(r, 0.35);
  return clamp(radial + rings, 0.0, 1.0);
}

void matCarBody(inout Surf s, bool front) {
  int zone = int(vMat.y + 0.5);
  vec3 rest = vExtra.xyz;
  float dmg = vExtra.w;
  vec3 paint = uParams[0].rgb;
  float dirt = uParams[1].x;
  float carSeed = uParams[1].y;
  if (!front) {
    // Interior seen through broken glass or open doors.
    s.albedo = vec3(0.035, 0.034, 0.033);
    s.rough = 0.9; s.metal = 0.0; s.N = -s.N;
    return;
  }
  s.clearcoat = 0.0;
  if (zone == 0) {
    // Paint: metallic flake + clearcoat; scratches reveal primer/metal where deformed.
    vec3 c = paint;
    float flake = hash13(floor(rest * 900.0));
    s.albedo = c * (0.92 + flake * 0.16);
    s.rough = 0.32;
    s.metal = uParams[0].w;
    s.clearcoat = 1.0;
    // Panel gaps.
    float z = rest.z;
    vec4 pl = uParams[6];
    float gap = 0.0;
    float gw = 0.004 + fwidth(z) * 0.7;
    if (rest.y > uParams[7].w + 0.03 && rest.y < uParams[7].z + 0.02) {
      gap += 1.0 - smoothstep(gw * 0.5, gw, abs(z - pl.x));
      gap += 1.0 - smoothstep(gw * 0.5, gw, abs(z - pl.y));
    }
    if (rest.y > uParams[7].z - 0.06) {
      gap += (1.0 - smoothstep(gw * 0.5, gw, abs(z - pl.z))) * step(abs(rest.x), uParams[1].w * 0.85);
      gap += (1.0 - smoothstep(gw * 0.5, gw, abs(z - pl.w))) * step(abs(rest.x), uParams[1].w * 0.85);
    }
    // Door handles.
    float handle = step(abs(z - (pl.y - 0.12)), 0.08) * step(abs(rest.y - (uParams[7].z - 0.08)), 0.018) * step(uParams[1].w * 0.9, abs(rest.x));
    s.albedo = mix(s.albedo, vec3(0.6), handle);
    s.albedo *= 1.0 - clamp(gap, 0.0, 1.0) * 0.9;
    if (dmg > 0.015) {
      float sc = fbm(rest.xz * 40.0 + rest.y * 13.0, 4);
      float scratch = smoothstep(0.55, 0.62, sc + dmg * 2.5) ;
      vec3 bare = mix(vec3(0.55, 0.55, 0.56), vec3(0.42, 0.42, 0.4), sc);
      s.albedo = mix(s.albedo, bare, scratch * 0.9);
      s.metal = mix(s.metal, 0.9, scratch);
      s.rough = mix(s.rough, 0.45, scratch);
      s.clearcoat *= 1.0 - scratch;
      // Crumpled paint gets dull and cracked.
      s.rough = mix(s.rough, 0.6, clamp(dmg * 3.0, 0.0, 0.7));
      s.clearcoat *= 1.0 - clamp(dmg * 4.0, 0.0, 0.8);
    }
  } else if (zone >= 1 && zone <= 4) {
    float health = zone == 1 ? uParams[2].x : zone == 2 ? uParams[2].y : zone == 3 ? uParams[2].z : uParams[2].w;
    if (zone >= 3 && health < 0.05) discard;  // shattered side window
    s.albedo = vec3(0.012, 0.014, 0.016);
    s.rough = 0.03; s.metal = 0.0;
    s.emissive = vec3(0.004);
    if (health < 0.99) {
      float cr = crackPattern(rest.xy * vec2(1.0, 1.4), vec2(hash11(carSeed) - 0.5, uParams[7].z + 0.25));
      cr *= smoothstep(1.0, 0.4, health);
      s.albedo = mix(s.albedo, vec3(0.75), cr * 0.85);
      s.rough = mix(s.rough, 0.6, cr);
    }
  } else if (zone >= 5 && zone <= 8) {
    float health = zone == 5 ? uParams[3].x : zone == 6 ? uParams[3].y : zone == 7 ? uParams[3].z : uParams[3].w;
    bool head = zone <= 6;
    float on = head ? uParams[4].x : 1.0;
    // Reflector cups.
    vec2 q = fract(rest.xy * vec2(8.0, 10.0)) - 0.5;
    float cup = smoothstep(0.45, 0.2, length(q));
    if (head) {
      s.albedo = mix(vec3(0.6), vec3(0.85), cup);
      s.metal = 0.9; s.rough = 0.08;
      s.emissive = vec3(1.0, 0.95, 0.86) * on * 30.0 * cup * health;
      s.emissive += vec3(0.6, 0.75, 1.0) * 1.5 * health;  // DRL
    } else {
      float isLeft = zone == 7 ? 1.0 : 0.0;
      float indicator = isLeft > 0.5 ? uParams[5].x : uParams[5].y;
      float outer = step(uParams[1].w * 0.78, abs(rest.x));
      s.albedo = mix(vec3(0.25, 0.01, 0.01), vec3(0.4, 0.2, 0.02), outer * step(0.5, indicator + 0.0));
      s.rough = 0.1; s.metal = 0.0;
      float tail = uParams[4].x * 1.2 + uParams[4].y * 9.0 + 0.15;
      s.emissive = vec3(1.0, 0.02, 0.01) * tail * health;
      float blink = step(0.5, fract(uTime * 1.5));
      s.emissive += vec3(1.0, 0.45, 0.02) * indicator * blink * 12.0 * outer * health;
      float rev = uParams[4].z * step(abs(abs(rest.x) - uParams[1].w * 0.55), 0.06);
      s.emissive += vec3(1.0) * rev * 6.0 * health;
    }
    if (health < 0.5) {
      float cr = crackPattern(rest.xy * 6.0, vec2(0.3));
      s.albedo = mix(s.albedo * 0.3, vec3(0.5), cr);
      s.rough = 0.5;
    }
  } else if (zone == 9) {
    float grille = step(0.5, fract(rest.y * 40.0)) * 0.3 + 0.7;
    s.albedo = vec3(0.025) * grille;
    s.rough = 0.55; s.metal = 0.0;
  } else if (zone == 10) {
    s.albedo = vec3(0.9); s.rough = 0.1; s.metal = 1.0;
  } else if (zone == 11) {
    s.albedo = vec3(0.03); s.rough = 0.9; s.metal = 0.0;
  } else if (zone == 12) {
    vec2 q = rest.xy;
    float chars = step(0.45, hash12(floor(q * vec2(28.0, 10.0)))) * step(abs(q.y - uParams[7].w - 0.25), 0.03);
    s.albedo = mix(vec3(0.85), vec3(0.05), chars);
    s.rough = 0.4; s.metal = 0.0;
  }
  // Road dirt on lower body.
  float low = 1.0 - smoothstep(uParams[7].w, uParams[7].w + 0.35, rest.y);
  float dn = fbm(rest.xz * 7.0 + rest.y * 3.0, 4);
  float dm = clamp(low * dirt * 1.6 * (0.5 + dn), 0.0, 1.0);
  s.albedo = mix(s.albedo, vec3(0.22, 0.19, 0.15), dm * 0.8);
  s.rough = mix(s.rough, 0.85, dm);
  s.clearcoat *= 1.0 - dm;
  applyWetness(s, 0.0, 0.0);
  if (uWetness > 0.0) s.clearcoat = max(s.clearcoat, uWetness * 0.8 * float(zone == 0));
}

void matTire(inout Surf s) {
  // uv.x around the tyre, uv.y across the profile.
  float across = vUV.y;
  float tread = smoothstep(0.25, 0.3, across) * smoothstep(0.75, 0.7, across);
  float groove = step(0.5, fract(vUV.x * 70.0 + abs(across - 0.5) * 3.0)) * tread;
  float lettering = (1.0 - tread) * step(0.82, hash12(floor(vec2(vUV.x * 120.0, across * 12.0)))) * step(abs(across - 0.5), 0.45) * step(0.3, abs(across - 0.5));
  s.albedo = vec3(0.028) * (1.0 - groove * 0.4) + lettering * 0.05;
  s.rough = 0.88 - groove * 0.1;
  s.metal = 0.0;
  applyWetness(s, 0.0, 0.0);
}

void matLeaf(inout Surf s, bool front) {
  // Palm frond: uv.x across (0..1), uv.y along the rib.
  float along = vUV.y;
  float across = abs(vUV.x - 0.5) * 2.0;
  float leaflet = fract(along * 34.0);
  float width = (1.0 - along * 0.75) * smoothstep(0.0, 0.08, along);
  float shape = step(across, width * (0.4 + 0.6 * smoothstep(0.0, 0.25, leaflet) * smoothstep(1.0, 0.75, leaflet)));
  float rib = step(across, 0.04);
  if (shape < 0.5 && rib < 0.5) discard;
  vec3 c = mix(vec3(0.05, 0.11, 0.03), vec3(0.15, 0.18, 0.05), hash11(floor(along * 34.0) + vColor.r * 10.0));
  c = mix(c, vec3(0.25, 0.2, 0.08), smoothstep(0.7, 1.0, along) * 0.6);
  s.albedo = c;
  s.rough = 0.6;
  s.metal = 0.0;
  if (!front) s.N = -s.N;
}

void matBark(inout Surf s) {
  float rings = fract(vUV.y * 3.2);
  float r = smoothstep(0.0, 0.15, rings) * smoothstep(1.0, 0.7, rings);
  vec3 c = mix(vec3(0.18, 0.15, 0.11), vec3(0.33, 0.28, 0.21), r);
  c *= 0.85 + vnoise(vUV * vec2(30.0, 60.0)) * 0.3;
  s.albedo = c;
  s.rough = 0.95;
  s.metal = 0.0;
  s.N = bumpNormal(s.N, r * 0.01, 1.0);
}

void main() {
  Surf s;
  s.albedo = srgbToLinear(vColor.rgb);
  s.rough = vMat.y / 255.0;
  s.metal = vMat.z / 255.0;
  s.emissive = vec3(0.0);
  s.N = normalize(vNormal);
  s.clearcoat = 0.0;
  int m = int(vMat.x + 0.5);
  bool front = gl_FrontFacing;
  if (!front && m != MAT_LEAF && m != MAT_CARBODY) s.N = -s.N;

  if (m == MAT_DEFAULT) {
    s.emissive = s.albedo * (vMat.w / 255.0) * 20.0 * uParams[0].w;
    applyWetness(s, 0.2, 0.0);
  } else if (m == MAT_ASPHALT) matAsphalt(s);
  else if (m == MAT_SIDEWALK) matSidewalk(s);
  else if (m == MAT_CURB) {
    s.albedo *= 0.85 + vnoise(wrapCoord(vWorldPos.xz, 256.0) * 9.0) * 0.3;
    s.rough = 0.8;
    applyWetness(s, 0.5, 0.0);
  }
  else if (m == MAT_FACADE) matFacade(s);
  else if (m == MAT_ROOF) matRoof(s);
  else if (m == MAT_TERRAIN) matTerrain(s);
  else if (m == MAT_CARBODY) matCarBody(s, front);
  else if (m == MAT_TIRE) matTire(s);
  else if (m == MAT_RIM) { s.albedo = s.albedo; s.metal = 1.0; s.rough = vMat.y / 255.0; }
  else if (m == MAT_GLASS) {
    vec3 V = normalize(vWorldPos - uCamPos);
    s.albedo = vec3(0.01); s.rough = 0.04; s.metal = 0.0;
    s.emissive = srgbToLinear(vColor.rgb) * mix(0.15, 0.9, uNight) * (1.0 - pow(1.0 - abs(dot(V, s.N)), 4.0));
  }
  else if (m == MAT_EMISSIVE) {
    s.emissive = srgbToLinear(vColor.rgb) * (vMat.w / 255.0) * 60.0 * uParams[0].w;
    s.albedo *= 0.3; s.rough = 0.2;
  }
  else if (m == MAT_SKIN) { s.rough = 0.55; s.metal = 0.0; }
  else if (m == MAT_CLOTH) {
    float weave = vnoise(vLocalPos.xy * 300.0) * 0.5 + vnoise(vLocalPos.zy * 300.0) * 0.5;
    s.albedo *= 0.85 + weave * 0.3;
    s.rough = 0.92; s.metal = 0.0;
    s.albedo *= uParams[1].x > 0.0 ? 1.0 : 1.0;
  }
  else if (m == MAT_BARK) matBark(s);
  else if (m == MAT_LEAF) matLeaf(s, front);
  else if (m == MAT_METAL) {
    s.metal = vMat.z / 255.0;
    s.rough = vMat.y / 255.0 + vnoise(vLocalPos.xy * 30.0) * 0.1;
    applyWetness(s, 0.0, 0.0);
  }
  else if (m == MAT_GRASS) matGrass(s);
  else if (m == MAT_INTERIOR) { s.rough = 0.85; s.metal = 0.0; }
  else if (m == MAT_DIRT) {
    vec2 w = wrapCoord(vWorldPos.xz, 256.0);
    s.albedo = mix(vec3(0.18, 0.14, 0.1), vec3(0.28, 0.22, 0.16), fbm(w * 2.0, 4));
    s.rough = 0.95;
    applyWetness(s, 0.9, 0.0);
  }

  vec2 cur = vCurClip.xy / vCurClip.w;
  vec2 prev = vPrevClip.xy / vPrevClip.w;
  oAlbedo = vec4(clamp(s.albedo, 0.0, 1.0), s.metal);
  oNormal = vec4(normalize(s.N), clamp(s.rough, 0.02, 1.0));
  oEmissive = vec4(s.emissive, s.clearcoat);
  oVelocity = vec4((cur - prev) * 0.5, 0.0, 1.0);
}
