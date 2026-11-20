#pragma once
// Veritas own OpenGL 4.1 core loader (no GLEW/glad). On macOS the system
// headers already export every core entry point.

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
namespace vx::gl {
inline bool load(void* (*)(const char*)) { return true; }
}
#else
#if defined(_WIN32)
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#endif
#include <GL/glcorearb.h>

#define VX_GL_FUNCS(X)                                                        \
  X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                                  \
  X(PFNGLATTACHSHADERPROC, glAttachShader)                                    \
  X(PFNGLBINDBUFFERPROC, glBindBuffer)                                        \
  X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                              \
  X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)                            \
  X(PFNGLBINDTEXTUREPROC, glBindTexture)                                      \
  X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                              \
  X(PFNGLBLENDEQUATIONPROC, glBlendEquation)                                  \
  X(PFNGLBLENDFUNCPROC, glBlendFunc)                                          \
  X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)                          \
  X(PFNGLBLENDFUNCIPROC, glBlendFunci)                                        \
  X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                              \
  X(PFNGLBUFFERDATAPROC, glBufferData)                                        \
  X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                                  \
  X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)                \
  X(PFNGLCLEARPROC, glClear)                                                  \
  X(PFNGLCLEARCOLORPROC, glClearColor)                                        \
  X(PFNGLCLEARDEPTHPROC, glClearDepth)                                        \
  X(PFNGLCLEARBUFFERFVPROC, glClearBufferfv)                                  \
  X(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync)                                \
  X(PFNGLCOLORMASKPROC, glColorMask)                                          \
  X(PFNGLCOLORMASKIPROC, glColorMaski)                                        \
  X(PFNGLCOMPILESHADERPROC, glCompileShader)                                  \
  X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                  \
  X(PFNGLCREATESHADERPROC, glCreateShader)                                    \
  X(PFNGLCULLFACEPROC, glCullFace)                                            \
  X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                                  \
  X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                        \
  X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                                  \
  X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)                      \
  X(PFNGLDELETESHADERPROC, glDeleteShader)                                    \
  X(PFNGLDELETESYNCPROC, glDeleteSync)                                        \
  X(PFNGLDELETETEXTURESPROC, glDeleteTextures)                                \
  X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                        \
  X(PFNGLDEPTHFUNCPROC, glDepthFunc)                                          \
  X(PFNGLDEPTHMASKPROC, glDepthMask)                                          \
  X(PFNGLDISABLEPROC, glDisable)                                              \
  X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray)            \
  X(PFNGLDRAWARRAYSPROC, glDrawArrays)                                        \
  X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced)                      \
  X(PFNGLDRAWBUFFERPROC, glDrawBuffer)                                        \
  X(PFNGLDRAWBUFFERSPROC, glDrawBuffers)                                      \
  X(PFNGLDRAWELEMENTSPROC, glDrawElements)                                    \
  X(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced)                  \
  X(PFNGLENABLEPROC, glEnable)                                                \
  X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)              \
  X(PFNGLFENCESYNCPROC, glFenceSync)                                          \
  X(PFNGLFINISHPROC, glFinish)                                                \
  X(PFNGLFLUSHPROC, glFlush)                                                  \
  X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)              \
  X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                    \
  X(PFNGLFRAMEBUFFERTEXTURELAYERPROC, glFramebufferTextureLayer)              \
  X(PFNGLFRONTFACEPROC, glFrontFace)                                          \
  X(PFNGLGENBUFFERSPROC, glGenBuffers)                                        \
  X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                              \
  X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)                            \
  X(PFNGLGENTEXTURESPROC, glGenTextures)                                      \
  X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                              \
  X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                                \
  X(PFNGLGETERRORPROC, glGetError)                                            \
  X(PFNGLGETINTEGERVPROC, glGetIntegerv)                                      \
  X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                          \
  X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                    \
  X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                            \
  X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                      \
  X(PFNGLGETSTRINGPROC, glGetString)                                          \
  X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                        \
  X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                      \
  X(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange)                                \
  X(PFNGLPIXELSTOREIPROC, glPixelStorei)                                      \
  X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset)                                  \
  X(PFNGLREADBUFFERPROC, glReadBuffer)                                        \
  X(PFNGLREADPIXELSPROC, glReadPixels)                                        \
  X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)                      \
  X(PFNGLSCISSORPROC, glScissor)                                              \
  X(PFNGLSHADERSOURCEPROC, glShaderSource)                                    \
  X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                                        \
  X(PFNGLTEXIMAGE3DPROC, glTexImage3D)                                        \
  X(PFNGLTEXPARAMETERFPROC, glTexParameterf)                                  \
  X(PFNGLTEXPARAMETERFVPROC, glTexParameterfv)                                \
  X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                                  \
  X(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D)                                  \
  X(PFNGLUNIFORM1FPROC, glUniform1f)                                          \
  X(PFNGLUNIFORM1FVPROC, glUniform1fv)                                        \
  X(PFNGLUNIFORM1IPROC, glUniform1i)                                          \
  X(PFNGLUNIFORM2FPROC, glUniform2f)                                          \
  X(PFNGLUNIFORM2FVPROC, glUniform2fv)                                        \
  X(PFNGLUNIFORM3FPROC, glUniform3f)                                          \
  X(PFNGLUNIFORM3FVPROC, glUniform3fv)                                        \
  X(PFNGLUNIFORM4FPROC, glUniform4f)                                          \
  X(PFNGLUNIFORM4FVPROC, glUniform4fv)                                        \
  X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                            \
  X(PFNGLUNMAPBUFFERPROC, glUnmapBuffer)                                      \
  X(PFNGLUSEPROGRAMPROC, glUseProgram)                                        \
  X(PFNGLVERTEXATTRIB4FPROC, glVertexAttrib4f)                                \
  X(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor)                      \
  X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)                      \
  X(PFNGLVIEWPORTPROC, glViewport)

