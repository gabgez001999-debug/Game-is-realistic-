#include "gfx/Renderer.h"

#include <cstddef>

#include "core/Image.h"

namespace vx {

static constexpr int kSkySize = 128;
static constexpr int kLumSize = 128;
static constexpr int kMaxLights = 64;
static constexpr float kShadowDistance = 460.f;

bool Renderer::reloadShaders() {
  bool ok = true;
  ok &= gbufferShader_.load("gbuffer.vert", "gbuffer.frag");
  ok &= shadowShader_.load("shadow.vert", "shadow.frag");
  ok &= skyShader_.load("fullscreen.vert", "sky_cube.frag");
  ok &= ssaoShader_.load("fullscreen.vert", "ssao.frag");
  ok &= ssaoBlurShader_.load("fullscreen.vert", "ssao_blur.frag");
  ok &= lightingShader_.load("fullscreen.vert", "lighting.frag");
  ok &= compositeShader_.load("fullscreen.vert", "composite.frag");
  ok &= waterShader_.load("water.vert", "water.frag");
  ok &= particleShader_.load("particles.vert", "particles.frag");
  ok &= bloomDownShader_.load("fullscreen.vert", "bloom_down.frag");
  ok &= bloomUpShader_.load("fullscreen.vert", "bloom_up.frag");
  ok &= lumShader_.load("fullscreen.vert", "lum.frag");
  ok &= adaptShader_.load("fullscreen.vert", "adapt.frag");
  ok &= postShader_.load("fullscreen.vert", "post.frag");
  ok &= fxaaShader_.load("fullscreen.vert", "fxaa.frag");
  skyUpdateCounter_ = 0;
  return ok;
}

bool Renderer::init(int w, int h) {
  if (!reloadShaders()) return false;
  glGenVertexArrays(1, &emptyVao_);

  // Cascaded shadow map array with hardware depth comparison.
  shadowMap_.createArray(settings.shadowSize, settings.shadowSize, 4, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
  glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMap_.id);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  shadowFbo_.create();

  skyCube_.createCube(kSkySize, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
  skyFbo_.create();

  // 1x1 empty heightmap until the game provides one.
  float zero = -1000.f;
  heightMap_.create2D(1, 1, GL_R32F, GL_RED, GL_FLOAT, &zero);

  lum_.create2D(kLumSize, kLumSize, GL_R16F, GL_RED, GL_HALF_FLOAT, nullptr, GL_LINEAR, GL_CLAMP_TO_EDGE, true);
  lumFbo_.create();
  lumFbo_.attach(GL_COLOR_ATTACHMENT0, lum_);
  lumFbo_.check("lum");
  for (int i = 0; i < 2; ++i) {
    float one = 0.f;
    adapt_[i].create2D(1, 1, GL_R32F, GL_RED, GL_FLOAT, &one, GL_NEAREST);
    adaptFbo_[i].create();
    adaptFbo_[i].attach(GL_COLOR_ATTACHMENT0, adapt_[i]);
    adaptFbo_[i].check("adapt");
  }

  // Instanced particle buffer.
  glGenVertexArrays(1, &particleVao_);
  glGenBuffers(1, &particleVbo_);
  glBindVertexArray(particleVao_);
  glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
  const GLsizei st = sizeof(GpuParticle);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(GpuParticle, posSize));
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(GpuParticle, color));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(GpuParticle, params));
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(GpuParticle, velocity));
  for (int a = 0; a < 4; ++a) glVertexAttribDivisor(GLuint(a), 1);
  glBindVertexArray(0);

  // Water: grid with quadratic spacing (dense near the camera).
  {
    MeshData md;
    const int N = 160;
    auto coord = [](int i) { float t = float(i) / float(N) * 2.f - 1.f; return signf(t) * t * t * 3000.f; };
    for (int z = 0; z <= N; ++z)
      for (int x = 0; x <= N; ++x) {
        Vertex v;
        v.pos = vec3(coord(x), 0.f, coord(z));
        md.v.push_back(v);
      }
    for (int z = 0; z < N; ++z)
      for (int x = 0; x < N; ++x) {
        uint32_t a = uint32_t(z * (N + 1) + x), b = a + 1, c = a + uint32_t(N + 1), d = c + 1;
        md.i.insert(md.i.end(), {a, c, b, b, c, d});
      }
    waterMesh_.upload(md);
  }

  resize(w, h);
  return true;
}

