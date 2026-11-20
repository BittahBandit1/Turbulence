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
  // Full-width solid menu panels fill horizontally without classifying
  // textured results bars, text or the circular menu design as backgrounds.
  assert(IsFullWidthPanel(-1, 1, 4, false));
  assert(!IsFullWidthPanel(-1, 1, 4, true));
  assert(!IsFullWidthPanel(-1, 1, 32, false));
  assert(!IsFullWidthPanel(-.7f, .7f, 4, false));
  for (float width : {1.0f, .75f, .5f, .375f}) {
    assert(std::abs(ExtendSilverLeft(-1, width, false)*width + width/width) < .00001f);
    // The portrait socket, rank and interior silver endpoints stay put.
    for (float x : {-.8f, -.5f, 0.0f, .7f, 1.0f})
      assert(ExtendSilverLeft(x, width, false)==x);
    assert(ExtendSilverLeft(-1, width, true)==-1);
  }
  // Only the menu's background strip geometry gets scanline gutters.
  assert(IsMenuScanlineQuad(-1,0,-.892f,-.358f,4));
  assert(!IsMenuScanlineQuad(-1,0,-.2f,.2f,4));
  assert(!IsMenuScanlineQuad(-.5f,.5f,-.892f,-.358f,4));
  assert(!IsMenuScanlineQuad(-1,0,-.892f,-.358f,8));
  // Horizontal extension preserves Y through a rotated affine UI matrix.
  float matrix[12]={0,-2,0,10,3,0,0,20,0,0,1,0};
  float projection[6]={.1f,-1,.2f,-1,1,0};
  float p[3]={4,5,0};
  const float old_y=projection[2]*(p[0]*matrix[4]+p[1]*matrix[5]+matrix[7])+projection[3];
  assert(SetClipX(p,matrix,projection,2));
  assert(std::abs(projection[0]*(p[0]*matrix[0]+p[1]*matrix[1]+matrix[3])+projection[1]-2)<.00001f);
  assert(std::abs(projection[2]*(p[0]*matrix[4]+p[1]*matrix[5]+matrix[7])+projection[3]-old_y)<.00001f);
  matrix[1]=0;
  assert(!SetClipX(p,matrix,projection,2));
  // Only exposed OUTER bar ends gain flat padding. Internal curved joins
  // and endpoints already beyond the display stay untouched.
  for(float w : {.75f,.375f}) {
    assert(NeedsMenuBarPadding(-1,w,false));
    assert(NeedsMenuBarPadding(1,w,true));
    assert(!NeedsMenuBarPadding(-.5f,w,false));
    assert(!NeedsMenuBarPadding(.5f,w,true));
    assert(!NeedsMenuBarPadding(-1.0f/w-.1f,w,false));
    assert(!NeedsMenuBarPadding(1.0f/w+.1f,w,true));
  }
  assert(!NeedsMenuBarPadding(-1,1,false));
  assert(!NeedsMenuBarPadding(1,1,true));
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
