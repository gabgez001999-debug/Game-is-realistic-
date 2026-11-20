// Headless GPU context for the cloud server: EGL without any window system.
// Prefers a real GPU via EGL_EXT_platform_device (NVIDIA/AMD), falls back to
// Mesa surfaceless (llvmpipe) so the server also runs on CPU-only machines.
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "platform/GL.h"
#include "platform/Platform.h"

namespace vx {

class HeadlessPlatform final : public Platform {
 public:
  ~HeadlessPlatform() override {
    if (dpy_ != EGL_NO_DISPLAY) {
      eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (ctx_ != EGL_NO_CONTEXT) eglDestroyContext(dpy_, ctx_);
      eglTerminate(dpy_);
    }
  }

  bool init(const PlatformConfig& cfg) override {
    w_ = cfg.width;
    h_ = cfg.height;
    auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    auto queryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto queryDeviceString = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));

    // 1) Real GPU devices first (skip software rasterizers).
    if (getPlatformDisplay && queryDevices) {
      EGLDeviceEXT devices[16];
      EGLint n = 0;
      if (queryDevices(16, devices, &n)) {
        for (EGLint i = 0; i < n && dpy_ == EGL_NO_DISPLAY; ++i) {
          const char* ext = queryDeviceString ? queryDeviceString(devices[i], EGL_EXTENSIONS) : nullptr;
          if (ext && std::string(ext).find("EGL_MESA_device_software") != std::string::npos) continue;
          EGLDisplay d = getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
          if (d != EGL_NO_DISPLAY && tryInit(d)) dpy_ = d;
        }
      }
    }
    // 2) Mesa surfaceless (software fallback).
    if (dpy_ == EGL_NO_DISPLAY && getPlatformDisplay) {
      EGLDisplay d = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
      if (d != EGL_NO_DISPLAY && tryInit(d)) dpy_ = d;
    }
    // 3) Default display.
    if (dpy_ == EGL_NO_DISPLAY) {
      EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      if (d != EGL_NO_DISPLAY && tryInit(d)) dpy_ = d;
    }
    if (dpy_ == EGL_NO_DISPLAY) { logError("No EGL display available"); return false; }

    eglBindAPI(EGL_OPENGL_API);
    const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config = nullptr;
    EGLint numCfg = 0;
    eglChooseConfig(dpy_, cfgAttr, &config, 1, &numCfg);
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 1,
                              EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    ctx_ = eglCreateContext(dpy_, numCfg ? config : EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ctxAttr);
    if (ctx_ == EGL_NO_CONTEXT) { logError("eglCreateContext failed (0x%x)", eglGetError()); return false; }
    if (!eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx_)) {
      logError("eglMakeCurrent (surfaceless) failed (0x%x)", eglGetError());
      return false;
    }
    if (!gl::load([](const char* n) { return reinterpret_cast<void*>(eglGetProcAddress(n)); })) return false;
    logInfo("Headless OpenGL: %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    // Offscreen output target.
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { logError("Output FBO incomplete"); return false; }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
  }

  void pollEvents(Input& in) override {
    in.beginFrame();
    queue_.drainInto(in);
  }
  void present() override { glFlush(); }
  int width() const override { return w_; }
  int height() const override { return h_; }
  unsigned outputFramebuffer() const override { return fbo_; }
  bool isHeadless() const override { return true; }

 private:
  bool tryInit(EGLDisplay d) {
    EGLint major = 0, minor = 0;
    if (!eglInitialize(d, &major, &minor)) return false;
    logInfo("EGL %d.%d: %s", major, minor, eglQueryString(d, EGL_VENDOR));
    return true;
  }

  EGLDisplay dpy_ = EGL_NO_DISPLAY;
  EGLContext ctx_ = EGL_NO_CONTEXT;
  GLuint fbo_ = 0, tex_ = 0;
  int w_ = 0, h_ = 0;
};

std::unique_ptr<Platform> createHeadlessPlatform() { return std::make_unique<HeadlessPlatform>(); }

}  // namespace vx
