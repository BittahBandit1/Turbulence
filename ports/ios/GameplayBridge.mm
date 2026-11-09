#include "GameplayBridge.h"
#include "turbulence_options.hpp"
#include "turbulence_prompts.hpp"
#include "imgui.h"
#include <atomic>
#include <vector>
#include "moderngekko/runtime.hpp"
#include "moderngekko/module.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "VideoCommon/Present.h"
#include "InputCommon/ControllerInterface/Touch/InputOverrider.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include <Foundation/Foundation.h>
#include <AVFoundation/AVFoundation.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

extern "C" const ModernGekkoModuleDesc* staticrecomp_get_module(void);
namespace {
std::mutex mutex;
std::jthread worker;
moderngekko::Runtime* active = nullptr;
int state = 0;
bool stopping = false;
bool paused = false;
std::string message;
std::atomic<bool> toggleMenu{false};
std::atomic<uint32_t> menuMask{0};
struct Pointer { double x, y; bool down; double scroll; };
std::mutex pointerMutex;
std::vector<Pointer> pointers;
turbulence::OptionsInput ReadMenuInput() {
  std::vector<Pointer> events;
  { std::lock_guard lock(pointerMutex); events.swap(pointers); }
  auto& io = ImGui::GetIO();
  for (const auto& event : events) {
    io.AddMousePosEvent(event.x * io.DisplaySize.x, event.y * io.DisplaySize.y);
    io.AddMouseButtonEvent(0, event.down);
    if (event.scroll) io.AddMouseWheelEvent(0, event.scroll);
  }
  const auto mask = menuMask.load();
  turbulence::OptionsInput input{};
  for (int i=0; i<turbulence::Count; ++i) input[i] = (mask & (1u << i)) != 0;
  input[turbulence::Toggle] = toggleMenu.exchange(false) || input[turbulence::Toggle];
  return input;
}
}
void TurbulenceStart(void* view, void* layer, float scale, const char* game, const char* user) {
  {
    std::lock_guard lock(mutex);
    if (state == 1) return;
  }
  if (worker.joinable()) worker.join();
  const std::filesystem::path root(game), profile(user);
  {
    std::lock_guard lock(mutex);
    state = 1; stopping = false; paused = false; toggleMenu = false; menuMask = 0; message = "Starting game…";
  }
  NSError* audioError = nil;
  [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryPlayback error:&audioError];
  [AVAudioSession.sharedInstance setActive:YES error:&audioError];
  id retainedView = [(id)view retain];
  id retainedLayer = [(id)layer retain];
  worker = std::jthread([=] {
    @autoreleasepool {
      try {
        std::filesystem::create_directories(profile / "Config");
        const auto gfx = profile / "Config/GFX.ini";
        if (!std::filesystem::exists(gfx)) {
          NSString* preset = [NSBundle.mainBundle pathForResource:@"GFX" ofType:@"ini"];
          if (preset) std::filesystem::copy_file(preset.UTF8String, gfx);
        }
        std::freopen((profile / "gameplay.log").c_str(), "a", stderr);
        moderngekko::RuntimeConfig config;
        config.game_root = root;
        config.user_directory = profile;
        config.module = moderngekko::ModuleSource::AttachedDescriptor(staticrecomp_get_module());
        config.native_window = view;
        config.native_metal_layer = layer;
        config.native_surface_scale = scale;
        config.graphics.backend = "Metal";
        // GFX.ini supplies the saved scale; its default is native (1x).
        config.audio.backend = "iOS";
        config.show_fps_in_title = false;
        config.input.blocks_input = [] { return turbulence::OptionsMenuBlocksInput(); };
        auto created = moderngekko::Runtime::Create(std::move(config));
        if (!created) throw std::runtime_error(created.error->message);
        {
          auto inputLock = ControllerEmu::EmulatedController::GetStateLock();
          ciface::Touch::RegisterGameCubeInputOverrider(0);
        }
        {
          std::lock_guard lock(mutex);
          active = created.runtime.get();
          if (stopping) active->RequestStop();
          message = "Running";
        }
        // Clear the borrowed pointer before destruction even if Run throws.
        struct ClearActive {
          ~ClearActive() { std::lock_guard lock(mutex); active = nullptr; }
        } clearActive;
        auto options = std::make_unique<turbulence::OptionsMenu>(profile, view,
            [] { TurbulenceStop(); }, ReadMenuInput, [] { return turbulence::MenuButtonCapture{}; });
        auto result = created.runtime->Run();
        options.reset();
        {
          std::lock_guard lock(mutex);
          active = nullptr;
        }
        {
          auto inputLock = ControllerEmu::EmulatedController::GetStateLock();
          ciface::Touch::UnregisterGameCubeInputOverrider(0);
        }
        created.runtime.reset();
        std::lock_guard lock(mutex);
        state = result.error ? 3 : 2;
        message = result.error ? result.error->message : "Game stopped.";
      } catch (const std::exception& error) {
        std::lock_guard lock(mutex);
        std::fprintf(stderr, "[iOS] startup error: %s\n", error.what());
        std::fflush(stderr);
        active = nullptr; state = 3; message = error.what();
      }
    }
    // The SwiftUI surface may disappear before asynchronous shutdown finishes.
    // Retain it through Run(), then release UIKit objects on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      [retainedLayer release];
      [retainedView release];
    });
  });
}
void TurbulenceToggleMenu() { toggleMenu = true; }
int TurbulenceMenuOpen() { return turbulence::OptionsMenuBlocksInput() ? 1 : 0; }
void TurbulenceMenuButtons(uint32_t mask) { menuMask = mask; }
void TurbulenceMenuPointer(double x, double y, int down, double scroll) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(scroll)) return;
  std::lock_guard lock(pointerMutex);
  if (pointers.size() < 256) pointers.push_back({std::clamp(x,0.0,1.0),std::clamp(y,0.0,1.0),down != 0,scroll});
}
void TurbulenceResize() {
  Core::QueueHostJob([](Core::System&) { if (g_presenter) g_presenter->ResizeSurface(); });
}
void TurbulenceStop() { std::lock_guard lock(mutex); stopping = true; if (active) active->RequestStop(); }
void TurbulencePause(int value) {
  std::lock_guard lock(mutex);
  paused = value != 0;
  if (active && Core::IsRunning(Core::System::GetInstance())) {
    const auto current = Core::GetState(Core::System::GetInstance());
    if (paused && current != Core::State::Paused) active->Pause();
    if (!paused && current == Core::State::Paused) active->Resume();
  }
}
void TurbulenceInput(int control, double value) {
  if (control < 0 || control > 17 || !std::isfinite(value)) return;
  auto inputLock = ControllerEmu::EmulatedController::GetStateLock();
  ciface::Touch::SetControlState(0, static_cast<ciface::Touch::ControlID>(control), turbulence::OptionsMenuBlocksInput() ? 0.0 : std::clamp(value, -1.0, 1.0));
}
int TurbulenceStatus(char* output, int capacity) {
  std::lock_guard lock(mutex);
  if (output && capacity > 0) std::snprintf(output, capacity, "%s", message.c_str());
  return state;
}

void TurbulenceControllerStyle(int style) { turbulence::SetAutomaticControllerStyle(style); }
