#include "gfx/Hud.h"

#include <cstddef>
#include <cstring>

namespace vx {

// 5x7 bitmap glyphs, rows top to bottom ('#' = set). Lowercase maps to uppercase.
struct Glyph { char c; const char* rows[7]; };
static const Glyph kGlyphs[] = {
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
    {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
    {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
    {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {'D', {"###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."}},
    {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'N', {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"}},
    {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."}},
    {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {',', {".....", ".....", ".....", ".....", ".##..", "..#..", ".#..."}},
    {':', {".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."}},
    {'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
    {'-', {".....", ".....", ".....", "#####", ".....", ".....", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {'%', {"##...", "##..#", "...#.", "..#..", ".#...", "#..##", "...##"}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
    {'\'', {"..#..", "..#..", ".#...", ".....", ".....", ".....", "....."}},
    {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
    {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
    {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
    {'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    {'|', {"..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
};

static constexpr int kCellW = 48, kCellH = 64, kCols = 16, kRows = 6;  // ASCII 32..127
static constexpr float kPix = 8.f;                                     // texels per glyph pixel

bool Hud::init() {
  if (!shader_.load("hud.vert", "hud.frag")) return false;
  // Build a signed distance field atlas from the bitmap glyphs.
  const int W = kCellW * kCols, H = kCellH * kRows;
  std::vector<uint8_t> atlas(size_t(W) * size_t(H), 0);
  for (const Glyph& g : kGlyphs) {
    int idx = int(g.c) - 32;
    int cx = (idx % kCols) * kCellW, cy = (idx / kCols) * kCellH;
    for (int y = 0; y < kCellH; ++y)
      for (int x = 0; x < kCellW; ++x) {
        // Glyph pixel space: 5x7 centred in a 6x8 cell.
        float gx = (float(x) + 0.5f) / kPix - 0.5f, gy = (float(y) + 0.5f) / kPix - 0.5f;
        float best = 1e9f;
        bool inside = false;
        for (int r = 0; r < 7; ++r)
          for (int c = 0; c < 5; ++c) {
            if (g.rows[r][c] != '#') continue;
            float dx = std::max(std::fabs(gx - (float(c) + 0.5f)) - 0.5f, 0.f);
            float dy = std::max(std::fabs(gy - (float(r) + 0.5f)) - 0.5f, 0.f);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d <= 0.f) inside = true;
            best = std::min(best, d);
          }
        float sd = inside ? 0.f : best;  // outside distance in glyph pixels (corners get rounded)
        float v = saturate(0.5f + (0.18f - sd) * 1.1f);
        atlas[size_t(cy + y) * size_t(W) + size_t(cx + x)] = uint8_t(v * 255.f);
      }
  }
  font_.create2D(W, H, GL_R8, GL_RED, GL_UNSIGNED_BYTE, atlas.data(), GL_LINEAR, GL_CLAMP_TO_EDGE, true);

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  const GLsizei st = sizeof(V);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void*)offsetof(V, pos));
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void*)offsetof(V, uv));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(V, color));
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(V, params));
  glBindVertexArray(0);
  return true;
}

void Hud::begin(int w, int h) {
  sw_ = w; sh_ = h;
  uiScale = float(h) / 1080.f;
  verts_.clear();
}

void Hud::quad(float x, float y, float w, float h, vec2 uv0, vec2 uv1, const vec4& c, const vec4& p) {
  V a{{x, y}, {uv0.x, uv0.y}, c, p}, b{{x + w, y}, {uv1.x, uv0.y}, c, p};
  V cc{{x + w, y + h}, {uv1.x, uv1.y}, c, p}, d{{x, y + h}, {uv0.x, uv1.y}, c, p};
  verts_.insert(verts_.end(), {a, b, cc, a, cc, d});
}

void Hud::rect(float x, float y, float w, float h, const vec4& color) { quad(x, y, w, h, {0, 0}, {1, 1}, color, {0, 0, 0, 0}); }

void Hud::ring(float cx, float cy, float r, float innerFrac, const vec4& color) {
  quad(cx - r, cy - r, r * 2.f, r * 2.f, {0, 0}, {1, 1}, color, {2, innerFrac, 0, 0});
}

float Hud::textWidth(float size, const std::string& s) const { return float(s.size()) * size * 0.75f; }

void Hud::text(float x, float y, float size, const std::string& s, const vec4& color, int align, bool shadow) {
  float w = textWidth(size, s);
  if (align == 1) x -= w * 0.5f;
  if (align == 2) x -= w;
  if (shadow) text(x + size * 0.07f, y + size * 0.07f, size, s, vec4(0, 0, 0, color.a * 0.6f), 0, false);
  const float W = float(kCellW * kCols), H = float(kCellH * kRows);
  float cw = size * 0.75f, ch = size;
  for (char raw : s) {
    char c = (raw >= 'a' && raw <= 'z') ? char(raw - 32) : raw;
    int idx = int(c) - 32;
    if (idx > 0 && idx < kCols * kRows) {
      float u0 = float((idx % kCols) * kCellW) / W, v0 = float((idx / kCols) * kCellH) / H;
      quad(x, y, cw, ch, {u0, v0}, {u0 + kCellW / W, v0 + kCellH / H}, color, {1, 0, 0, 0});
    }
    x += cw;
  }
}

void Hud::radar(float x, float y, float size, const vec2& p, float heading, float mpu) {
  radarParams_ = vec4(p.x, p.y, heading, mpu);
  quad(x, y, size, size, {0, 0}, {1, 1}, vec4(1), {3, 0, 0, 0});
}

void Hud::draw() {
  if (verts_.empty()) return;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  shader_.use();
  shader_.set("uScreen", vec2(float(sw_), float(sh_)));
  shader_.set("uRadar", radarParams_);
  shader_.set("uCityInfo", cityInfo);
  shader_.tex("uFont", 0, GL_TEXTURE_2D, font_.id);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts_.size() * sizeof(V)), verts_.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts_.size()));
  glDisable(GL_BLEND);
}

}  // namespace vx
