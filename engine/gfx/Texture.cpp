#include "gfx/Texture.h"

namespace vx {

void Texture::create2D(int width, int height, GLenum internalFmt, GLenum fmt, GLenum type, const void* data,
                       GLenum filter, GLenum wrap, bool mips) {
  destroy();
  target = GL_TEXTURE_2D;
  w = width; h = height; internal = internalFmt;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, GLint(internalFmt), w, h, 0, fmt, type, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GLint(filter));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLint(filter));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(wrap));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(wrap));
  levels = 1;
  if (mips) {
    glGenerateMipmap(GL_TEXTURE_2D);
    levels = 1 + int(std::floor(std::log2(float(std::max(w, h)))));
  }
}

void Texture::createArray(int width, int height, int layerCount, GLenum internalFmt, GLenum fmt, GLenum type) {
  destroy();
  target = GL_TEXTURE_2D_ARRAY;
  w = width; h = height; layers = layerCount; internal = internalFmt;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D_ARRAY, id);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GLint(internalFmt), w, h, layers, 0, fmt, type, nullptr);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Texture::createCube(int size, GLenum internalFmt, GLenum fmt, GLenum type, bool mips) {
  destroy();
  target = GL_TEXTURE_CUBE_MAP;
  w = h = size; internal = internalFmt;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_CUBE_MAP, id);
  for (int f = 0; f < 6; ++f)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GLint(internalFmt), size, size, 0, fmt, type, nullptr);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  levels = 1;
  if (mips) {
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    levels = 1 + int(std::floor(std::log2(float(size))));
  }
}

void Texture::generateMips() {
  glBindTexture(target, id);
  glGenerateMipmap(target);
}

void Texture::destroy() {
  if (id) glDeleteTextures(1, &id);
  id = 0;
}

void Framebuffer::create() {
  destroy();
  glGenFramebuffers(1, &id);
}

void Framebuffer::attach(GLenum attachment, const Texture& t, int level) {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, t.id, level);
  w = std::max(1, t.w >> level);
  h = std::max(1, t.h >> level);
}

void Framebuffer::attachLayer(GLenum attachment, const Texture& t, int layer, int level) {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, t.id, level, layer);
  w = std::max(1, t.w >> level);
  h = std::max(1, t.h >> level);
}

void Framebuffer::attachCubeFace(GLenum attachment, const Texture& t, int face, int level) {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GLenum(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face), t.id, level);
  w = std::max(1, t.w >> level);
  h = std::max(1, t.h >> level);
}

void Framebuffer::setDrawBuffers(int count) {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  if (count == 0) {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    return;
  }
  GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
  glDrawBuffers(count, bufs);
}

bool Framebuffer::check(const char* name) const {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (s != GL_FRAMEBUFFER_COMPLETE) {
    logError("Framebuffer '%s' incomplete: 0x%x", name, s);
    return false;
  }
  return true;
}

void Framebuffer::bind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, id);
  glViewport(0, 0, w, h);
}

void Framebuffer::destroy() {
  if (id) glDeleteFramebuffers(1, &id);
  id = 0;
}

}  // namespace vx
