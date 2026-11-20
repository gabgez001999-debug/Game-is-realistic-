#pragma once
#include "gfx/Camera.h"
#include "gfx/Environment.h"
#include "gfx/Hud.h"
#include "gfx/Mesh.h"
#include "gfx/Shader.h"
#include "gfx/Texture.h"

namespace vx {

struct DrawItem {
  const Mesh* mesh = nullptr;
  mat4 model{1.f};
  mat4 prevModel{1.f};
  vec4 params[8]{};
  bool castShadow = true;
  bool twoSided = false;
  float windSway = 0.f;
  float maxShadowCascade = 3.f;  // small props skip far cascades
};

struct LocalLight {
  vec3 pos{0.f};
  float range = 10.f;
  vec3 color{1.f};
  float spotOuter = -2.f;  // cosine; -2 = point light
  vec3 dir{0, -1, 0};
  float spotInner = 1.f;
};

struct GpuParticle {
  vec4 posSize;   // xyz, size
  vec4 color;     // linear rgb, alpha
  vec4 params;    // rotation, type, stretch, emissive
  vec3 velocity;
};

struct RenderSettings {
  float motionBlur = 0.45f;
  float bloom = 0.045f;
  float exposureBias = 0.f;
  float vignette = 0.45f;
  float grain = 0.025f;
  float damageFlash = 0.f;
  bool ssao = true;
  bool ssr = true;
  int shadowSize = 2048;
};

class Renderer {
 public:
  bool init(int w, int h);
  void resize(int w, int h);
  bool reloadShaders();

  void beginFrame(const Camera& cam, const Environment& env, float time, float dt);
  void submit(const DrawItem& item) { items_.push_back(item); }
  void submitLight(const LocalLight& l) { lights_.push_back(l); }
  std::vector<GpuParticle>& particles() { return particles_; }

  void setCity(const vec4& city) { city_ = city; }
  void setHeightMap(const std::vector<float>& heights, int w, int h, vec2 origin, vec2 size);
  void setWater(bool enabled, float level) { waterEnabled_ = enabled; waterLevel_ = level; }

  // Renders the frame into targetFbo (0 = window). Hud is drawn on top if given.
  void render(GLuint targetFbo, int targetW, int targetH, Hud* hud);
  bool screenshot(GLuint fbo, int w, int h, const std::string& path);

  RenderSettings settings;
  int width() const { return w_; }
  int height() const { return h_; }
  uint64_t frameIndex() const { return frame_; }
  struct Stats { int drawCalls = 0, shadowCalls = 0, lights = 0, particles = 0; } stats;

 private:
  void createTargets();
  void updateSkyCube();
  void renderShadows();
  void renderGBuffer();
  void renderSSAO();
  void renderLighting();
  void renderComposite();
  void renderForward();
  void renderBloom();
  void renderExposure();
  void renderPost(GLuint targetFbo, int tw, int th);
  void setFrameUniforms(Shader& s);
  void drawFullscreen();
  void drawItem(Shader& s, const DrawItem& it);

  int w_ = 0, h_ = 0;
  uint64_t frame_ = 0;
  float time_ = 0.f, dt_ = 0.f;
  Camera cam_;
  Environment env_;
  mat4 prevViewProj_{1.f};
  bool hasPrev_ = false;
  vec4 city_{100.f, 7.5f, 4.5f, 500.f};

  std::vector<DrawItem> items_;
  std::vector<LocalLight> lights_;
  std::vector<GpuParticle> particles_;

  // Shaders
  Shader gbufferShader_, shadowShader_, skyShader_, ssaoShader_, ssaoBlurShader_, lightingShader_, compositeShader_,
      waterShader_, particleShader_, bloomDownShader_, bloomUpShader_, lumShader_, adaptShader_, postShader_, fxaaShader_;

  // Targets
  Texture gAlbedo_, gNormal_, gEmissive_, gVelocity_, gDepth_;
  Framebuffer gFbo_;
  Texture shadowMap_;
  Framebuffer shadowFbo_;
  Texture skyCube_;
  Framebuffer skyFbo_;
  Texture ao_[2];
  Framebuffer aoFbo_[2];
  Texture lit_, envSpec_;
  Framebuffer litFbo_;
  Texture hdr_, hdrCopy_, depthCopy_;
  Framebuffer hdrFbo_, hdrCopyFbo_;
  std::vector<Texture> bloomMips_;
  std::vector<Framebuffer> bloomFbos_;
  Texture lum_;
  Framebuffer lumFbo_;
  Texture adapt_[2];
  Framebuffer adaptFbo_[2];
  int adaptIdx_ = 0;
  Texture ldr_;
  Framebuffer ldrFbo_;
  Texture heightMap_;
  vec4 heightMapInfo_{0.f};

  // Cascades
  mat4 shadowMats_[4];
  float cascadeFar_[4]{};
  float cascadeTexel_[4]{};
  vec3 cascadeCenter_[4];
  float cascadeRadius_[4]{};
  vec3 shadowLightDir_{0, 1, 0};

  GLuint emptyVao_ = 0;
  GLuint particleVao_ = 0, particleVbo_ = 0;
  // Water grid
  Mesh waterMesh_;
  bool waterEnabled_ = false;
  float waterLevel_ = 0.f;

  int skyUpdateCounter_ = 0;
  vec3 lastSkySun_{0.f};
  float lastSkyCloud_ = -1.f;
  float lastSkyRain_ = -1.f;
};

}  // namespace vx