void Renderer::resize(int w, int h) {
  if (w == w_ && h == h_) return;
  w_ = std::max(w, 16);
  h_ = std::max(h, 16);
  createTargets();
}

void Renderer::createTargets() {
  int w = w_, h = h_;
  gAlbedo_.create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, GL_NEAREST);
  gNormal_.create2D(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr, GL_NEAREST);
  gEmissive_.create2D(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr, GL_NEAREST);
  gVelocity_.create2D(w, h, GL_RG16F, GL_RG, GL_HALF_FLOAT, nullptr, GL_NEAREST);
  gDepth_.create2D(w, h, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr, GL_NEAREST);
  gFbo_.create();
  gFbo_.attach(GL_COLOR_ATTACHMENT0, gAlbedo_);
  gFbo_.attach(GL_COLOR_ATTACHMENT1, gNormal_);
  gFbo_.attach(GL_COLOR_ATTACHMENT2, gEmissive_);
  gFbo_.attach(GL_COLOR_ATTACHMENT3, gVelocity_);
  gFbo_.attach(GL_DEPTH_ATTACHMENT, gDepth_);
  gFbo_.setDrawBuffers(4);
  gFbo_.check("gbuffer");

  for (int i = 0; i < 2; ++i) {
    ao_[i].create2D(w / 2, h / 2, GL_R8, GL_RED, GL_UNSIGNED_BYTE, nullptr, GL_LINEAR);
    aoFbo_[i].create();
    aoFbo_[i].attach(GL_COLOR_ATTACHMENT0, ao_[i]);
    aoFbo_[i].check("ao");
  }

  lit_.create2D(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr, GL_LINEAR);
  envSpec_.create2D(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr, GL_NEAREST);
  litFbo_.create();
  litFbo_.attach(GL_COLOR_ATTACHMENT0, lit_);
  litFbo_.attach(GL_COLOR_ATTACHMENT1, envSpec_);
  litFbo_.setDrawBuffers(2);
  litFbo_.check("lit");

  hdr_.create2D(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr, GL_LINEAR);
  hdrFbo_.create();
  hdrFbo_.attach(GL_COLOR_ATTACHMENT0, hdr_);
  hdrFbo_.attach(GL_DEPTH_ATTACHMENT, gDepth_);
  hdrFbo_.setDrawBuffers(1);
  hdrFbo_.check("hdr");
  hdrCopy_.create2D(w, h, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, nullptr, GL_LINEAR);
  hdrCopyFbo_.create();
  hdrCopyFbo_.attach(GL_COLOR_ATTACHMENT0, hdrCopy_);
  depthCopy_.create2D(w, h, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr, GL_NEAREST);
  hdrCopyFbo_.attach(GL_DEPTH_ATTACHMENT, depthCopy_);
  hdrCopyFbo_.check("hdrCopy");

  for (auto& t : bloomMips_) t.destroy();
  for (auto& f : bloomFbos_) f.destroy();
  bloomMips_.clear();
  bloomFbos_.clear();
  int bw = w / 2, bh = h / 2;
  for (int i = 0; i < 6 && bw >= 4 && bh >= 4; ++i) {
    Texture t;
    t.create2D(bw, bh, GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT, nullptr, GL_LINEAR);
    Framebuffer f;
    f.create();
    f.attach(GL_COLOR_ATTACHMENT0, t);
    f.check("bloom");
    bloomMips_.push_back(t);
    bloomFbos_.push_back(f);
    bw /= 2; bh /= 2;
  }

  ldr_.create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, GL_LINEAR);
  ldrFbo_.create();
  ldrFbo_.attach(GL_COLOR_ATTACHMENT0, ldr_);
  ldrFbo_.check("ldr");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::setHeightMap(const std::vector<float>& heights, int w, int h, vec2 origin, vec2 size) {
  heightMap_.create2D(w, h, GL_R32F, GL_RED, GL_FLOAT, heights.data(), GL_LINEAR);
  heightMapInfo_ = vec4(origin, size);
}

