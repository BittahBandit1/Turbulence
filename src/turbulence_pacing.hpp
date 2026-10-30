// Turbulence frame pacing: defaults, thread priority and a diagnostic recorder
// (ported from ReFare's refare_runtime_services / refare_metal_hud).
#pragma once
#include <filesystem>
#include <memory>

namespace turbulence {
// Diagnostic: with TURBULENCE_PACING_DIR set, creating <dir>/pacing-request
// records the next 600 presented frames to <dir>/pacing-N.csv, including the
// time each frame actually reached the display ("glass" rows).
class FramePacingRecorder {
public:
  FramePacingRecorder();
  ~FramePacingRecorder();
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace turbulence
