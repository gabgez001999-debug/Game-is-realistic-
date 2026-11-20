#include "City.h"

#include "core/Noise.h"
#include "gfx/MeshBuilder.h"

namespace game {
using glm::mix;

// ---------------------------------------------------------------------------
//  Geometry helpers
// ---------------------------------------------------------------------------

// Axis-aligned building volume: facade on the sides (metric UVs, v = height above ground),
// roof material on top.
static void addBuildingBox(MeshData& md, const vec3& mn, const vec3& mx, const Surface& facade, const Surface& roof,
                           float groundY, bool top = true) {
  auto side = [&](vec3 a, vec3 b, vec3 n) {
    float len = glm::length(vec2(b.x - a.x, b.z - a.z));
    uint32_t base = uint32_t(md.v.size());
    const vec3 ps[4] = {vec3(a.x, mn.y, a.z), vec3(b.x, mn.y, b.z), vec3(b.x, mx.y, b.z), vec3(a.x, mx.y, a.z)};
    const vec2 uvs[4] = {{0.f, mn.y - groundY}, {len, mn.y - groundY}, {len, mx.y - groundY}, {0.f, mx.y - groundY}};
    for (int k = 0; k < 4; ++k) {
      Vertex v;
      v.pos = ps[k];
      v.normal = n;
      v.uv = uvs[k];
      applySurface(v, facade);
      md.v.push_back(v);
    }
    md.i.insert(md.i.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  side(vec3(mn.x, 0, mx.z), vec3(mx.x, 0, mx.z), vec3(0, 0, 1));
  side(vec3(mx.x, 0, mn.z), vec3(mn.x, 0, mn.z), vec3(0, 0, -1));
  side(vec3(mx.x, 0, mx.z), vec3(mx.x, 0, mn.z), vec3(1, 0, 0));
  side(vec3(mn.x, 0, mn.z), vec3(mn.x, 0, mx.z), vec3(-1, 0, 0));
  if (top) addQuad(md, vec3(mn.x, mx.y, mx.z), vec3(mx.x, mx.y, mx.z), vec3(mx.x, mx.y, mn.z), vec3(mn.x, mx.y, mn.z), roof);
}

static void addAABox(MeshData& md, const vec3& mn, const vec3& mx, const Surface& s) {
  addBox(md, (mn + mx) * 0.5f, (mx - mn) * 0.5f, s);
}

static StaticBox staticAABB(const vec3& mn, const vec3& mx, SurfaceType st) {
  StaticBox b;
  b.box.c = (mn + mx) * 0.5f;
  b.box.h = (mx - mn) * 0.5f;
  b.surface = st;
  return b;
}

// Palm tree: curved tapered trunk + drooping fronds (alpha-tested leaf material).
static void buildPalm(MeshData& out, uint32_t seed, float& heightOut) {
  Rng r(seed * 7919u + 13u);
  float H = r.range(8.5f, 13.5f);
  heightOut = H;
  float lean = r.range(0.3f, 1.8f);
  const int segs = 12;
  Surface bark = surf(MAT_BARK, 0x8a7a66, 0.95f);
  MeshData trunk;
  vec3 prev(0.f);
  vec3 crown(0.f);
  for (int i = 0; i < segs; ++i) {
    float t0 = float(i) / segs, t1 = float(i + 1) / segs;
    vec3 a(lean * t0 * t0, H * t0, 0.f), b(lean * t1 * t1, H * t1, 0.f);
    float r0 = mix(0.26f, 0.15f, t0) + (i == 0 ? 0.08f : 0.f), r1 = mix(0.26f, 0.15f, t1);
    addCylinder(trunk, a, glm::normalize(b - a), r0, r1, glm::length(b - a), 12, bark, false);
    crown = b;
  }
  // Leaf stubs at the crown.
  addEllipsoid(trunk, crown + vec3(0, 0.15f, 0), vec3(0.32f, 0.45f, 0.32f), 10, 6, surf(MAT_BARK, 0x6b5a3a, 0.9f));

  MeshData fronds;
  int n = 13 + int(r.uniform() * 4.f);
  for (int k = 0; k < n; ++k) {
    float yaw = TAU * float(k) / float(n) + r.range(-0.15f, 0.15f);
    bool dead = k % 6 == 0;
    float L = r.range(3.6f, 4.8f) * (dead ? 0.85f : 1.f);
    float pitch = dead ? -0.9f : r.range(-0.1f, 0.55f);
    float droop = dead ? 0.2f : r.range(0.7f, 1.1f);
    vec3 h(std::cos(yaw), 0.f, std::sin(yaw));
    vec3 side = glm::normalize(glm::cross(h, vec3(0, 1, 0)));
    uint8_t shade = uint8_t(dead ? 200 : r.rangei(40, 160));
    uint32_t base = uint32_t(fronds.v.size());
    const int S = 10;
    for (int s = 0; s <= S; ++s) {
      float t = float(s) / S;
      vec3 p = crown + vec3(0, 0.2f, 0) + h * (L * t * std::cos(pitch)) +
               vec3(0, 1, 0) * (L * t * std::sin(pitch) - droop * t * t * L * 0.55f);
      float hw = 0.7f * (1.f - t * 0.35f);
      vec3 vpos[3] = {p - side * hw + vec3(0, -0.12f * t, 0), p + vec3(0, 0.06f, 0), p + side * hw + vec3(0, -0.12f * t, 0)};
      for (int c = 0; c < 3; ++c) {
        Vertex v;
        v.pos = vpos[c];
        v.uv = vec2(float(c) * 0.5f, t);
        applySurface(v, surf(MAT_LEAF, 0x3c5a20, 0.6f));
        v.color[0] = shade;
        if (dead) { v.color[1] = 120; v.color[2] = 60; }
        fronds.v.push_back(v);
      }
    }
    for (int s = 0; s < S; ++s)
      for (int c = 0; c < 2; ++c) {
        uint32_t a = base + uint32_t(s * 3 + c), b = a + 1, cc = a + 3, d = cc + 1;
        fronds.i.insert(fronds.i.end(), {a, b, d, a, d, cc});
      }
  }
  fronds.computeNormals();
  out.append(trunk, mat4(1.f));
  out.append(fronds, mat4(1.f));
}

// ---------------------------------------------------------------------------
//  Terrain
// ---------------------------------------------------------------------------

float City::terrainHeight(float x, float z) const { return statics.terrain.height(x, z); }

void City::buildTerrain(uint32_t seed) {
  Heightfield& hf = statics.terrain;
  hf.cell = 8.f;
  hf.nx = hf.nz = 400;
  hf.origin = vec2(-1600.f, -1600.f);
  hf.h.resize(size_t(hf.nx + 1) * size_t(hf.nz + 1));
  const float Ec = L.extent() + L.roadHalf;
  for (int j = 0; j <= hf.nz; ++j)
    for (int i = 0; i <= hf.nx; ++i) {
      float x = hf.origin.x + float(i) * hf.cell, z = hf.origin.y + float(j) * hf.cell;
      float n = fbm2(x * 0.0011f, z * 0.0011f, 5, 2.f, 0.5f, seed);
      float ridge = ridged2(x * 0.0008f + 3.f, z * 0.0008f - 7.f, 6, seed + 5);
      float north = smoothstepf(Ec + 40.f, Ec + 750.f, -z);
      float sides = smoothstepf(Ec + 60.f, Ec + 650.f, std::fabs(x)) * (1.f - smoothstepf(Ec - 100.f, Ec + 200.f, z));
      float mask = std::max(north, sides * 0.8f);
      float hgt = 2.4f + n * 9.f * (0.3f + mask) + ridge * 160.f * mask * mask + mask * 25.f;
      // Coast to the south: grass strip, then beach, then sea floor.
      float coast = smoothstepf(Ec, Ec + 40.f, z);
      float beach = 2.05f - std::max(0.f, z - (Ec + 42.f)) * 0.022f - std::max(0.f, z - (Ec + 260.f)) * 0.03f;
      beach += fbm2(x * 0.01f, z * 0.01f, 3, 2.f, 0.5f, seed + 9) * 0.25f * smoothstepf(Ec + 40.f, Ec + 80.f, z);
      hgt = mix(hgt, beach, coast);
      // City plateau (slightly below the asphalt slab).
      float d = std::max({std::fabs(x) - Ec, std::fabs(z) - Ec, 0.f});
      float inside = (std::fabs(x) < Ec - 2.f && std::fabs(z) < Ec - 2.f) ? 1.f : 0.f;
      float blend = smoothstepf(0.f, 70.f, d);
      hgt = inside > 0.5f ? L.groundY - 0.1f : mix(L.groundY, hgt, blend);
      hf.h[size_t(j) * size_t(hf.nx + 1) + size_t(i)] = hgt;
    }

  // Render chunks (exactly the physics triangulation).
  const int C = 8, cells = hf.nx / C;
  for (int cz = 0; cz < C; ++cz)
    for (int cx = 0; cx < C; ++cx) {
      MeshData md;
      Surface s = surf(MAT_TERRAIN, 0x707070, 0.95f);
      for (int j = 0; j <= cells; ++j)
        for (int i = 0; i <= cells; ++i) {
          int gi = cx * cells + i, gj = cz * cells + j;
          Vertex v;
          v.pos = vec3(hf.origin.x + float(gi) * hf.cell, hf.at(gi, gj), hf.origin.y + float(gj) * hf.cell);
          v.normal = hf.normal(v.pos.x + 0.01f, v.pos.z + 0.01f);
          // Smooth vertex normals from neighbours.
          float hl = hf.at(gi - 1, gj), hr = hf.at(gi + 1, gj), hd = hf.at(gi, gj - 1), hu = hf.at(gi, gj + 1);
          v.normal = glm::normalize(vec3(hl - hr, 2.f * hf.cell, hd - hu));
          v.uv = vec2(v.pos.x, v.pos.z);
          applySurface(v, s);
          md.v.push_back(v);
        }
      for (int j = 0; j < cells; ++j)
        for (int i = 0; i < cells; ++i) {
          uint32_t a = uint32_t(j * (cells + 1) + i), b = a + 1, c = a + uint32_t(cells + 1), d = c + 1;
          md.i.insert(md.i.end(), {a, c, b, b, c, d});
        }
      // Skip chunks fully hidden under the city slab.
      auto m = std::make_unique<Mesh>();
      m->upload(md);
      chunks.push_back(std::move(m));
      chunkShadows.push_back(true);
    }
}

// ---------------------------------------------------------------------------
//  Prop meshes
// ---------------------------------------------------------------------------

Mesh* City::propMesh(PropType t, int variant) {
  int idx = propMeshIndex_[size_t(t)] + variant;
  return propMeshes[size_t(idx)].get();
}

void City::buildPropMeshes() {
  propMeshIndex_.assign(8, 0);
  auto push = [&](MeshData& md) {
    auto m = std::make_unique<Mesh>();
    m->upload(md);
    propMeshes.push_back(std::move(m));
    return int(propMeshes.size()) - 1;
  };
  Surface metal = surf(MAT_METAL, 0x3b3f43, 0.45f, 0.7f);
  // Street light: origin at the base, arm towards +X.
  {
    MeshData md;
    addCylinder(md, vec3(0), vec3(0, 1, 0), 0.22f, 0.2f, 0.6f, 14, metal);
    addCylinder(md, vec3(0, 0.6f, 0), vec3(0, 1, 0), 0.13f, 0.085f, 7.7f, 14, metal);
    addCylinder(md, vec3(0, 7.9f, 0), glm::normalize(vec3(1, 0.12f, 0)), 0.06f, 0.05f, 2.1f, 8, metal);
    addBox(md, vec3(2.1f, 8.12f, 0), vec3(0.42f, 0.07f, 0.2f), metal);
    addBox(md, vec3(2.1f, 8.03f, 0), vec3(0.34f, 0.025f, 0.15f), surf(MAT_EMISSIVE, 0xffe2b8, 0.2f, 0.f, 1.f));
    propMeshIndex_[size_t(PropType::StreetLight)] = push(md);
  }
  // Traffic light housing (front faces -Z) + three separate lenses.
  {
    MeshData md;
    Surface dark = surf(MAT_METAL, 0x1c1d1e, 0.5f, 0.5f);
    addCylinder(md, vec3(0), vec3(0, 1, 0), 0.12f, 0.1f, 4.6f, 12, metal);
    addBox(md, vec3(0, 4.25f, -0.18f), vec3(0.2f, 0.55f, 0.14f), dark);
    for (int i = 0; i < 3; ++i)
      addBox(md, vec3(0, 4.62f - float(i) * 0.36f, -0.36f), vec3(0.17f, 0.02f, 0.06f), dark);  // visors
    propMeshIndex_[size_t(PropType::TrafficLight)] = push(md);
    const uint32_t colors[3] = {0xff2010, 0xffa000, 0x20ff60};
    for (int i = 0; i < 3; ++i) {
      MeshData l;
      addEllipsoid(l, vec3(0, 4.58f - float(i) * 0.36f, -0.31f), vec3(0.12f, 0.12f, 0.03f), 14, 6,
                   surf(MAT_EMISSIVE, colors[i], 0.2f, 0.f, 0.35f));
      lensMeshes_[i] = push(l);
    }
  }
  {
    MeshData md;
    Surface red = surf(MAT_METAL, 0xb01810, 0.5f, 0.3f);
    addCylinder(md, vec3(0), vec3(0, 1, 0), 0.16f, 0.14f, 0.65f, 12, red);
    addEllipsoid(md, vec3(0, 0.65f, 0), vec3(0.15f, 0.12f, 0.15f), 12, 6, red);
    addCylinder(md, vec3(-0.25f, 0.42f, 0), vec3(1, 0, 0), 0.06f, 0.06f, 0.5f, 8, red);
    propMeshIndex_[size_t(PropType::Hydrant)] = push(md);
  }
  {
    MeshData md;
    addCylinder(md, vec3(0), vec3(0, 1, 0), 0.3f, 0.32f, 1.0f, 14, surf(MAT_METAL, 0x2a3a2e, 0.55f, 0.4f));
    addCylinder(md, vec3(0, 1.0f, 0), vec3(0, 1, 0), 0.34f, 0.3f, 0.08f, 14, surf(MAT_METAL, 0x222222, 0.5f, 0.5f));
    propMeshIndex_[size_t(PropType::Bin)] = push(md);
  }
  {
    MeshData md;
    Surface wood = surf(MAT_DEFAULT, 0x6a4a30, 0.75f);
    for (int i = 0; i < 4; ++i) addBox(md, vec3(0, 0.45f, -0.25f + float(i) * 0.14f), vec3(0.9f, 0.025f, 0.055f), wood);
    for (int i = 0; i < 3; ++i) addBox(md, vec3(0, 0.62f + float(i) * 0.13f, 0.23f), vec3(0.9f, 0.05f, 0.02f), wood);
    addBox(md, vec3(-0.8f, 0.4f, 0), vec3(0.04f, 0.4f, 0.3f), metal);
    addBox(md, vec3(0.8f, 0.4f, 0), vec3(0.04f, 0.4f, 0.3f), metal);
    propMeshIndex_[size_t(PropType::Bench)] = push(md);
  }
  // Palm variants.
  propMeshIndex_[size_t(PropType::Palm)] = int(propMeshes.size());
  palmHeights_.clear();
  for (int v = 0; v < 5; ++v) {
    MeshData md;
    float h;
    buildPalm(md, uint32_t(v * 31 + 7), h);
    push(md);
    palmHeights_.push_back(h);
  }
}

void City::addProp(PropType t, const vec3& base, float yaw, PhysicsWorld& physics, int inter, int facing) {
  auto p = std::make_unique<Prop>();
  p->type = t;
  p->intersection = inter;
  p->facing = facing;
  RigidBody& b = p->body;
  b.kind = BodyKind::Prop;
  b.user = p.get();
  b.pinned = true;
  b.surface = SurfaceType::Metal;
  b.rot = glm::angleAxis(yaw, vec3(0, 1, 0));
  float halfH = 0.5f;
  switch (t) {
    case PropType::StreetLight:
      halfH = 4.15f;
      b.setBoxMass(160.f, vec3(0.3f, 8.3f, 0.3f));
      b.boxHalf = vec3(0.16f, halfH, 0.16f);
      b.breakImpulse = 3200.f;
      b.hull = {{{0, -4.0f, 0}, 0.2f}, {{0, -1.5f, 0}, 0.12f}, {{0, 1.5f, 0}, 0.1f}, {{0, 3.8f, 0}, 0.1f}, {{2.0f, 3.95f, 0}, 0.16f}};
      break;
    case PropType::TrafficLight:
      halfH = 2.4f;
      b.setBoxMass(120.f, vec3(0.3f, 4.8f, 0.3f));
      b.boxHalf = vec3(0.16f, halfH, 0.16f);
      b.breakImpulse = 3600.f;
      b.hull = {{{0, -2.3f, 0}, 0.16f}, {{0, 0.f, 0}, 0.12f}, {{0, 1.9f, -0.2f}, 0.22f}};
      break;
    case PropType::Hydrant:
      halfH = 0.4f;
      b.setBoxMass(90.f, vec3(0.34f, 0.8f, 0.34f));
      b.boxHalf = vec3(0.17f, halfH, 0.17f);
      b.breakImpulse = 2600.f;
      b.hull = {{{0, -0.25f, 0}, 0.16f}, {{0, 0.2f, 0}, 0.16f}};
      break;
    case PropType::Bin:
      halfH = 0.52f;
      b.setBoxMass(35.f, vec3(0.64f, 1.05f, 0.64f));
      b.boxHalf = vec3(0.32f, halfH, 0.32f);
      b.breakImpulse = 250.f;
      b.hull = {{{0, -0.3f, 0}, 0.3f}, {{0, 0.25f, 0}, 0.3f}};
      b.surface = SurfaceType::Metal;
      break;
    case PropType::Bench:
      halfH = 0.42f;
      b.setBoxMass(60.f, vec3(1.8f, 0.85f, 0.6f));
      b.boxHalf = vec3(0.9f, halfH, 0.3f);
      b.breakImpulse = 600.f;
      b.hull = {{{-0.7f, -0.1f, 0}, 0.3f}, {{0.f, -0.1f, 0}, 0.3f}, {{0.7f, -0.1f, 0}, 0.3f}};
      b.surface = SurfaceType::Wood;
      break;
    default: break;
  }
  b.pos = base + vec3(0, halfH, 0);
  b.friction = 0.6f;
  b.restitution = 0.1f;
  p->meshOffset = vec3(0, -halfH, 0);
  p->mesh = propMesh(t);
  physics.add(&b);
  p->prevModel = b.interpolatedMatrix(1.f) * glm::translate(mat4(1.f), p->meshOffset);
  props.push_back(std::move(p));
}

// ---------------------------------------------------------------------------
//  Blocks, buildings, streets
// ---------------------------------------------------------------------------

void City::buildBlocks(uint32_t seed, PhysicsWorld& physics) {
  const float P = L.pitch, W = L.roadHalf, SW = L.sidewalk, gy = L.groundY, ch = L.curbH;
  const int K = L.roadsPerSide;
  const float E = L.extent(), Ec = E + W;
  const int CH = 4;  // city chunks per side
  const float chunkSize = 2.f * Ec / float(CH);
  std::vector<MeshData> cm(size_t(CH * CH));
  auto chunkOf = [&](float x, float z) -> MeshData& {
    int cx = std::clamp(int((x + Ec) / chunkSize), 0, CH - 1), cz = std::clamp(int((z + Ec) / chunkSize), 0, CH - 1);
    return cm[size_t(cz * CH + cx)];
  };

  // Asphalt ground: one quad per chunk (markings are analytic in the shader).
  for (int cz = 0; cz < CH; ++cz)
    for (int cx = 0; cx < CH; ++cx) {
      float x0 = -Ec + float(cx) * chunkSize, z0 = -Ec + float(cz) * chunkSize;
      addQuad(cm[size_t(cz * CH + cx)], vec3(x0, gy, z0 + chunkSize), vec3(x0 + chunkSize, gy, z0 + chunkSize),
              vec3(x0 + chunkSize, gy, z0), vec3(x0, gy, z0), surf(MAT_ASPHALT, 0x202020, 0.9f));
    }
  statics.boxes.push_back(staticAABB(vec3(-Ec, gy - 2.f, -Ec), vec3(Ec, gy, Ec), SurfaceType::Asphalt));

  Rng rng(seed);
  Surface sidewalk = surf(MAT_SIDEWALK, 0x8a8782, 0.85f);
  Surface curb = surf(MAT_CURB, 0x9c9a95, 0.8f);
  Surface roofS = surf(MAT_ROOF, 0x404040, 0.9f);
  Surface grass = surf(MAT_GRASS, 0x406020, 0.92f);
  Surface acUnit = surf(MAT_METAL, 0xa8aaa8, 0.55f, 0.6f);

  for (int bz = -K; bz < K; ++bz)
    for (int bx = -K; bx < K; ++bx) {
      float cx = (float(bx) + 0.5f) * P, cz = (float(bz) + 0.5f) * P;
      float bh = P * 0.5f - W;  // half size curb to curb
      MeshData& md = chunkOf(cx, cz);
      float top = gy + ch;
      uint32_t bseed = hash2i(bx, bz) ^ seed;
      float rnd = hashf(bseed);
      float ring = std::max(std::fabs(float(bx) + 0.5f), std::fabs(float(bz) + 0.5f));  // 0.5 .. K-0.5
      bool beachRow = bz == K - 1;
      bool park = !beachRow && ring > 1.f && rnd < 0.09f;
      int zone = ring < 1.f ? 0 : ring < 2.f ? 1 : ring < 3.f ? 2 : 3;
      if (beachRow) zone = 4;

      // Sidewalk slab with granite curbs.
      float inner = bh - 0.3f;
      addQuad(md, vec3(cx - inner, top, cz + inner), vec3(cx + inner, top, cz + inner), vec3(cx + inner, top, cz - inner),
              vec3(cx - inner, top, cz - inner), park ? sidewalk : sidewalk);
      addAABox(md, vec3(cx - bh, gy - 0.05f, cz - bh), vec3(cx + bh, top, cz - inner), curb);
      addAABox(md, vec3(cx - bh, gy - 0.05f, cz + inner), vec3(cx + bh, top, cz + bh), curb);
      addAABox(md, vec3(cx - bh, gy - 0.05f, cz - inner), vec3(cx - inner, top, cz + inner), curb);
      addAABox(md, vec3(cx + inner, gy - 0.05f, cz - inner), vec3(cx + bh, top, cz + inner), curb);
      statics.boxes.push_back(staticAABB(vec3(cx - bh, gy - 0.5f, cz - bh), vec3(cx + bh, top, cz + bh), SurfaceType::Concrete));

      float lotHalf = bh - SW;
      if (park) {
        addQuad(md, vec3(cx - lotHalf, top + 0.02f, cz + lotHalf), vec3(cx + lotHalf, top + 0.02f, cz + lotHalf),
                vec3(cx + lotHalf, top + 0.02f, cz - lotHalf), vec3(cx - lotHalf, top + 0.02f, cz - lotHalf), grass);
        for (int i = 0; i < 18; ++i) {
          vec3 p(cx + rng.range(-lotHalf + 3.f, lotHalf - 3.f), top, cz + rng.range(-lotHalf + 3.f, lotHalf - 3.f));
          palms_.push_back({p, rng.range(0.f, TAU), rng.rangei(0, 4)});
        }
        for (int i = 0; i < 6; ++i)
          addProp(PropType::Bench, vec3(cx + rng.range(-30.f, 30.f), top, cz + rng.range(-30.f, 30.f)), rng.range(0.f, TAU), physics);
      } else {
        int n = zone == 0 ? 2 : zone == 1 ? 2 : zone == 2 ? 3 : 4;
        if (zone == 4) n = 4;
        std::vector<float> xs(size_t(n + 1)), zs(size_t(n + 1));
        for (int i = 0; i <= n; ++i) {
          float t = float(i) / float(n);
          float j = (i == 0 || i == n) ? 0.f : rng.range(-0.12f, 0.12f);
          xs[size_t(i)] = cx - lotHalf + 2.f * lotHalf * (t + j / float(n));
          zs[size_t(i)] = cz - lotHalf + 2.f * lotHalf * (t + rng.range(-0.12f, 0.12f) / float(n) * float(i != 0 && i != n));
        }
        for (int lz = 0; lz < n; ++lz)
          for (int lx = 0; lx < n; ++lx) {
            bool interior = lx > 0 && lx < n - 1 && lz > 0 && lz < n - 1;
            float x0 = xs[size_t(lx)], x1 = xs[size_t(lx + 1)], z0 = zs[size_t(lz)], z1 = zs[size_t(lz + 1)];
            uint32_t ls = hash32(bseed + uint32_t(lz * 7 + lx) * 977u);
            float lr = hashf(ls);
            if (interior && lr < 0.6f) continue;  // courtyard
            // Gaps between neighbours (alleys) on interior edges only.
            float gap = 0.f;
            if (lx > 0) x0 += rng.range(0.f, 1.f) < 0.35f ? 2.5f : gap;
            if (lz > 0) z0 += rng.range(0.f, 1.f) < 0.35f ? 2.5f : gap;
            // Parking lot instead of a building in the outskirts.
            if (zone >= 2 && lr > 0.9f && !interior) {
              addQuad(md, vec3(x0, top + 0.01f, z1), vec3(x1, top + 0.01f, z1), vec3(x1, top + 0.01f, z0), vec3(x0, top + 0.01f, z0),
                      surf(MAT_ASPHALT, 0x202020, 0.9f));
              for (float px = x0 + 3.f; px < x1 - 3.f; px += 3.2f) {
                parkingSpots.push_back(vec3(px, top + 0.01f, (z0 + z1) * 0.5f));
                parkingYaw.push_back(0.f);
              }
              continue;
            }
            float H;
            int style;
            float floorH = 3.3f;
            float r1 = hashf(ls ^ 0x51u), r2 = hashf(ls ^ 0xA7u);
            switch (zone) {
              case 0:
                H = mix(110.f, 240.f, r1);
                style = r2 < 0.5f ? 0 : (r2 < 0.75f ? 4 : 1);
                floorH = 3.9f;
                break;
              case 1:
                H = mix(55.f, 130.f, r1 * r1);
                style = r2 < 0.4f ? 0 : (r2 < 0.65f ? 1 : (r2 < 0.85f ? 4 : 2));
                floorH = 3.7f;
                break;
              case 2:
                H = mix(16.f, 48.f, r1);
                style = r2 < 0.35f ? 1 : (r2 < 0.7f ? 2 : (r2 < 0.9f ? 3 : 0));
                floorH = 3.3f;
                break;
              case 4:
                H = mix(7.f, 20.f, r1);
                style = 3;
                floorH = 3.2f;
                break;
              default:
                H = mix(7.f, 18.f, r1);
                style = r2 < 0.45f ? 2 : (r2 < 0.9f ? 3 : 1);
                floorH = 3.15f;
                break;
            }
            if (interior) H *= 0.6f;
            H = std::max(H, floorH * 2.f + 1.f);
            H = std::floor(H / floorH) * floorH + 0.6f;
            Surface fac = surf(MAT_FACADE, 0xffffff);
            float colW = style == 0 ? 1.6f : mix(2.4f, 3.4f, hashf(ls ^ 0x33u));
            fac.extra = vec4(float(ls % 1000u) * 0.37f, float(style), floorH, colW);
            float base = top;
            // Plinth / podium tiers.
            std::vector<std::pair<vec3, vec3>> tiers;
            if (H > 60.f) {
              float podium = std::floor(mix(10.f, 20.f, r2) / floorH) * floorH + 0.2f;
              tiers.push_back({vec3(x0, base, z0), vec3(x1, base + podium, z1)});
              float inset = mix(2.5f, 7.f, hashf(ls ^ 0x77u));
              float mx0 = x0 + inset, mx1 = x1 - inset, mz0 = z0 + inset, mz1 = z1 - inset;
              if (mx1 - mx0 > 10.f && mz1 - mz0 > 10.f) {
                if (H > 130.f) {
                  float mid = base + H * 0.68f;
                  tiers.push_back({vec3(mx0, base + podium, mz0), vec3(mx1, mid, mz1)});
                  float i2 = 3.f;
                  tiers.push_back({vec3(mx0 + i2, mid, mz0 + i2), vec3(mx1 - i2, base + H, mz1 - i2)});
                } else {
                  tiers.push_back({vec3(mx0, base + podium, mz0), vec3(mx1, base + H, mz1)});
                }
              } else {
                tiers[0].second.y = base + H;
              }
            } else {
              tiers.push_back({vec3(x0, base, z0), vec3(x1, base + H, z1)});
            }
            for (auto& t : tiers) {
              addBuildingBox(md, t.first, t.second, fac, roofS, base);
              statics.boxes.push_back(staticAABB(t.first, t.second, SurfaceType::Concrete));
            }
            // Roof details on the top tier.
            vec3 rmn = tiers.back().first, rmx = tiers.back().second;
            float ry = rmx.y;
            Surface parapet = surf(MAT_DEFAULT, style == 2 ? 0x7a4a38 : (style == 3 ? 0xd8d0c0 : 0x8c8a86), 0.85f);
            float pt = 0.3f, ph = 1.1f;
            addAABox(md, vec3(rmn.x, ry, rmn.z), vec3(rmx.x, ry + ph, rmn.z + pt), parapet);
            addAABox(md, vec3(rmn.x, ry, rmx.z - pt), vec3(rmx.x, ry + ph, rmx.z), parapet);
            addAABox(md, vec3(rmn.x, ry, rmn.z + pt), vec3(rmn.x + pt, ry + ph, rmx.z - pt), parapet);
            addAABox(md, vec3(rmx.x - pt, ry, rmn.z + pt), vec3(rmx.x, ry + ph, rmx.z - pt), parapet);
            if (style == 1 || style == 2 || style == 3) {
              // Cornice: slightly projecting band at the top.
              const float co = 0.35f, cw = 0.7f;
              addAABox(md, vec3(rmn.x - co, ry - 0.5f, rmn.z - co), vec3(rmx.x + co, ry + 0.1f, rmn.z - co + cw), parapet);
              addAABox(md, vec3(rmn.x - co, ry - 0.5f, rmx.z + co - cw), vec3(rmx.x + co, ry + 0.1f, rmx.z + co), parapet);
              addAABox(md, vec3(rmn.x - co, ry - 0.5f, rmn.z - co + cw), vec3(rmn.x - co + cw, ry + 0.1f, rmx.z + co - cw), parapet);
              addAABox(md, vec3(rmx.x + co - cw, ry - 0.5f, rmn.z - co + cw), vec3(rmx.x + co, ry + 0.1f, rmx.z + co - cw), parapet);
            }
            int units = 1 + int(hashf(ls ^ 0x1234u) * 4.f);
            for (int u = 0; u < units; ++u) {
              float ux = rng.range(rmn.x + 2.f, std::max(rmn.x + 2.1f, rmx.x - 4.f));
              float uz = rng.range(rmn.z + 2.f, std::max(rmn.z + 2.1f, rmx.z - 4.f));
              addAABox(md, vec3(ux, ry, uz), vec3(ux + rng.range(1.2f, 2.6f), ry + rng.range(0.8f, 1.6f), uz + rng.range(1.0f, 2.2f)), acUnit);
            }
            if ((zone == 2 || zone == 3) && hashf(ls ^ 0x999u) < 0.25f) {
              // Wooden water tower on steel legs.
              vec3 c((rmn.x + rmx.x) * 0.5f + 2.f, ry, (rmn.z + rmx.z) * 0.5f);
              for (int k = 0; k < 4; ++k) {
                float a = float(k) * PI * 0.5f + 0.785f;
                addCylinder(md, c + vec3(std::cos(a) * 1.6f, 0, std::sin(a) * 1.6f), vec3(0, 1, 0), 0.08f, 0.08f, 4.f, 6,
                            surf(MAT_METAL, 0x303030, 0.6f, 0.6f));
              }
              addCylinder(md, c + vec3(0, 4.f, 0), vec3(0, 1, 0), 2.1f, 2.f, 4.2f, 18, surf(MAT_BARK, 0x7a5c40, 0.9f));
              addCylinder(md, c + vec3(0, 8.2f, 0), vec3(0, 1, 0), 2.15f, 0.2f, 1.5f, 18, surf(MAT_METAL, 0x404448, 0.6f, 0.5f));
            }
            if (H > 180.f) {
              vec3 c = (rmn + rmx) * 0.5f;
              addCylinder(md, vec3(c.x, ry, c.z), vec3(0, 1, 0), 0.5f, 0.12f, 28.f, 8, surf(MAT_METAL, 0x9a9a9a, 0.4f, 0.8f));
              addEllipsoid(md, vec3(c.x, ry + 28.2f, c.z), vec3(0.25f), 8, 6, surf(MAT_EMISSIVE, 0xff2010, 0.3f, 0.f, 0.5f));
            }
          }
      }

      // Street furniture along the four sides of the block.
      for (int side = 0; side < 4; ++side) {
        // Side frame: along = direction along the curb, out = towards the road.
        vec3 out = side == 0 ? vec3(0, 0, -1) : side == 1 ? vec3(1, 0, 0) : side == 2 ? vec3(0, 0, 1) : vec3(-1, 0, 0);
        vec3 along(-out.z, 0, out.x);
        vec3 edgeMid = vec3(cx, top, cz) + out * bh;
        float yawToRoad = std::atan2(-out.z, out.x);  // rotates local +X onto `out`
        for (float s = -bh + 14.f; s <= bh - 14.f; s += 26.f) {
          vec3 p = edgeMid + along * s - out * 0.75f;
          addProp(PropType::StreetLight, p, yawToRoad, physics);
        }
        if (!park) {
          addProp(PropType::Hydrant, edgeMid + along * rng.range(-bh + 8.f, bh - 8.f) - out * 0.9f, rng.range(0.f, TAU), physics);
          addProp(PropType::Bin, edgeMid + along * (bh - 6.f) - out * 1.0f, 0.f, physics);
        }
        // Palms on sidewalks in the beach row and some midtown streets.
        if (zone == 4 || (zone >= 2 && hashf(bseed + uint32_t(side)) < 0.3f)) {
          for (float s = -bh + 27.f; s <= bh - 20.f; s += 26.f)
            palms_.push_back({edgeMid + along * s - out * 1.6f, rng.range(0.f, TAU), rng.rangei(0, 4)});
        }
      }
    }

  // Beach boulevard palms south of the city.
  for (float x = -Ec + 10.f; x < Ec - 10.f; x += 18.f) {
    float z = Ec + 14.f + rng.range(-2.f, 2.f);
    palms_.push_back({vec3(x + rng.range(-3.f, 3.f), 0.f, z), rng.range(0.f, TAU), rng.rangei(0, 4)});
  }

  // Traffic lights at every intersection (far-right corner of each approach).
  for (int iz = -K; iz <= K; ++iz)
    for (int ix = -K; ix <= K; ++ix) {
      vec2 c(float(ix) * P, float(iz) * P);
      intersections.push_back({c, hashf(hash2i(ix, iz)) * 36.f});
      int id = int(intersections.size()) - 1;
      const float o = W + 0.9f;
      struct Corner { vec2 off; float yawFace; int facing; };
      // Lens front is local -Z. facing = approach direction index the head serves.
      const Corner corners[4] = {
          {{o, -o}, PI, 0},        // northbound traffic (from south), head faces +Z
          {{-o, o}, 0.f, 2},       // southbound traffic, head faces -Z
          {{o, o}, PI * 0.5f, 1},  // eastbound traffic (from west), head faces -X
          {{-o, -o}, -PI * 0.5f, 3} // westbound traffic, head faces +X
      };
      for (auto& cr : corners) {
        vec2 p = c + cr.off;
        if (std::fabs(p.x) > E || std::fabs(p.y) > E) continue;
        addProp(PropType::TrafficLight, vec3(p.x, gy + ch, p.y), cr.yawFace, physics, id, cr.facing);
      }
    }

  for (int i = 0; i < CH * CH; ++i) {
    auto m = std::make_unique<Mesh>();
    m->upload(cm[size_t(i)]);
    chunks.push_back(std::move(m));
    chunkShadows.push_back(true);
  }
}

void City::buildHeightMap() {
  heightMapRes = 800;
  heightMapOrigin = vec2(-1600.f);
  heightMapSize = vec2(3200.f);
  float cell = heightMapSize.x / float(heightMapRes);
  heightMap.assign(size_t(heightMapRes) * size_t(heightMapRes), 0.f);
  for (int j = 0; j < heightMapRes; ++j)
    for (int i = 0; i < heightMapRes; ++i) {
      float x = heightMapOrigin.x + (float(i) + 0.5f) * cell, z = heightMapOrigin.y + (float(j) + 0.5f) * cell;
      heightMap[size_t(j) * size_t(heightMapRes) + size_t(i)] = std::max(statics.terrain.height(x, z), L.groundY);
    }
  for (auto& b : statics.boxes) {
    AABB a = b.box.aabb();
    if (a.max.y - a.min.y > 300.f || a.max.x - a.min.x > 300.f) continue;  // ground slab
    int i0 = std::max(0, int((a.min.x - heightMapOrigin.x) / cell)), i1 = std::min(heightMapRes - 1, int((a.max.x - heightMapOrigin.x) / cell));
    int j0 = std::max(0, int((a.min.z - heightMapOrigin.y) / cell)), j1 = std::min(heightMapRes - 1, int((a.max.z - heightMapOrigin.y) / cell));
    for (int j = j0; j <= j1; ++j)
      for (int i = i0; i <= i1; ++i) {
        float& h = heightMap[size_t(j) * size_t(heightMapRes) + size_t(i)];
        h = std::max(h, a.max.y);
      }
  }
}

void City::generate(uint32_t seed, PhysicsWorld& physics) {
  buildPropMeshes();
  buildTerrain(seed);
  buildBlocks(seed, physics);
  // Palms: ground them and add trunk colliders.
  for (auto& p : palms_) {
    if (p.pos.y <= 0.f) p.pos.y = statics.terrain.height(p.pos.x, p.pos.z);
    float h = palmHeights_[size_t(p.variant)];
    statics.boxes.push_back(staticAABB(p.pos + vec3(-0.25f, 0.f, -0.25f), p.pos + vec3(0.25f, h * 0.9f, 0.25f), SurfaceType::Wood));
  }
  statics.build(24.f);
  buildHeightMap();
  // Spawn on the sidewalk of block (0,0), next to the road at x = 0.
  spawnPoint = vec3(L.roadHalf + 2.2f, L.groundY + L.curbH, 34.f);
  spawnYaw = PI * 0.5f;  // facing -X (towards the road)
  logInfo("City: %zu props, %zu palms, %zu static boxes, %zu chunks", props.size(), palms_.size(), statics.boxes.size(), chunks.size());
}

int City::signalState(int inter, bool northSouth, float time) const {
  if (inter < 0 || inter >= int(intersections.size())) return 0;
  float t = std::fmod(time + intersections[size_t(inter)].phaseOffset, 36.f);
  if (!northSouth) t = std::fmod(t + 18.f, 36.f);
  if (t < 14.f) return 0;
  if (t < 17.f) return 1;
  return 2;
}

int City::nearestIntersection(const vec2& p) const {
  int ix = int(std::round(p.x / L.pitch)), iz = int(std::round(p.y / L.pitch));
  int K = L.roadsPerSide;
  if (std::abs(ix) > K || std::abs(iz) > K) return -1;
  return (iz + K) * (2 * K + 1) + (ix + K);
}

bool City::onRoad(const vec2& p) const {
  float E = L.extent() + L.roadHalf;
  if (std::fabs(p.x) > E || std::fabs(p.y) > E) return false;
  vec2 d = glm::abs(p - glm::round(p / L.pitch) * L.pitch);
  return d.x < L.roadHalf || d.y < L.roadHalf;
}

void City::updateProps(float dt) {
  (void)dt;
  for (auto& p : props) {
    if (p->broken || p->body.pinned) continue;
    // A street light or signal that has been knocked loose goes dark once it tilts.
    vec3 up = p->body.rot * vec3(0, 1, 0);
    if (up.y < 0.9f) p->broken = true;
  }
}

void City::submit(Renderer& r, const vec3& camPos, float night, float alpha) {
  for (size_t i = 0; i < chunks.size(); ++i) {
    DrawItem it;
    it.mesh = chunks[i].get();
    it.castShadow = chunkShadows[i];
    r.submit(it);
  }
  const float palmDist2 = 700.f * 700.f;
  for (auto& p : palms_) {
    if (glm::length2(p.pos - camPos) > palmDist2) continue;
    DrawItem it;
    it.mesh = propMeshes[size_t(propMeshIndex_[size_t(PropType::Palm)] + p.variant)].get();
    it.model = glm::translate(mat4(1.f), p.pos) * glm::rotate(mat4(1.f), p.yaw, vec3(0, 1, 0));
    it.prevModel = it.model;
    it.twoSided = true;
    it.windSway = 1.f;
    it.maxShadowCascade = 2.f;
    r.submit(it);
  }
  bool lampsOn = night > 0.25f;
  for (auto& p : props) {
    float d2 = glm::length2(p->body.pos - camPos);
    float maxD = p->type == PropType::StreetLight || p->type == PropType::TrafficLight ? 420.f : 160.f;
    if (d2 > maxD * maxD) continue;
    DrawItem it;
    it.mesh = p->mesh;
    it.model = p->body.interpolatedMatrix(alpha) * glm::translate(mat4(1.f), p->meshOffset);
    it.prevModel = p->prevModel;
    p->prevModel = it.model;
    it.maxShadowCascade = p->type == PropType::StreetLight ? 2.f : 1.f;
    bool lit = lampsOn && !p->broken;
    it.params[0].w = (p->type == PropType::StreetLight && lit) ? 1.f : 0.02f;
    r.submit(it);
    if (p->type == PropType::StreetLight && lit && d2 < 300.f * 300.f) {
      LocalLight l;
      l.pos = vec3(it.model * vec4(2.1f, 7.95f, 0.f, 1.f));
      l.range = 30.f;
      l.color = vec3(1.f, 0.78f, 0.52f) * 55.f;
      l.dir = vec3(0, -1, 0);
      l.spotOuter = 0.2f;
      l.spotInner = 0.65f;
      r.submitLight(l);
    }
    if (p->type == PropType::TrafficLight && !p->broken) {
      bool ns = p->facing == 0 || p->facing == 2;
      int st = signalState(p->intersection, ns, time_);
      for (int k = 0; k < 3; ++k) {
        DrawItem lens = it;
        lens.mesh = propMeshes[size_t(lensMeshes_[k])].get();
        lens.castShadow = false;
        lens.params[0].w = (k == st) ? 1.f : 0.004f;
        r.submit(lens);
      }
    }
  }
}

}  // namespace game
