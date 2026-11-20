#include "core/Image.h"

#include <fstream>

namespace vx {

static uint32_t crcTable[256];
static void initCrc() {
  static bool done = false;
  if (done) return;
  for (uint32_t n = 0; n < 256; ++n) {
    uint32_t c = n;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
  done = true;
}
static uint32_t crc(const uint8_t* buf, size_t len, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < len; ++i) c = crcTable[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
  return c;
}

static void put32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16)); v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x));
}

static void chunk(std::ofstream& f, const char* type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> buf;
  put32(buf, uint32_t(data.size()));
  size_t typeStart = buf.size();
  buf.insert(buf.end(), type, type + 4);
  buf.insert(buf.end(), data.begin(), data.end());
  uint32_t c = crc(buf.data() + typeStart, buf.size() - typeStart) ^ 0xFFFFFFFFu;
  put32(buf, c);
  f.write(reinterpret_cast<const char*>(buf.data()), std::streamsize(buf.size()));
}

bool writePNG(const std::string& path, int w, int h, const uint8_t* rgba, bool flipY) {
  initCrc();
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  f.write(reinterpret_cast<const char*>(sig), 8);

  std::vector<uint8_t> ihdr;
  put32(ihdr, uint32_t(w)); put32(ihdr, uint32_t(h));
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
  chunk(f, "IHDR", ihdr);

  // Raw scanlines with filter byte 0.
  std::vector<uint8_t> raw;
  raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
  for (int y = 0; y < h; ++y) {
    int sy = flipY ? (h - 1 - y) : y;
    raw.push_back(0);
    const uint8_t* row = rgba + size_t(sy) * size_t(w) * 4;
    raw.insert(raw.end(), row, row + size_t(w) * 4);
  }
  // zlib stream of stored blocks.
  std::vector<uint8_t> z;
  z.push_back(0x78); z.push_back(0x01);
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = std::min<size_t>(65535, raw.size() - pos);
    bool last = pos + n == raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(uint8_t(n & 0xFF)); z.push_back(uint8_t(n >> 8));
    z.push_back(uint8_t(~n & 0xFF)); z.push_back(uint8_t((~n >> 8) & 0xFF));
    z.insert(z.end(), raw.begin() + long(pos), raw.begin() + long(pos + n));
    pos += n;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521u; b = (b + a) % 65521u; }
  put32(z, (b << 16) | a);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  return true;
}

}  // namespace vx