void Renderer::beginFrame(const Camera& cam, const Environment& env, float time, float dt) {
  if (hasPrev_) prevViewProj_ = cam_.viewProj();
  cam_ = cam;
  if (!hasPrev_) { prevViewProj_ = cam_.viewProj(); hasPrev_ = true; }
  env_ = env;
  time_ = time;
  dt_ = dt;
  items_.clear();
  lights_.clear();
  particles_.clear();
}

void Renderer::setFrameUniforms(Shader& s) {
  mat4 vp = cam_.viewProj();
  s.set("uView", cam_.view);
  s.set("uProj", cam_.proj);
  s.set("uViewProj", vp);
  s.set("uInvViewProj", glm::inverse(vp));
  s.set("uPrevViewProj", prevViewProj_);
  s.set("uInvProj", glm::inverse(cam_.proj));
  s.set("uCamPos", cam_.pos);
  s.set("uSunDir", env_.lightDir);
  s.set("uSunColor", env_.lightColor);
  s.set("uTrueSunDir", env_.sunDir);
  s.set("uTime", time_);
  s.set("uResolution", vec2(float(w_), float(h_)));
  s.set("uWetness", env_.wetness);
  s.set("uRain", env_.rain);
  s.set("uCloudCover", env_.cloudCover);
  s.set("uFogDensity", env_.fogDensity);
  s.set("uNight", env_.night);
  s.set("uWind", env_.wind);
}

