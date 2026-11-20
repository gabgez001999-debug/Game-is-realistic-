#pragma once
#include "core/Common.h"

namespace vx {

// Writes an 8-bit RGBA PNG (zlib "stored" blocks, no external dependency).
bool writePNG(const std::string& path, int w, int h, const uint8_t* rgba, bool flipY);

}  // namespace vx
