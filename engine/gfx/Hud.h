#pragma once
#include "gfx/Shader.h"
#include "gfx/Texture.h"

namespace vx {

// Immediate-mode 2D overlay: rectangles, rings, distance-field text, radar.
class Hud {
 public:
  bool init();
  void begin(int screenW, int screenH);
  void rect(float x, float y, float w, float h, const vec4& color);
  void ring(float cx, float cy, float r, float innerFrac, const vec4& color);
  // align: 0 left, 1 center, 2 right
  void text(float x, float y, float size, const std::string& s, const vec4& color, int align = 0, bool shadow = true);
  float textWidth(float size, const std::string& s) const;
  void radar(float x, float y, float size, const vec2& playerXZ, float heading, float metersPerUnit);
  void draw();

  vec4 cityInfo{100.f, 7.5f, 500.f, 1e9f};
  float uiScale = 1.f;

 private:
  struct V { vec2 pos, uv; vec4 color, params; };
  void quad(float x, float y, float w, float h, vec2 uv0, vec2 uv1, const vec4& c, const vec4& p);
  std::vector<V> verts_;
  Shader shader_;
  Texture font_;
  GLuint vao_ = 0, vbo_ = 0;
  int sw_ = 1, sh_ = 1;
  vec4 radarParams_{0.f};
};

}  // namespace vx
