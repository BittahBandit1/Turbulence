#include "turbulence_pacing.hpp"
#include "VideoCommon/TurbulencePresentTimes.h"
#include "VideoCommon/VideoEvents.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace turbulence {
struct FramePacingRecorder::Impl {
  std::filesystem::path directory;
  Common::EventHook hook;
  std::FILE* file = nullptr;
  int remaining = 0;
  unsigned poll = 0;
  Clock::time_point start{};
  uint64_t glass_tail = 0;
  void OnPresent(const PresentInfo& info) {
    namespace fs = std::filesystem;
    if (!file) {
      if ((poll++ % 30u) != 0u) return;
      std::error_code ec;
      if (!fs::exists(directory / "pacing-request", ec)) return;
      fs::remove(directory / "pacing-request", ec);
      int index = 0;
      while (fs::exists(directory / ("pacing-" + std::to_string(index) + ".csv"), ec)) ++index;
      file = std::fopen((directory / ("pacing-" + std::to_string(index) + ".csv")).string().c_str(), "w");
      if (!file) return;
      remaining = 600;
      start = Clock::now();
      std::fprintf(file, "present,frame,reason,emulated_us,intended_ms,actual_ms,host_ms,accuracy,drawable_wait_ms,submit_ms,drawable_requests,drawable_failures,display_callbacks\n");
      glass_tail = TurbulencePresentTimes::Get().head.load(std::memory_order_acquire);
    }
    // On-glass times (steady_clock and CACurrentMediaTime share mach time).
    {
      auto& ring = TurbulencePresentTimes::Get();
      const uint64_t head = ring.head.load(std::memory_order_acquire);
      const double start_s = std::chrono::duration<double>(start.time_since_epoch()).count();
      for (; glass_tail < head; ++glass_tail) {
        const double t = ring.seconds[glass_tail % ring.seconds.size()].load(std::memory_order_relaxed);
        std::fprintf(file, "glass,%.3f\n", t > 0.0 ? (t - start_s) * 1000.0 : -1.0);
      }
    }
    const auto ms = [this](Clock::time_point t) {
      return t.time_since_epoch().count() == 0
                 ? -1.0
                 : std::chrono::duration<double, std::milli>(t - start).count();
    };
    auto& timing = TurbulencePresentTimes::Get();
    std::fprintf(file, "%llu,%llu,%d,%llu,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%llu,%llu,%llu\n",
                 static_cast<unsigned long long>(info.present_count),
                 static_cast<unsigned long long>(info.frame_count),
                 static_cast<int>(info.reason),
                 static_cast<unsigned long long>(info.emulated_timestamp),
                 ms(info.intended_present_time), ms(info.actual_present_time),
                 ms(Clock::now()), static_cast<int>(info.present_time_accuracy),
                 timing.drawable_wait_ms.load(std::memory_order_relaxed),
                 timing.submit_ms.load(std::memory_order_relaxed),
                 static_cast<unsigned long long>(timing.drawable_requests.load()),
                 static_cast<unsigned long long>(timing.drawable_failures.load()),
                 static_cast<unsigned long long>(timing.head.load()));
    if (--remaining <= 0) { std::fclose(file); file = nullptr; }
  }
};
FramePacingRecorder::FramePacingRecorder() : m_impl(std::make_unique<Impl>()) {
  const char* dir = std::getenv("TURBULENCE_PACING_DIR");
  if (!dir || !*dir) return;
  m_impl->directory = dir;
  m_impl->hook = GetVideoEvents().after_present_event.Register(
      [impl = m_impl.get()](const PresentInfo& info) { impl->OnPresent(info); });
}
FramePacingRecorder::~FramePacingRecorder() {
  m_impl->hook.reset();
  if (m_impl->file) std::fclose(m_impl->file);
}
}  // namespace turbulence
