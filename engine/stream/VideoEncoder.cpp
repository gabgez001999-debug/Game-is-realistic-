#include "stream/VideoEncoder.h"

#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

namespace vx {

VideoEncoder::~VideoEncoder() {
  if (pkt_) av_packet_free(&pkt_);
  if (frame_) av_frame_free(&frame_);
  if (ctx_) avcodec_free_context(&ctx_);
}

bool VideoEncoder::open(const char* encoderName) {
  const AVCodec* codec = avcodec_find_encoder_by_name(encoderName);
  if (!codec) return false;
  AVCodecContext* c = avcodec_alloc_context3(codec);
  c->width = w_;
  c->height = h_;
  c->time_base = AVRational{1, 1000000};
  c->framerate = AVRational{fps_, 1};
  c->pix_fmt = AV_PIX_FMT_YUV420P;
  c->gop_size = fps_ * 2;
  c->max_b_frames = 0;
  c->bit_rate = int64_t(kbps_) * 1000;
  c->rc_max_rate = c->bit_rate;
  c->rc_buffer_size = int(c->bit_rate / fps_ * 2);
  c->thread_count = 0;
  c->color_primaries = AVCOL_PRI_BT709;
  c->color_trc = AVCOL_TRC_BT709;
  c->colorspace = AVCOL_SPC_BT709;
  c->color_range = AVCOL_RANGE_MPEG;
  if (std::strcmp(encoderName, "h264_nvenc") == 0) {
    av_opt_set(c->priv_data, "preset", "p2", 0);
    av_opt_set(c->priv_data, "tune", "ull", 0);
    av_opt_set(c->priv_data, "rc", "cbr", 0);
    av_opt_set(c->priv_data, "zerolatency", "1", 0);
    av_opt_set(c->priv_data, "forced-idr", "1", 0);
    av_opt_set(c->priv_data, "delay", "0", 0);
    av_opt_set(c->priv_data, "profile", "high", 0);
  } else if (std::strcmp(encoderName, "libx264") == 0) {
    av_opt_set(c->priv_data, "preset", w_ * h_ > 1280 * 720 ? "ultrafast" : "superfast", 0);
    av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    av_opt_set(c->priv_data, "profile", "main", 0);
    av_opt_set(c->priv_data, "x264-params", "repeat-headers=1:scenecut=0:aud=0", 0);
  }
  if (avcodec_open2(c, codec, nullptr) < 0) {
    avcodec_free_context(&c);
    return false;
  }
  ctx_ = c;
  name_ = encoderName;
  return true;
}

bool VideoEncoder::init(int width, int height, int fps, int bitrateKbps, const std::string& preferred) {
  w_ = width; h_ = height; fps_ = fps; kbps_ = bitrateKbps;
  std::vector<std::string> order;
  if (!preferred.empty()) order.push_back(preferred);
  order.push_back("h264_nvenc");
  order.push_back("libx264");
  for (auto& n : order)
    if (open(n.c_str())) break;
  if (!ctx_) { logError("No H.264 encoder available"); return false; }
  frame_ = av_frame_alloc();
  frame_->format = AV_PIX_FMT_YUV420P;
  frame_->width = w_;
  frame_->height = h_;
  pkt_ = av_packet_alloc();
  logInfo("Video encoder: %s %dx%d@%d %d kbit/s", name_.c_str(), w_, h_, fps_, kbps_);
  return true;
}

static std::string spsCodecString(const uint8_t* d, size_t n) {
  for (size_t i = 0; i + 7 < n; ++i) {
    bool sc4 = d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 0 && d[i + 3] == 1;
    bool sc3 = d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1;
    if (!sc3 && !sc4) continue;
    size_t p = i + (sc4 ? 4 : 3);
    if ((d[p] & 0x1F) == 7 && p + 3 < n) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "avc1.%02x%02x%02x", d[p + 1], d[p + 2], d[p + 3]);
      return buf;
    }
  }
  return {};
}

bool VideoEncoder::encode(const uint8_t* i420, int64_t pts, bool forceKey, const PacketFn& out) {
  if (!ctx_) return false;
  // Point the frame planes directly at the caller's buffer.
  frame_->data[0] = const_cast<uint8_t*>(i420);
  frame_->data[1] = const_cast<uint8_t*>(i420) + size_t(w_) * size_t(h_);
  frame_->data[2] = frame_->data[1] + size_t(w_ / 2) * size_t(h_ / 2);
  frame_->linesize[0] = w_;
  frame_->linesize[1] = w_ / 2;
  frame_->linesize[2] = w_ / 2;
  frame_->pts = pts;
  frame_->pict_type = forceKey ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;
#if LIBAVUTIL_VERSION_MAJOR >= 58
  if (forceKey) frame_->flags |= AV_FRAME_FLAG_KEY;
  else frame_->flags &= ~AV_FRAME_FLAG_KEY;
#else
  frame_->key_frame = forceKey ? 1 : 0;
#endif
  if (avcodec_send_frame(ctx_, frame_) < 0) return false;
  while (true) {
    int r = avcodec_receive_packet(ctx_, pkt_);
    if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
    if (r < 0) return false;
    bool key = (pkt_->flags & AV_PKT_FLAG_KEY) != 0;
    if (key && codecString_.empty()) codecString_ = spsCodecString(pkt_->data, size_t(pkt_->size));
    out(pkt_->data, size_t(pkt_->size), key, pkt_->pts);
    av_packet_unref(pkt_);
  }
  return true;
}

}  // namespace vx
