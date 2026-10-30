#include "turbulence_metal.hpp"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>
#include <pthread/qos.h>

namespace turbulence {
void SetMetalHUDVisible(void* render_view, bool enabled) {
  if (!render_view) return;
  // The runtime's macOS render window is an NSView. Touch its layer on the
  // main thread; the options overlay runs on the render thread. Only change
  // the HUD's mode: replacing the whole property dictionary, or re-enabling
  // it through the environment at run time, registers a second, broken HUD.
  NSView* view = (__bridge NSView*)render_view;
  const auto update = ^{
    if (@available(macOS 13.0, *)) {
      CALayer* layer = view.layer;
      if ([layer isKindOfClass:[CAMetalLayer class]]) {
        CAMetalLayer* metal_layer = (CAMetalLayer*)layer;
        NSMutableDictionary* properties = [metal_layer.developerHUDProperties mutableCopy];
        if (!properties) properties = [NSMutableDictionary dictionary];
        properties[@"mode"] = enabled ? @"default" : @"disabled";
        metal_layer.developerHUDProperties = properties;
      }
    }
  };
  if ([NSThread isMainThread]) update();
  else dispatch_async(dispatch_get_main_queue(), update);
}

void KeepGameResponsive() {
  static id<NSObject> activity = nil;
  if (activity) return;
  // Latency-critical: macOS must not nap or coalesce timers for the game
  // when the window is in the background or covered by Spotlight.
  activity = [[NSProcessInfo processInfo]
      beginActivityWithOptions:(NSActivityUserInitiatedAllowingIdleSystemSleep |
                                NSActivityLatencyCritical)
                        reason:@"Turbulence game emulation and frame presentation"];
}

void PrioritizeCurrentThread() {
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}
}  // namespace turbulence
