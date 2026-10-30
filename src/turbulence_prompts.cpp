#include "turbulence_prompts.hpp"
#include "Common/HookableEvent.h"
#include "Core/Host.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/TurbulencePromptTextures.h"
#include "VideoCommon/VideoEvents.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace turbulence {
namespace {
using TurbulencePromptTextures::Style;
std::atomic<int> s_preference{static_cast<int>(PromptPreference::Automatic)};
std::atomic<int> s_style{Style::GameCube};

// Sonic Riders (USA) prompt textures, identified by Dolphin's texture name
// without the palette hash (CI8 = GX format 9). Each holds the GameCube,
// Xbox and PlayStation 2 icons side by side; the GameCube cell is the one the
// game draws. Cells measured from the game's own textures.
void RegisterRecipes() {
  using TurbulencePromptTextures::CellCopy;
  using TurbulencePromptTextures::Recipe;
  // 256x128 HUD/menu sheet: A row (GC teal A, Xbox green A, PS cross, PS
  // triangle) at y 91, Y row (GC grey Y, Xbox gold Y, PS circle) at y 109.
  TurbulencePromptTextures::Register(Recipe{
      "tex1_256x128_af289750027e9b53", 9,
      {CellCopy{1, 91, 19, 18, {1, 20, 39}, {91, 91, 91}},      // A -> A / cross
       CellCopy{1, 109, 19, 18, {1, 20, 58}, {109, 109, 91}}}}); // Y -> Y / triangle
  // 512x128 "Select / Back" bar: A row at y 0, B row at y 22, 24 px cells.
  TurbulencePromptTextures::Register(Recipe{
      "tex1_512x128_585837ce16d8abc6", 9,
      {CellCopy{0, 0, 24, 22, {0, 24, 48}, {0, 0, 0}},          // A -> A / cross
       CellCopy{0, 22, 24, 22, {0, 24, 48}, {22, 22, 22}}}});   // B -> B / circle
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

void Apply(int style) {
  if (s_style.exchange(style) != style) {
    TurbulencePromptTextures::SetStyle(static_cast<Style>(style));
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
    if ((tick++ % 2u) != 0u || !g_controller_interface.IsInit()) return;
    const bool focused = Host_RendererHasFocus();
    int first_pad = -1, used = -1;
    for (const auto& device : g_controller_interface.GetAllDevices()) {
      if (device->GetSource() != "SDL") continue;
      const int style = StyleForDevice(device->GetName());
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
  RegisterRecipes();
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
  TurbulencePromptTextures::SetStyle(Style::GameCube);
}
}  // namespace turbulence
