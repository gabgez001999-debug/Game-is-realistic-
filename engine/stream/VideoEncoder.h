#pragma once
#include <functional>

#include "core/Common.h"

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

namespace vx {

// H.264 encoder on top of libavcodec. Prefers NVIDIA NVENC (hardware), then
// VA-API-less software x264 tuned for zero latency. Output is Annex B with SPS/PPS
// repeated on every keyframe, which is what the browser's WebCodecs decoder expects.
class VideoEncoder {
 public:
  using PacketFn = std::function<void(const uint8_t* data, size_t size, bool key, int64_t pts)>;

  ~VideoEncoder();
  bool init(int width, int height, int fps, int bitrateKbps, const std::string& preferred = "");
  // i420: Y plane (w*h) followed by U and V planes (w/2*h/2 each).
  bool encode(const uint8_t* i420, int64_t pts, bool forceKey, const PacketFn& out);
  const std::string& name() const { return name_; }
  // WebCodecs codec string ("avc1.PPCCLL") derived from the first SPS.
  const std::string& codecString() const { return codecString_; }

 private:
  bool open(const char* encoderName);
  AVCodecContext* ctx_ = nullptr;
  AVFrame* frame_ = nullptr;
  AVPacket* pkt_ = nullptr;
  int w_ = 0, h_ = 0, fps_ = 60, kbps_ = 8000;
  std::string name_, codecString_;
};

}  // namespace vx