void Renderer::drawFullscreen() {
  glBindVertexArray(emptyVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

static float maxScale(const mat4& m) {
  return std::sqrt(std::max({glm::length2(vec3(m[0])), glm::length2(vec3(m[1])), glm::length2(vec3(m[2]))}));
}

void Renderer::drawItem(Shader& s, const DrawItem& it) {
  s.set("uModel", it.model);
  s.set("uPrevModel", it.prevModel);
  s.set("uNormalMat", mat4(glm::transpose(glm::inverse(mat3(it.model)))));
  s.setArray("uParams", it.params, 8);
  s.set("uWindSway", it.windSway);
  if (it.twoSided) glDisable(GL_CULL_FACE);
  else glEnable(GL_CULL_FACE);
  it.mesh->draw();
}

void Renderer::updateSkyCube() {
  bool sunMoved = glm::length(env_.sunDir - lastSkySun_) > 0.002f;
  bool weather = std::fabs(env_.cloudCover - lastSkyCloud_) > 0.01f || std::fabs(env_.rain - lastSkyRain_) > 0.01f;
  if (skyUpdateCounter_-- > 0 && !sunMoved && !weather) return;
  skyUpdateCounter_ = 30;
  lastSkySun_ = env_.sunDir;
  lastSkyCloud_ = env_.cloudCover;
  lastSkyRain_ = env_.rain;

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  skyShader_.use();
  setFrameUniforms(skyShader_);
  skyShader_.set("uSunIntensity", env_.sunIntensity);
  skyShader_.set("uGroundColor", env_.groundColor);
  for (int f = 0; f < 6; ++f) {
    skyFbo_.attachCubeFace(GL_COLOR_ATTACHMENT0, skyCube_, f);
    skyFbo_.setDrawBuffers(1);
    skyFbo_.bind();
    skyShader_.set("uFace", f);
    drawFullscreen();
  }
  skyCube_.generateMips();
}

void Renderer::renderShadows() {
  // Practical split scheme.
  const float n = cam_.nearZ, f = kShadowDistance, lambda = 0.8f;
  float splits[5];
  splits[0] = n;
  for (int i = 1; i <= 4; ++i) {
    float t = float(i) / 4.f;
    float logS = n * std::pow(f / n, t);
    float linS = n + (f - n) * t;
    splits[i] = lambda * logS + (1.f - lambda) * linS;
  }
  vec3 L = glm::normalize(env_.lightDir);
  shadowLightDir_ = L;
  vec3 up = std::fabs(L.y) > 0.99f ? vec3(0, 0, 1) : vec3(0, 1, 0);
  mat4 invView = glm::inverse(cam_.view);
  float th = std::tan(cam_.fovY * 0.5f), tw = th * cam_.aspect;
  int size = settings.shadowSize;

  for (int c = 0; c < 4; ++c) {
    float zn = splits[c], zf = splits[c + 1];
    vec3 corners[8];
    int k = 0;
    for (float z : {zn, zf})
      for (int sy = -1; sy <= 1; sy += 2)
        for (int sx = -1; sx <= 1; sx += 2)
          corners[k++] = vec3(invView * vec4(float(sx) * tw * z, float(sy) * th * z, -z, 1.f));
    vec3 center(0.f);
    for (auto& p : corners) center += p;
    center /= 8.f;
    float radius = 0.f;
    for (auto& p : corners) radius = std::max(radius, glm::length(p - center));
    radius = std::ceil(radius * 4.f) / 4.f;
    mat4 lv = glm::lookAt(center + L * (radius + 600.f), center, up);
    mat4 lp = glm::ortho(-radius, radius, -radius, radius, 1.f, radius * 2.f + 1200.f);
    // Texel snapping to avoid shimmering.
    mat4 m = lp * lv;
    vec4 origin = m * vec4(0, 0, 0, 1);
    origin *= float(size) * 0.5f;
    vec4 rounded = glm::round(origin);
    vec4 off = (rounded - origin) * (2.f / float(size));
    lp[3][0] += off.x;
    lp[3][1] += off.y;
    shadowMats_[c] = lp * lv;
    cascadeFar_[c] = zf;
    cascadeTexel_[c] = 2.f * radius / float(size);
    cascadeCenter_[c] = center;
    cascadeRadius_[c] = radius;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_.id);
  glViewport(0, 0, size, size);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.5f, 2.5f);
  shadowShader_.use();
  setFrameUniforms(shadowShader_);
  stats.shadowCalls = 0;
  for (int c = 0; c < 4; ++c) {
    shadowFbo_.attachLayer(GL_DEPTH_ATTACHMENT, shadowMap_, c);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadowShader_.set("uLightViewProj", shadowMats_[c]);
    mat4 lv = glm::lookAt(cascadeCenter_[c] + L * (cascadeRadius_[c] + 600.f), cascadeCenter_[c], up);
    for (const DrawItem& it : items_) {
      if (!it.castShadow || float(c) > it.maxShadowCascade) continue;
      const AABB& b = it.mesh->bounds();
      vec3 wc = vec3(it.model * vec4(b.center(), 1.f));
      float r = glm::length(b.extent()) * maxScale(it.model);
      vec3 ls = vec3(lv * vec4(wc, 1.f));
      if (std::fabs(ls.x) > cascadeRadius_[c] + r || std::fabs(ls.y) > cascadeRadius_[c] + r) continue;
      shadowShader_.set("uModel", it.model);
      shadowShader_.set("uWindSway", it.windSway);
      it.mesh->draw();
      ++stats.shadowCalls;
    }
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::renderGBuffer() {
  gFbo_.bind();
  const float zero[4] = {0, 0, 0, 0};
  const float nclear[4] = {0, 1, 0, 1};
  glClearBufferfv(GL_COLOR, 0, zero);
  glClearBufferfv(GL_COLOR, 1, nclear);
  glClearBufferfv(GL_COLOR, 2, zero);
  glClearBufferfv(GL_COLOR, 3, zero);
  glDepthMask(GL_TRUE);
  glClearDepth(1.0);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDisable(GL_BLEND);
  glCullFace(GL_BACK);
  gbufferShader_.use();
  setFrameUniforms(gbufferShader_);
  gbufferShader_.set("uCity", city_);
  Frustum fr;
  fr.fromMatrix(cam_.viewProj());
  stats.drawCalls = 0;
  for (const DrawItem& it : items_) {
    const AABB& b = it.mesh->bounds();
    vec3 wc = vec3(it.model * vec4(b.center(), 1.f));
    float r = glm::length(b.extent()) * maxScale(it.model);
    if (!fr.sphere(wc, r)) continue;
    drawItem(gbufferShader_, it);
    ++stats.drawCalls;
  }
  glEnable(GL_CULL_FACE);
}

void Renderer::renderSSAO() {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  if (!settings.ssao) {
    aoFbo_[1].bind();
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }
  aoFbo_[0].bind();
  ssaoShader_.use();
  setFrameUniforms(ssaoShader_);
  ssaoShader_.set("uRadius", 1.2f);
  ssaoShader_.tex("uDepth", 0, GL_TEXTURE_2D, gDepth_.id);
  ssaoShader_.tex("uNormal", 1, GL_TEXTURE_2D, gNormal_.id);
  drawFullscreen();
  aoFbo_[1].bind();
  ssaoBlurShader_.use();
  ssaoBlurShader_.set("uTexel", vec2(1.f / float(ao_[0].w), 1.f / float(ao_[0].h)));
  ssaoBlurShader_.tex("uAO", 0, GL_TEXTURE_2D, ao_[0].id);
  ssaoBlurShader_.tex("uDepth", 1, GL_TEXTURE_2D, gDepth_.id);
  drawFullscreen();
}

void Renderer::renderLighting() {
  litFbo_.bind();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  Shader& s = lightingShader_;
  s.use();
  setFrameUniforms(s);
  s.tex("gAlbedo", 0, GL_TEXTURE_2D, gAlbedo_.id);
  s.tex("gNormal", 1, GL_TEXTURE_2D, gNormal_.id);
  s.tex("gEmissive", 2, GL_TEXTURE_2D, gEmissive_.id);
  s.tex("gDepth", 3, GL_TEXTURE_2D, gDepth_.id);
  s.tex("uAO", 4, GL_TEXTURE_2D, ao_[1].id);
  s.tex("uShadowMap", 5, GL_TEXTURE_2D_ARRAY, shadowMap_.id);
  s.tex("uSkyCube", 6, GL_TEXTURE_CUBE_MAP, skyCube_.id);
  s.tex("uHeightMap", 7, GL_TEXTURE_2D, heightMap_.id);
  s.setArray("uShadowMats", shadowMats_, 4);
  s.set("uCascadeFar", vec4(cascadeFar_[0], cascadeFar_[1], cascadeFar_[2], cascadeFar_[3]));
  s.set("uCascadeTexel", vec4(cascadeTexel_[0], cascadeTexel_[1], cascadeTexel_[2], cascadeTexel_[3]));
  s.set("uHeightMapInfo", heightMapInfo_);
  s.set("uSkyMips", float(skyCube_.levels - 1));
  s.set("uSunDiscIntensity", 4000.f);

  // Nearest, visible lights.
  Frustum fr;
  fr.fromMatrix(cam_.viewProj());
  std::vector<const LocalLight*> vis;
  for (auto& l : lights_)
    if (fr.sphere(l.pos, l.range)) vis.push_back(&l);
  std::sort(vis.begin(), vis.end(), [&](const LocalLight* a, const LocalLight* b) {
    return glm::length2(a->pos - cam_.pos) < glm::length2(b->pos - cam_.pos);
  });
  int n = std::min<int>(int(vis.size()), kMaxLights);
  vec4 lp[kMaxLights], lc[kMaxLights], ld[kMaxLights];
  for (int i = 0; i < n; ++i) {
    lp[i] = vec4(vis[size_t(i)]->pos, vis[size_t(i)]->range);
    lc[i] = vec4(vis[size_t(i)]->color, vis[size_t(i)]->spotOuter);
    ld[i] = vec4(vis[size_t(i)]->dir, vis[size_t(i)]->spotInner);
  }
  s.set("uLightCount", n);
  if (n > 0) {
    s.setArray("uLightPos", lp, n);
    s.setArray("uLightColor", lc, n);
    s.setArray("uLightDir", ld, n);
  }
  stats.lights = n;
  drawFullscreen();
}

void Renderer::renderComposite() {
  glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_.id);
  glViewport(0, 0, w_, h_);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  Shader& s = compositeShader_;
  s.use();
  setFrameUniforms(s);
  s.tex("uLit", 0, GL_TEXTURE_2D, lit_.id);
  s.tex("uEnvSpec", 1, GL_TEXTURE_2D, envSpec_.id);
  s.tex("gAlbedo", 2, GL_TEXTURE_2D, gAlbedo_.id);
  s.tex("gNormal", 3, GL_TEXTURE_2D, gNormal_.id);
  s.tex("gDepth", 4, GL_TEXTURE_2D, gDepth_.id);
  s.tex("uSkyCube", 5, GL_TEXTURE_CUBE_MAP, skyCube_.id);
  s.set("uSkyMips", float(skyCube_.levels - 1));
  drawFullscreen();
  glDepthMask(GL_TRUE);
}

void Renderer::renderForward() {
  bool needCopy = waterEnabled_ || !particles_.empty();
  if (!needCopy) return;
  // Copy scene color for refraction / soft particles.
  glBindFramebuffer(GL_READ_FRAMEBUFFER, hdrFbo_.id);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrCopyFbo_.id);
  glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

  glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_.id);
  glViewport(0, 0, w_, h_);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  // Depth is tested against the live buffer but sampled from the copy (no feedback loop).
  if (waterEnabled_) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Shader& s = waterShader_;
    s.use();
    setFrameUniforms(s);
    const vec4 waves[6] = {{1.0f, 0.3f, 60.f, 0.10f}, {0.7f, 0.9f, 31.f, 0.09f}, {-0.3f, 1.0f, 18.f, 0.08f},
                           {0.9f, -0.4f, 9.f, 0.07f},  {0.2f, 0.8f, 5.f, 0.05f},  {-0.8f, 0.5f, 3.1f, 0.04f}};
    s.setArray("uWaves", waves, 6);
    s.tex("uSceneColor", 0, GL_TEXTURE_2D, hdrCopy_.id);
    s.tex("uSceneDepth", 1, GL_TEXTURE_2D, depthCopy_.id);
    s.tex("uSkyCube", 2, GL_TEXTURE_CUBE_MAP, skyCube_.id);
    s.set("uSkyMips", float(skyCube_.levels - 1));
    s.set("uModel", mat4(1.f));
    waterMesh_.draw();
  }

  if (!particles_.empty()) {
    std::sort(particles_.begin(), particles_.end(), [&](const GpuParticle& a, const GpuParticle& b) {
      return glm::length2(vec3(a.posSize) - cam_.pos) > glm::length2(vec3(b.posSize) - cam_.pos);
    });
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    Shader& s = particleShader_;
    s.use();
    setFrameUniforms(s);
    s.tex("uSceneDepth", 0, GL_TEXTURE_2D, depthCopy_.id);
    s.tex("uSkyCube", 1, GL_TEXTURE_CUBE_MAP, skyCube_.id);
    s.set("uSkyMips", float(skyCube_.levels - 1));
    glBindVertexArray(particleVao_);
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(particles_.size() * sizeof(GpuParticle)), particles_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(particles_.size()));
  }
  stats.particles = int(particles_.size());
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
}

