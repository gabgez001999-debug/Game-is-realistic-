#include "core/FileSystem.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace vx {

static std::string g_exeDir;

bool readTextFile(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::stringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

bool readBinaryFile(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  out.resize(size_t(f.tellg()));
  f.seekg(0);
  f.read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size()));
  return true;
}

bool fileExists(const std::string& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

void setExecutablePath(const char* argv0) {
  std::error_code ec;
  fs::path p = fs::weakly_canonical(fs::path(argv0), ec);
  g_exeDir = p.parent_path().string();
}

static bool isRoot(const fs::path& p) {
  std::error_code ec;
  return fs::exists(p / "shaders" / "common.glsl", ec);
}

const std::string& dataRoot() {
  static std::string root = [] {
    if (const char* env = std::getenv("VERITAS_DATA"); env && isRoot(env)) return std::string(env);
    std::vector<fs::path> candidates;
    candidates.push_back(fs::current_path());
    if (!g_exeDir.empty()) {
      fs::path d = g_exeDir;
      for (int i = 0; i < 4 && !d.empty(); ++i) { candidates.push_back(d); d = d.parent_path(); }
    }
    candidates.push_back(VERITAS_SOURCE_DIR);
    for (auto& c : candidates)
      if (isRoot(c)) return c.string();
    logError("Could not locate data root (shaders/). Set VERITAS_DATA.");
    return std::string(".");
  }();
  return root;
}

std::string dataPath(const std::string& rel) { return (fs::path(dataRoot()) / rel).string(); }

}  // namespace vx
