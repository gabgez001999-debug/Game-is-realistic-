#include "gfx/Shader.h"

#include <set>

#include "core/FileSystem.h"

namespace vx {

Shader::~Shader() {
  if (prog_) glDeleteProgram(prog_);
}

static bool preprocess(const std::string& file, std::string& out, std::set<std::string>& seen, int depth) {
  if (depth > 8) return false;
  std::string src;
  if (!readTextFile(dataPath("shaders/" + file), src)) {
    logError("Shader file not found: %s", file.c_str());
    return false;
  }
  size_t pos = 0;
  while (pos < src.size()) {
    size_t end = src.find('\n', pos);
    if (end == std::string::npos) end = src.size();
    std::string line = src.substr(pos, end - pos);
    size_t inc = line.find("#include");
    if (inc != std::string::npos && line.find_first_not_of(" \t") == inc) {
      size_t a = line.find('"'), b = line.rfind('"');
      std::string name = line.substr(a + 1, b - a - 1);
      if (!seen.count(name)) {
        seen.insert(name);
        if (!preprocess(name, out, seen, depth + 1)) return false;
      }
    } else {
      out += line;
      out += '\n';
    }
    pos = end + 1;
  }
  return true;
}

GLuint Shader::compile(GLenum type, const std::string& file) {
  std::string body;
  std::set<std::string> seen;
  if (!preprocess(file, body, seen, 0)) return 0;
  std::string src = "#version 410 core\n";
  for (auto& d : defines_) src += "#define " + d + "\n";
  src += type == GL_VERTEX_SHADER ? "#define VERTEX_SHADER 1\n" : "#define FRAGMENT_SHADER 1\n";
  src += "#line 1\n";
  src += body;
  GLuint s = glCreateShader(type);
  const char* p = src.c_str();
  glShaderSource(s, 1, &p, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    logError("Shader compile error in %s:\n%s", file.c_str(), log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

bool Shader::load(const std::string& vs, const std::string& fs, const std::vector<std::string>& defines) {
  vs_ = vs; fs_ = fs; defines_ = defines;
  return reload();
}

bool Shader::reload() {
  GLuint v = compile(GL_VERTEX_SHADER, vs_);
  GLuint f = compile(GL_FRAGMENT_SHADER, fs_);
  if (!v || !f) {
    if (v) glDeleteShader(v);
    if (f) glDeleteShader(f);
    return false;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetProgramInfoLog(p, sizeof(log), nullptr, log);
    logError("Shader link error (%s, %s):\n%s", vs_.c_str(), fs_.c_str(), log);
    glDeleteProgram(p);
    return false;
  }
  if (prog_) glDeleteProgram(prog_);
  prog_ = p;
  locs_.clear();
  return true;
}

GLint Shader::loc(const char* name) {
  auto it = locs_.find(name);
  if (it != locs_.end()) return it->second;
  GLint l = glGetUniformLocation(prog_, name);
  locs_.emplace(name, l);
  return l;
}

void Shader::tex(const char* n, int unit, GLenum target, GLuint texture) {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(target, texture);
  glUniform1i(loc(n), unit);
}

}  // namespace vx