void Renderer::renderBloom() {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  if (bloomMips_.empty()) return;
  bloomDownShader_.use();
  GLuint src = hdr_.id;
  vec2 texel(1.f / float(w_), 1.f / float(h_));
  for (size_t i = 0; i < bloomMips_.size(); ++i) {
    bloomFbos_[i].bind();
    bloomDownShader_.tex("uSrc", 0, GL_TEXTURE_2D, src);
    bloomDownShader_.set("uSrcTexel", texel);
    bloomDownShader_.set("uFirst", i == 0 ? 1 : 0);
    drawFullscreen();
    src = bloomMips_[i].id;
    texel = vec2(1.f / float(bloomMips_[i].w), 1.f / float(bloomMips_[i].h));
  }
  bloomUpShader_.use();
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  for (size_t i = bloomMips_.size() - 1; i > 0; --i) {
    bloomFbos_[i - 1].bind();
    bloomUpShader_.tex("uSrc", 0, GL_TEXTURE_2D, bloomMips_[i].id);
    bloomUpShader_.set("uSrcTexel", vec2(1.f / float(bloomMips_[i].w), 1.f / float(bloomMips_[i].h)));
    bloomUpShader_.set("uRadius", 1.f);
    drawFullscreen();
  }
  glDisable(GL_BLEND);
}