#define VX_GL_DECLARE(type, name) extern type vxgl_##name;
VX_GL_FUNCS(VX_GL_DECLARE)
#undef VX_GL_DECLARE

#define glActiveTexture vxgl_glActiveTexture
#define glAttachShader vxgl_glAttachShader
#define glBindBuffer vxgl_glBindBuffer
#define glBindFramebuffer vxgl_glBindFramebuffer
#define glBindRenderbuffer vxgl_glBindRenderbuffer
#define glBindTexture vxgl_glBindTexture
#define glBindVertexArray vxgl_glBindVertexArray
#define glBlendEquation vxgl_glBlendEquation
#define glBlendFunc vxgl_glBlendFunc
#define glBlendFuncSeparate vxgl_glBlendFuncSeparate
#define glBlendFunci vxgl_glBlendFunci
#define glBlitFramebuffer vxgl_glBlitFramebuffer
#define glBufferData vxgl_glBufferData
#define glBufferSubData vxgl_glBufferSubData
#define glCheckFramebufferStatus vxgl_glCheckFramebufferStatus
#define glClear vxgl_glClear
#define glClearColor vxgl_glClearColor
#define glClearDepth vxgl_glClearDepth
#define glClearBufferfv vxgl_glClearBufferfv
#define glClientWaitSync vxgl_glClientWaitSync
#define glColorMask vxgl_glColorMask
#define glColorMaski vxgl_glColorMaski
#define glCompileShader vxgl_glCompileShader
#define glCreateProgram vxgl_glCreateProgram
#define glCreateShader vxgl_glCreateShader
#define glCullFace vxgl_glCullFace
#define glDeleteBuffers vxgl_glDeleteBuffers
#define glDeleteFramebuffers vxgl_glDeleteFramebuffers
#define glDeleteProgram vxgl_glDeleteProgram
#define glDeleteRenderbuffers vxgl_glDeleteRenderbuffers
#define glDeleteShader vxgl_glDeleteShader
#define glDeleteSync vxgl_glDeleteSync
#define glDeleteTextures vxgl_glDeleteTextures
#define glDeleteVertexArrays vxgl_glDeleteVertexArrays
#define glDepthFunc vxgl_glDepthFunc
#define glDepthMask vxgl_glDepthMask
#define glDisable vxgl_glDisable
#define glDisableVertexAttribArray vxgl_glDisableVertexAttribArray
#define glDrawArrays vxgl_glDrawArrays
#define glDrawArraysInstanced vxgl_glDrawArraysInstanced
#define glDrawBuffer vxgl_glDrawBuffer
#define glDrawBuffers vxgl_glDrawBuffers
#define glDrawElements vxgl_glDrawElements
#define glDrawElementsInstanced vxgl_glDrawElementsInstanced
#define glEnable vxgl_glEnable
#define glEnableVertexAttribArray vxgl_glEnableVertexAttribArray
#define glFenceSync vxgl_glFenceSync
#define glFinish vxgl_glFinish
#define glFlush vxgl_glFlush
#define glFramebufferRenderbuffer vxgl_glFramebufferRenderbuffer
#define glFramebufferTexture2D vxgl_glFramebufferTexture2D
#define glFramebufferTextureLayer vxgl_glFramebufferTextureLayer
#define glFrontFace vxgl_glFrontFace
#define glGenBuffers vxgl_glGenBuffers
#define glGenFramebuffers vxgl_glGenFramebuffers
#define glGenRenderbuffers vxgl_glGenRenderbuffers
#define glGenTextures vxgl_glGenTextures
#define glGenVertexArrays vxgl_glGenVertexArrays
#define glGenerateMipmap vxgl_glGenerateMipmap
#define glGetError vxgl_glGetError
#define glGetIntegerv vxgl_glGetIntegerv
#define glGetProgramInfoLog vxgl_glGetProgramInfoLog
#define glGetProgramiv vxgl_glGetProgramiv
#define glGetShaderInfoLog vxgl_glGetShaderInfoLog
#define glGetShaderiv vxgl_glGetShaderiv
#define glGetString vxgl_glGetString
#define glGetUniformLocation vxgl_glGetUniformLocation
#define glLinkProgram vxgl_glLinkProgram
#define glMapBufferRange vxgl_glMapBufferRange
#define glPixelStorei vxgl_glPixelStorei
#define glPolygonOffset vxgl_glPolygonOffset
#define glReadBuffer vxgl_glReadBuffer
#define glReadPixels vxgl_glReadPixels
#define glRenderbufferStorage vxgl_glRenderbufferStorage
#define glScissor vxgl_glScissor
#define glShaderSource vxgl_glShaderSource
#define glTexImage2D vxgl_glTexImage2D
#define glTexImage3D vxgl_glTexImage3D
#define glTexParameterf vxgl_glTexParameterf
#define glTexParameterfv vxgl_glTexParameterfv
#define glTexParameteri vxgl_glTexParameteri
#define glTexSubImage2D vxgl_glTexSubImage2D
#define glUniform1f vxgl_glUniform1f
#define glUniform1fv vxgl_glUniform1fv
#define glUniform1i vxgl_glUniform1i
#define glUniform2f vxgl_glUniform2f
#define glUniform2fv vxgl_glUniform2fv
#define glUniform3f vxgl_glUniform3f
#define glUniform3fv vxgl_glUniform3fv
#define glUniform4f vxgl_glUniform4f
#define glUniform4fv vxgl_glUniform4fv
#define glUniformMatrix4fv vxgl_glUniformMatrix4fv
#define glUnmapBuffer vxgl_glUnmapBuffer
#define glUseProgram vxgl_glUseProgram
#define glVertexAttrib4f vxgl_glVertexAttrib4f
#define glVertexAttribDivisor vxgl_glVertexAttribDivisor
#define glVertexAttribPointer vxgl_glVertexAttribPointer
#define glViewport vxgl_glViewport

namespace vx::gl {
// Resolves every entry point through the given proc-address function.
bool load(void* (*getProc)(const char*));
}
#endif
