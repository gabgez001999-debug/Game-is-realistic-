#pragma once
#include "core/Common.h"

namespace vx {

bool readTextFile(const std::string& path, std::string& out);
bool readBinaryFile(const std::string& path, std::vector<uint8_t>& out);
bool fileExists(const std::string& path);

// Locates the data root (folder containing "shaders/" and "web/").
// Search order: $VERITAS_DATA, cwd, executable dir and parents, compile-time source dir.
const std::string& dataRoot();
void setExecutablePath(const char* argv0);
std::string dataPath(const std::string& rel);

}  // namespace vx
