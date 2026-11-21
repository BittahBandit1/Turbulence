#include "turbulence_metal.hpp"
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <pthread/qos.h>
#include <atomic>
#include <cstdlib>
namespace turbulence {
static std::atomic<double> refresh{60};

// Apple's HUD owns the live ProcessInfo thermal-state reporting. Keep the
// existing metric selection and appearance, adding its built-in thermal row.
// https://developer.apple.com/documentation/xcode/customizing-metal-performance-hud
static NSMutableDictionary* MetalHUDProperties(CAMetalLayer* layer) API_AVAILABLE(ios(16.0)) {
  NSMutableDictionary* values = [layer.developerHUDProperties mutableCopy];
  if (!values) values = [NSMutableDictionary dictionary];
  NSString* elements = values[@"MTL_HUD_ELEMENTS"];
  if (![elements isKindOfClass:NSString.class] || elements.length == 0) {
    const char* configured = std::getenv("MTL_HUD_ELEMENTS");
    elements = configured && configured[0] ? [NSString stringWithUTF8String:configured] :
        @"device,layersize,layerscale,memory,fps,frameinterval,gputime,frameintervalgraph";
  }
  if (elements.length == 0)
    elements = @"device,layersize,layerscale,memory,fps,frameinterval,gputime,frameintervalgraph";
  NSMutableArray<NSString*>* metrics = [NSMutableArray array];
  for (NSString* element in [elements componentsSeparatedByString:@","]) {
    NSString* metric = [element stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (metric.length && ![metrics containsObject:metric]) [metrics addObject:metric];
  }
  if (![metrics containsObject:@"thermal"]) [metrics addObject:@"thermal"];
  values[@"MTL_HUD_ELEMENTS"] = [metrics componentsJoinedByString:@","];
  // Nominal is thermal state zero; keep it visible while the device is cool.
  values[@"MTL_HUD_SHOW_ZERO_METRICS"] = @"1";
  return values;
}

void SetMetalHUDVisible(void* surface, bool enabled) {
  if (!surface) return;
  UIView* view = (__bridge UIView*)surface;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (@available(iOS 16.0, *)) {
      if ([view.layer isKindOfClass:CAMetalLayer.class]) {
        CAMetalLayer* layer = (CAMetalLayer*)view.layer;
        NSMutableDictionary* values = MetalHUDProperties(layer);
        values[@"mode"] = enabled ? @"default" : @"disabled";
        layer.developerHUDProperties = values;
      }
    }
  });
}
void KeepGameResponsive() {}
void PrioritizeCurrentThread() { pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0); }
double SecondsSinceKeyDown() { return -1; }
double DisplayRefreshRate(void*) { return refresh.load(); }
}
extern "C" void TurbulenceDisplayRate(double rate) { turbulence::refresh.store(rate); }

// Called on the UI thread before the first Metal device is created.
extern "C" void TurbulencePrepareMetalHUD(void* surface_layer) {
  setenv("MTL_HUD_ENABLED", "1", 0);
  if (@available(iOS 16.0, *)) {
    CAMetalLayer* layer = (__bridge CAMetalLayer*)surface_layer;
    if (layer) {
      NSMutableDictionary* values = turbulence::MetalHUDProperties(layer);
      if (!values[@"mode"]) values[@"mode"] = @"default";
      layer.developerHUDProperties = values;
    }
  }
}
