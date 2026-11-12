#include "turbulence_prompts.hpp"
#include "turbulence_prompt_recipes.hpp"
#include "Common/HookableEvent.h"
#include "Core/Host.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#ifdef HAVE_SDL3
#include "InputCommon/ControllerInterface/SDL/SDLGamepad.h"
#endif
#include "VideoCommon/TurbulencePromptTextures.h"
#include "VideoCommon/VideoEvents.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <mutex>

namespace turbulence {
namespace {
using TurbulencePromptTextures::Style;
std::atomic<int> s_preference{static_cast<int>(PromptPreference::Automatic)};
std::atomic<int> s_style{Style::GameCube};
std::atomic<int> s_host_style{-1};

// Sonic Riders (USA) prompt textures, identified by Dolphin's texture name
// without the palette hash (CI8 = GX format 9). Each holds the GameCube,
// Xbox and PlayStation 2 icons side by side; the GameCube cell is the one the
// game draws. Cells measured from the game's own textures.
void RegisterRecipes() {
  for (auto recipe : BuildPromptRecipes())
    TurbulencePromptTextures::Register(std::move(recipe));
}

Style StyleForDevice(const std::string& name) {
  const auto has = [&](const char* s) { return name.find(s) != std::string::npos; };
  if (has("PS5") || has("PS4") || has("PS3") || has("DualSense") || has("DualShock") ||
      has("PlayStation") || has("Wireless Controller"))
    return Style::PlayStation;
  if (has("Xbox") || has("XBOX") || has("XInput") || has("X-Box"))
    return Style::Xbox;
  return Style::GameCube;
}

Style StyleForDevice(const ciface::Core::Device& device) {
  if (device.GetSource() == "XInput") return Style::Xbox;
#ifdef HAVE_SDL3
  // Every SDL-source device is an SDL::Gamepad. No dynamic_cast: Dolphin
  // builds with /GR- on Windows, where it throws "no RTTI data".
  if (device.GetSource() == "SDL") {
    const auto* pad = static_cast<const ciface::SDL::Gamepad*>(&device);
    switch (SDL_GetGamepadTypeForID(pad->GetSDLInstanceID())) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5: return Style::PlayStation;
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE: return Style::Xbox;
    default: break;
    }
  }
#endif
  return StyleForDevice(device.GetName());
}

void Apply(int style) {
  // Keep backend and frontend synchronized across a runtime restart too.
  TurbulencePromptTextures::SetStyle(static_cast<Style>(style));
  if (s_style.exchange(style) != style) {
    std::fprintf(stderr, "[turbulence] button prompts: %s\n",
                 style == Style::Xbox ? "Xbox" : style == Style::PlayStation ? "PlayStation"
                                                                              : "GameCube");
  }
}
}  // namespace

void SetPromptPreference(PromptPreference preference) {
  s_preference.store(static_cast<int>(preference));
  switch (preference) {
  case PromptPreference::GameCube: Apply(Style::GameCube); break;
  case PromptPreference::Xbox: Apply(Style::Xbox); break;
  case PromptPreference::PlayStation: Apply(Style::PlayStation); break;
  case PromptPreference::Automatic: break;  // The next poll picks the device.
  }
}
PromptPreference GetPromptPreference() {
  return static_cast<PromptPreference>(s_preference.load());
}
int CurrentPromptStyle() { return s_style.load(); }
void SetAutomaticControllerStyle(int style) {
  s_host_style.store(style >= -1 && style <= 2 ? style : -1);
}

struct ButtonPrompts::Impl {
  Common::EventHook hook;
  unsigned tick = 0;
  bool chosen = false;
  // Video thread, once per present; every other frame is plenty.
  void Poll() {
    if (s_preference.load() != static_cast<int>(PromptPreference::Automatic)) {
      chosen = false;
      return;
    }
    if (const int style = s_host_style.load(); style >= 0) {
      Apply(style); chosen = true; return;
    }
    if ((tick++ % 2u) != 0u || !g_controller_interface.IsInit()) return;
    const bool focused = Host_RendererHasFocus();
    int first_pad = -1, used = -1;
    // On Windows the same pad also shows up through DInput (with a generic
    // name -> GameCube prompts); with SDL present, ignore DInput so the
    // prompts don't flip on every press.
    bool have_sdl = false;
    for (const auto& device : g_controller_interface.GetAllDevices())
      have_sdl = have_sdl || device->GetSource() == "SDL";
    for (const auto& device : g_controller_interface.GetAllDevices()) {
      const auto source = device->GetSource();
      if (source != "SDL" && source != "XInput" && source != "DInput") continue;
      if (have_sdl && source == "DInput") continue;
      const int style = StyleForDevice(*device);
      if (first_pad < 0) first_pad = style;
      if (!focused || used >= 0) continue;
      for (const auto* input : device->Inputs()) {
        if (input->IsDetectable() && input->GetState() > 0.5) { used = style; break; }
      }
    }
    if (used >= 0) Apply(used);
    else if (!chosen && first_pad >= 0) Apply(first_pad);
    else if (!chosen) Apply(Style::GameCube);
    chosen = true;
  }
};

ButtonPrompts::ButtonPrompts() : m_impl(std::make_unique<Impl>()) {
  static std::once_flag recipes;
  std::call_once(recipes, RegisterRecipes);
  // Diagnostic / unattended tests: TURBULENCE_PROMPTS=xbox|playstation|gamecube.
  if (const char* force = std::getenv("TURBULENCE_PROMPTS")) {
    const std::string f = force;
    if (f == "xbox") SetPromptPreference(PromptPreference::Xbox);
    else if (f == "playstation") SetPromptPreference(PromptPreference::PlayStation);
    else if (f == "gamecube") SetPromptPreference(PromptPreference::GameCube);
  }
  m_impl->hook = GetVideoEvents().before_present_event.Register(
      [impl = m_impl.get()](PresentInfo&) { impl->Poll(); });
}
ButtonPrompts::~ButtonPrompts() {
  m_impl->hook.reset();
  s_host_style = -1;
  TurbulencePromptTextures::SetStyle(Style::GameCube);
}
}  // namespace turbulence
