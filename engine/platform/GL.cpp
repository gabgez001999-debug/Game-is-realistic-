#include "platform/GL.h"

#include "core/Common.h"

#if !defined(__APPLE__)

#define VX_GL_DEFINE(type, name) type vxgl_##name = nullptr;
VX_GL_FUNCS(VX_GL_DEFINE)
#undef VX_GL_DEFINE

namespace vx::gl {

bool load(void* (*getProc)(const char*)) {
  int missing = 0;
#define VX_GL_LOAD(type, name)                                     \
  vxgl_##name = reinterpret_cast<type>(getProc(#name));            \
  if (!vxgl_##name) { logError("GL entry point missing: %s", #name); ++missing; }
  VX_GL_FUNCS(VX_GL_LOAD)
#undef VX_GL_LOAD
  return missing == 0;
}

}  // namespace vx::gl
#endif
