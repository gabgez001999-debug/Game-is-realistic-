#include "core/Common.h"

#include <chrono>
#include <cstdarg>
#include <mutex>

namespace vx {

static std::mutex g_logMutex;

static void vlog(const char* tag, const char* fmt, va_list ap) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  double t = duration<double>(steady_clock::now() - start).count();
  std::fprintf(stderr, "[%8.3f] %s ", t, tag);
  std::vfprintf(stderr, fmt, ap);
  std::fputc('\n', stderr);
  std::fflush(stderr);
}

void logInfo(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog("INFO ", fmt, ap); va_end(ap); }
void logWarn(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog("WARN ", fmt, ap); va_end(ap); }
void logError(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog("ERROR", fmt, ap); va_end(ap); }

}  // namespace vx
