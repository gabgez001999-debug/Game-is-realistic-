#pragma once
#include "core/Common.h"
#include "platform/GL.h"

namespace vx {

struct Texture {
  GLuint id = 0;
  GLenum target = GL_TEXTURE_2D;
  int w = 0, h = 0, layers = 1, levels = 1;
  GLenum internal = GL_RGBA8;

  void create2D(int width, int height, GLenum internalFmt, GLenum fmt, GLenum type, const void* data = nullptr,
                GLenum filter = GL_LINEAR, GLenum wrap = GL_CLAMP_TO_EDGE, bool mips = false);
  void createArray(int width, int height, int layerCount, GLenum internalFmt, GLenum fmt, GLenum type);
  void createCube(int size, GLenum internalFmt, GLenum fmt, GLenum type, bool mips);
  void destroy();
  void generateMips();
};

// Framebuffer wrapper with up to 4 color attachments.
struct Framebuffer {
  GLuint id = 0;
  int w = 0, h = 0;
  void create();
  void attach(GLenum attachment, const Texture& t, int level = 0);
  void attachLayer(GLenum attachment, const Texture& t, int layer, int level = 0);
  void attachCubeFace(GLenum attachment, const Texture& t, int face, int level = 0);
  void setDrawBuffers(int count);
  bool check(const char* name) const;
  void bind() const;
  void destroy();
};

}  // namespace vx
