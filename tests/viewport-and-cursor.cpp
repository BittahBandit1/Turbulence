#include "Common/CursorAutoHide.h"
#include "VideoCommon/TurbulenceViewport.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <array>

int main()
{
  using namespace std::chrono_literals;
  Common::CursorAutoHide cursor;
  const auto t = Common::CursorAutoHide::Clock::time_point{};
  assert(!cursor.Update(t, true, false));
  assert(!cursor.Update(t + 2999ms, true, false));
  assert(cursor.Update(t + 3s, true, false));
  assert(!cursor.Update(t + 4s, true, true));
  assert(!cursor.Update(t + 6999ms, true, false));
  assert(cursor.Update(t + 7s, true, false));
  assert(!cursor.Update(t + 8s, false, false));
  assert(!cursor.Update(t + 20s, true, false));
  assert(cursor.Update(t + 23s, true, false));

  using namespace TurbulenceViewport;
  // Edge-touching result rows and a batch of all eight rows are foreground,
  // while full-screen backgrounds/fades keep filling the viewport.
  assert(!IsFullscreenBackground(-1, 1, 0.2f, 0.3f, 4));
  assert(!IsFullscreenBackground(-1, 1, -1, 1, 32));
  assert(!IsFullscreenBackground(-1, 0.2f, -1, 1, 4));
  assert(IsFullscreenBackground(-1, 1, -1, 1, 4));
  // Both the CPU frustum and final GPU projection must have the same FOV.
  for (double ar : {4.0/3, 16.0/9, 16.0/10, 21.0/9, 32.0/9, 9.0/16})
  {
    Scale scale{static_cast<float>(std::min(1.0, OriginalAspect/ar)),
                static_cast<float>(std::min(1.0, ar/OriginalAspect))};
    double aspect = OriginalAspect;
    std::uint32_t fov = 8192; // 45 degrees
    const bool changed = AdjustCamera(aspect, fov, scale);
    assert(changed == (ar != OriginalAspect));
    const double y = 1.0 / std::tan(fov * Pi / 65536.0);
    const double original_y = 1.0 / std::tan(8192.0 * Pi / 65536.0);
    assert(std::abs(y - original_y*scale.height) < 0.001);
    assert(std::abs(y/aspect - original_y/OriginalAspect*scale.width) < 0.001);
    camera_adjusted.store(changed);
    assert(ProjectionAlreadyAdjusted(y/aspect, y, scale) == changed);

  }
  double invalid_aspect = 2.0;
  std::uint32_t fov = 8192;
  assert(!AdjustCamera(invalid_aspect, fov, Scale{0.375f,1}));
  assert(invalid_aspect == 2.0 && fov == 8192);
  std::cout << "Cursor timeout/focus and viewport FOV scenarios passed\n";
}