void Renderer::renderExposure() {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  lumFbo_.bind();
  lumShader_.use();
  lumShader_.tex("uSrc", 0, GL_TEXTURE_2D, hdr_.id);
  drawFullscreen();
  lum_.generateMips();
  int next = 1 - adaptIdx_;
  adaptFbo_[next].bind();
  adaptShader_.use();
  adaptShader_.tex("uLum", 0, GL_TEXTURE_2D, lum_.id);
  adaptShader_.tex("uPrev", 1, GL_TEXTURE_2D, adapt_[adaptIdx_].id);
  adaptShader_.set("uLumMip", float(lum_.levels - 1));
  adaptShader_.set("uDt", dt_);
  adaptShader_.set("uReset", frame_ < 2 ? 1.f : 0.f);
  drawFullscreen();
  adaptIdx_ = next;
}

void Renderer::renderPost(GLuint targetFbo, int tw, int th) {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  ldrFbo_.bind();
  Shader& s = postShader_;
  s.use();
  setFrameUniforms(s);
  s.tex("uHDR", 0, GL_TEXTURE_2D, hdr_.id);
  s.tex("uBloom", 1, GL_TEXTURE_2D, bloomMips_.empty() ? hdr_.id : bloomMips_[0].id);
  s.tex("uVelocity", 2, GL_TEXTURE_2D, gVelocity_.id);
  s.tex("uDepth", 3, GL_TEXTURE_2D, gDepth_.id);
  s.tex("uExposure", 4, GL_TEXTURE_2D, adapt_[adaptIdx_].id);
  s.set("uBloomStrength", settings.bloom);
  s.set("uBloomNorm", 1.f / float(std::max<size_t>(1, bloomMips_.size())));
  // Shutter: 180 degrees scaled by frame time (velocity is per frame).
  s.set("uMotionBlur", settings.motionBlur);
  s.set("uExposureBias", settings.exposureBias);
  s.set("uVignette", settings.vignette);
  s.set("uGrain", settings.grain);
  s.set("uDamageFlash", settings.damageFlash);
  drawFullscreen();

  glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
  glViewport(0, 0, tw, th);
  fxaaShader_.use();
  fxaaShader_.tex("uSrc", 0, GL_TEXTURE_2D, ldr_.id);
  fxaaShader_.set("uTexel", vec2(1.f / float(w_), 1.f / float(h_)));
  drawFullscreen();
}

void Renderer::render(GLuint targetFbo, int targetW, int targetH, Hud* hud) {
  glDisable(GL_SCISSOR_TEST);
  updateSkyCube();
  renderShadows();
  renderGBuffer();
  renderSSAO();
  renderLighting();
  renderComposite();
  renderForward();
  renderBloom();
  renderExposure();
  renderPost(targetFbo, targetW, targetH);
  if (hud) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, targetW, targetH);
    hud->draw();
  }
  ++frame_;
}

bool Renderer::screenshot(GLuint fbo, int w, int h, const std::string& path) {
  std::vector<uint8_t> px(size_t(w) * size_t(h) * 4);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
  bool ok = writePNG(path, w, h, px.data(), true);
  if (ok) logInfo("Screenshot saved: %s", path.c_str());
  return ok;
}

}  // namespace vx
