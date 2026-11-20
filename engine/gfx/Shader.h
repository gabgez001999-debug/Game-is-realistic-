#pragma once
#include <unordered_map>

#include "core/Common.h"
#include "platform/GL.h"

namespace vx {

// GLSL program loaded from shaders/ with #include support and injected defines.
class Shader {
 public:
  Shader() = default;
  ~Shader();
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  bool load(const std::string& vs, const std::string& fs, const std::vector<std::string>& defines = {});
  bool reload();
  void use() const { glUseProgram(prog_); }
  GLuint id() const { return prog_; }

  GLint loc(const char* name);
  void set(const char* n, int v) { glUniform1i(loc(n), v); }
  void set(const char* n, float v) { glUniform1f(loc(n), v); }
  void set(const char* n, const vec2& v) { glUniform2f(loc(n), v.x, v.y); }
  void set(const char* n, const vec3& v) { glUniform3f(loc(n), v.x, v.y, v.z); }
  void set(const char* n, const vec4& v) { glUniform4f(loc(n), v.x, v.y, v.z, v.w); }
  void set(const char* n, const mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
  void setArray(const char* n, const vec4* v, int count) { glUniform4fv(loc(n), count, glm::value_ptr(v[0])); }
  void setArray(const char* n, const mat4* m, int count) { glUniformMatrix4fv(loc(n), count, GL_FALSE, glm::value_ptr(m[0])); }
  void setArray(const char* n, const float* v, int count) { glUniform1fv(loc(n), count, v); }
  // Binds a texture to a unit and points the sampler uniform at it.
  void tex(const char* n, int unit, GLenum target, GLuint texture);

 private:
  GLuint compile(GLenum type, const std::string& file);
  GLuint prog_ = 0;
  std::string vs_, fs_;
  std::vector<std::string> defines_;
  std::unordered_map<std::string, GLint> locs_;
};

}  // namespace vx
