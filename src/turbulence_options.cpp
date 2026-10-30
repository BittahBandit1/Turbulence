// Turbulence options menu, ported from ReFare's (src/refare_options.cpp) with
// a Sonic Riders palette. Drawn through Dolphin's ImGui overlay on the render
// thread; reads controllers through Dolphin's controller interface, so it
// respects window focus like the game does.
#include "turbulence_options.hpp"
#include "Common/Config/Config.h"
#include "Common/IniFile.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Host.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <string>
#include <vector>

// Defined in Dolphin's VideoCommon (HUD layout) and Metal backend (HUD).
extern bool g_turbulence_hud_stretched;
#ifdef __APPLE__
extern bool g_turbulence_metal_hud_enabled;
#endif

namespace turbulence {
namespace {
std::atomic<bool> s_blocks_input{false};

// Sonic Riders palette, sampled from the title screen: the deep indigo of the
// Sonic silhouette for panels, the sky blue behind the logo for frames and
// highlights, the logo's gold for focus, and the orange-red of its "R".
struct Palette {
  ImVec4 window{0.090f, 0.086f, 0.290f, 0.96f};       // indigo #17164A
  ImVec4 border{0.180f, 0.560f, 0.960f, 0.90f};       // sky blue #2E8FF5
  ImVec4 title{1.000f, 0.820f, 0.080f, 1.00f};        // logo gold #FFD114
  ImVec4 accent{0.360f, 0.720f, 1.000f, 1.00f};       // light sky #5CB8FF
  ImVec4 text{0.960f, 0.970f, 1.000f, 1.00f};
  ImVec4 text_dim{0.620f, 0.680f, 0.880f, 1.00f};
  ImVec4 button{0.160f, 0.170f, 0.470f, 1.00f};
  ImVec4 button_hover{0.150f, 0.440f, 0.860f, 1.00f};
  ImVec4 button_active{0.180f, 0.560f, 0.960f, 1.00f};
  ImVec4 row_selected{0.180f, 0.560f, 0.960f, 0.45f};
  ImVec4 row_hover{0.180f, 0.560f, 0.960f, 0.25f};
  ImVec4 focus{1.000f, 0.820f, 0.080f, 1.00f};        // logo gold
  ImVec4 focus_text{0.090f, 0.086f, 0.290f, 1.00f};   // indigo on gold
  ImVec4 danger{0.910f, 0.290f, 0.100f, 1.00f};       // the "R" #E84A1A
  ImVec4 separator{0.180f, 0.560f, 0.960f, 0.40f};
};
const Palette kPalette;

// Pushes the palette for one window; pop with PopTheme().
void PushTheme() {
  const Palette& p = kPalette;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 22));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, p.window);
  ImGui::PushStyleColor(ImGuiCol_Border, p.border);
  ImGui::PushStyleColor(ImGuiCol_Text, p.text);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, p.text_dim);
  ImGui::PushStyleColor(ImGuiCol_Button, p.button);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.button_hover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.button_active);
  ImGui::PushStyleColor(ImGuiCol_Header, p.row_selected);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, p.row_hover);
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, p.row_selected);
  ImGui::PushStyleColor(ImGuiCol_Separator, p.separator);
}
void PopTheme() { ImGui::PopStyleColor(11); ImGui::PopStyleVar(4); }

struct Scale { int value; const char* label; const char* resolution; };
constexpr std::array<Scale, 7> scales = {{{1,"1x (native)","640x528"},
 {2,"2x","1280x720"},{3,"3x","1920x1080"},{4,"4x","2560x1440"},
 {6,"6x","3840x2160"},{8,"8x","5120x2880"},{12,"12x","7680x4320"}}};
struct Aspect { const char* label; AspectMode mode; int width; int height; };
constexpr std::array<Aspect, 6> aspects = {{{"Match window / display",AspectMode::Stretch,4,3},
 {"4:3 (original)",AspectMode::ForceStandard,4,3},
 {"16:9",AspectMode::CustomStretch,16,9},{"16:10",AspectMode::CustomStretch,16,10},
 {"21:9",AspectMode::CustomStretch,21,9},{"32:9",AspectMode::CustomStretch,32,9}}};
// Dolphin post-processing shader shipped in Sys/Shaders/FXAA.glsl.
constexpr const char* kFXAAShader = "FXAA";

enum class Row { Scale, Aspect, HudLayout, FXAA, VSync, MetalHUD, MenuButton, Exit };
const std::vector<Row>& Rows() {
  static const std::vector<Row> rows = {Row::Scale, Row::Aspect, Row::HudLayout, Row::FXAA,
    Row::VSync,
#ifdef __APPLE__
    Row::MetalHUD,
#endif
    Row::MenuButton, Row::Exit};
  return rows;
}
const char* RowLabel(Row row) {
  switch (row) {
  case Row::Scale: return "Resolution scale";
  case Row::Aspect: return "Aspect ratio";
  case Row::HudLayout: return "HUD and menus";
  case Row::FXAA: return "FXAA";
  case Row::VSync: return "V-Sync";
  case Row::MetalHUD: return "Metal HUD";
  case Row::MenuButton: return "Options menu button";
  case Row::Exit: return "Exit The Game";
  }
  return "";
}

// Controller button and keyboard key that open/close this menu, by Dolphin
// input name. Saved in Config/Turbulence.ini; render thread only.
MenuButtonBinding s_menu_binding;

// Display name for an SDL controller input (PlayStation labels, like the rest
// of the menu); anything unknown is shown as Dolphin names it.
std::string PadLabel(const std::string& name) {
  static constexpr std::pair<const char*, const char*> labels[] = {
    {"Shoulder L","L1"},{"Shoulder R","R1"},{"Trigger L","L2"},{"Trigger R","R2"},
    {"Thumb L","L3"},{"Thumb R","R3"},{"Button W","Square"},{"Button N","Triangle"},
    {"Button S","Cross"},{"Button E","Circle"},{"Back","Select / Share"},
    {"Start","Start / Options"},{"Guide","Home / PS"},{"Touchpad","Touchpad"}};
  for (const auto& [from, to] : labels) if (name == from) return to;
  return name;
}

// Inputs that can't become the menu button: the menu's own navigation and
// cancel, sticks, motion sensors and the mouse.
bool BindablePad(const std::string& n) {
  for (const char* p : {"Pad ","Left ","Right ","Accel","Gyro","Motion","Battery"})
    if (n.starts_with(p)) return false;
  return n != "Button S" && n != "Button E";
}
bool BindableKey(const std::string& n) {
  for (const char* p : {"Click","Cursor","Axis","Scroll","Up","Down","Left","Right"})
    if (n.starts_with(p)) return false;
  return n != "Escape" && n != "Return" && n != "Keypad Enter";
}

MenuButtonCapture ReadCapture() {
  MenuButtonCapture result;
  if (!Host_RendererHasFocus() || !g_controller_interface.IsInit()) return result;
  for (const auto& device : g_controller_interface.GetAllDevices()) {
    const bool pad = device->GetSource() == "SDL";
    if (!pad && device->GetName().find("Keyboard") == std::string::npos) continue;
    for (const auto* input : device->Inputs()) {
      if (!input->IsDetectable() || input->GetState() <= 0.5) continue;
      const std::string name = input->GetName();
      if (pad ? name == "Button E" : name == "Escape") { result.cancel = true; continue; }
      if (pad ? !BindablePad(name) : !BindableKey(name)) {
        // Navigation still down from choosing the row (e.g. Cross, Enter) must be
        // released first; sticks, motion sensors and the mouse never count.
        if (pad ? (name == "Button S" || name.starts_with("Pad "))
                : !(name.starts_with("Click") || name.starts_with("Cursor") ||
                    name.starts_with("Axis") || name.starts_with("Scroll")))
          result.held = true;
        continue;
      }
      result.held = true;
      if (pad && result.pad.empty()) result.pad = name;
      if (!pad && result.key.empty()) result.key = name;
    }
  }
  return result;
}

using Buttons = OptionsInput;
Buttons ReadButtons() {
  Buttons result{};
  if (!Host_RendererHasFocus() || !g_controller_interface.IsInit()) return result;
  // Normal SI polling updates devices. Do not poll them again on the render
  // thread: that contends with game input and adds jitter to presentation.
  for (const auto& device : g_controller_interface.GetAllDevices()) {
    const auto held = [&](std::string_view name) {
      auto* input = device->FindInput(name);
      return input && input->GetState() > 0.5;
    };
    if (device->GetSource() == "SDL") {
      result[Toggle] |= !s_menu_binding.pad.empty() && held(s_menu_binding.pad);
      result[Up] |= held("Pad N"); result[Down] |= held("Pad S");
      result[Left] |= held("Pad W"); result[Right] |= held("Pad E");
      result[Back] |= held("Button E");
      result[Confirm] |= held("Button S");
    } else {
      result[Toggle] |= !s_menu_binding.key.empty() && held(s_menu_binding.key);
      result[Up] |= held("Up Arrow") || held("Up");
      result[Down] |= held("Down Arrow") || held("Down");
      result[Left] |= held("Left Arrow") || held("Left");
      result[Right] |= held("Right Arrow") || held("Right");
      result[Back] |= held("Escape");
      result[Confirm] |= held("Return") || held("Keypad Enter");
    }
  }
  return result;
}

std::string TurbulenceIni(const std::filesystem::path& directory) {
  return (directory / "Config" / "Turbulence.ini").string();
}
bool SaveTurbulenceIni(const std::filesystem::path& directory,
                       const std::function<void(Common::IniFile&)>& write) {
  Common::IniFile ini;
  const auto file = TurbulenceIni(directory);
  ini.Load(file);
  write(ini);
  std::error_code ec;
  std::filesystem::create_directories(directory / "Config", ec);
  return !ec && ini.Save(file);
}
}  // namespace

struct OptionsMenu::Impl {
  std::filesystem::path directory;
  Common::EventHook draw_hook;
  std::function<OptionsInput()> input_source;
  std::function<void()> request_exit;
  std::function<MenuButtonCapture()> capture_source;
  Buttons previous{};
  bool capturing = false;
  bool capture_armed = false;  // nothing held since capture started
  mutable std::string binding_text;
  bool open = false;
  bool drain = false;
  int selected = 0;
  int scale_index = 0;
  int aspect_index = 0;
  bool stretch_hud = false;
  bool fxaa = false;
  bool vsync = true;
  bool metal_hud = true;
  bool confirm_exit = false;
  int exit_choice = 0;  // 0 = No (left, default), 1 = Yes (right)
  bool exiting = false;
  std::string save_error;
  MenuButtonBinding binding;

  Impl(std::filesystem::path path, std::function<void()> exit,
       std::function<OptionsInput()> source, std::function<MenuButtonCapture()> capture)
      : directory(std::move(path)), input_source(source ? std::move(source) : ReadButtons),
        request_exit(std::move(exit)),
        capture_source(capture ? std::move(capture) : ReadCapture) {
    s_menu_binding = LoadMenuButtonBinding(directory);
    binding = s_menu_binding;
    draw_hook = GetVideoEvents().imgui_draw_event.Register([this] { Draw(); });
    // Unattended tests (scripts/turbulence_agent.sh) can't press the real menu
    // button: automation input drives the emulated GameCube pad.
    if (const char* open_now = std::getenv("TURBULENCE_OPTIONS_OPEN"); open_now && *open_now == '1')
      open_on_first_draw = true;
  }
  bool open_on_first_draw = false;
  ~Impl() { draw_hook.reset(); s_blocks_input = false; }
  Row CurrentRow() const { return Rows()[selected]; }
  void ReadSettings() {
    stretch_hud = g_turbulence_hud_stretched;
#ifdef __APPLE__
    metal_hud = g_turbulence_metal_hud_enabled;
#endif
    fxaa = Config::Get(Config::GFX_ENHANCE_POST_SHADER) == kFXAAShader;
    vsync = Config::Get(Config::GFX_VSYNC);
    const int scale = Config::Get(Config::GFX_EFB_SCALE);
    scale_index = 0;
    for (int i=0; i<static_cast<int>(scales.size()); ++i)
      if (scales[i].value == scale) scale_index = i;
    const auto mode = Config::Get(Config::GFX_ASPECT_RATIO);
    aspect_index = 0;
    for (int i=0; i<static_cast<int>(aspects.size()); ++i) {
      if (aspects[i].mode != mode) continue;
      if (mode == AspectMode::CustomStretch &&
          (Config::Get(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH) != aspects[i].width ||
           Config::Get(Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT) != aspects[i].height)) continue;
      aspect_index = i;
    }
  }
  // Save only the given GFX.ini keys, retaining all unrelated user settings.
  template <typename Fn> void SaveGraphics(Fn&& write) {
    Common::IniFile graphics;
    const auto file = (directory / "Config" / "GFX.ini").string();
    graphics.Load(file);
    write(graphics);
    std::error_code ec;
    std::filesystem::create_directories(directory / "Config", ec);
    save_error = (!ec && graphics.Save(file)) ? "" : "Could not save video settings; change applies this session.";
  }
  void ApplyScale() {
    Config::SetBaseOrCurrent(Config::GFX_EFB_SCALE, scales[scale_index].value);
    // The frontend applies this setting at boot; keep it in sync with GFX.ini.
    Common::IniFile frontend;
    const auto file = (directory / "config.ini").string();
    if (!frontend.Load(file)) { save_error = "Could not read settings; change applies this session."; return; }
    frontend.GetOrCreateSection("Video")->Set("resolution", std::string(scales[scale_index].resolution));
    if (!frontend.Save(file)) { save_error = "Could not save resolution; change applies this session."; return; }
    SaveGraphics([&](Common::IniFile& ini) {
      ini.GetOrCreateSection("Settings")->Set("InternalResolution", scales[scale_index].value);
    });
  }
  void ApplyAspect() {
    const auto& aspect = aspects[aspect_index];
    Config::SetBaseOrCurrent(Config::GFX_ASPECT_RATIO, aspect.mode);
    Config::SetBaseOrCurrent(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH, aspect.width);
    Config::SetBaseOrCurrent(Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT, aspect.height);
    Config::SetBaseOrCurrent(Config::GFX_WIDESCREEN_HACK, aspect_index != 1);
    Config::SetBaseOrCurrent(Config::GFX_SUGGESTED_ASPECT_RATIO, AspectMode::ForceStandard);
    Config::SetBaseOrCurrent(Config::GFX_CROP_TO_ASPECT_RATIO, false);
    SaveGraphics([&](Common::IniFile& ini) {
      auto* section = ini.GetOrCreateSection("Settings");
      section->Set("AspectRatio", static_cast<int>(aspect.mode));
      section->Set("CustomAspectRatioWidth", aspect.width);
      section->Set("CustomAspectRatioHeight", aspect.height);
      section->Set("wideScreenHack", aspect_index != 1);
      section->Set("SuggestedAspectRatio", 2);
      section->Set("Crop", false);
    });
  }
  void ApplyHudLayout() {
    // VertexShaderManager reloads orthographic projections every draw.
    g_turbulence_hud_stretched = stretch_hud;
    save_error = SaveTurbulenceIni(directory, [&](Common::IniFile& ini) {
      ini.GetOrCreateSection("Video")->Set("HudStretched", stretch_hud);
    }) ? "" : "Could not save the HUD layout; change applies this session.";
  }
  void ApplyFXAA() {
    // The presenter recompiles its post-processing pipeline on the next frame.
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, std::string(fxaa ? kFXAAShader : ""));
    SaveGraphics([&](Common::IniFile& ini) {
      ini.GetOrCreateSection("Enhancements")->Set("PostProcessingShader", std::string(fxaa ? kFXAAShader : ""));
    });
  }
  void ApplyVSync() {
    // Off lets presents run unthrottled (tearing possible); applies live.
    Config::SetBaseOrCurrent(Config::GFX_VSYNC, vsync);
    SaveGraphics([&](Common::IniFile& ini) {
      ini.GetOrCreateSection("Hardware")->Set("VSync", vsync);
    });
  }
  void ApplyMetalHUD() {
#ifdef __APPLE__
    g_turbulence_metal_hud_enabled = metal_hud;  // The Metal backend syncs it at present.
#endif
    save_error = SaveTurbulenceIni(directory, [&](Common::IniFile& ini) {
      ini.GetOrCreateSection("Video")->Set("MetalHUD", metal_hud);
    }) ? "" : "Could not save the HUD setting; change applies this session.";
  }
  void SaveMenuButton() {
    s_menu_binding = binding;
    save_error = SaveMenuButtonBinding(directory, binding) ? "" : "Could not save the menu button; change applies this session.";
  }
  void StartCapture() { capturing = true; capture_armed = false; }
  // Waits for every input to be released (the press that chose the row),
  // then binds the next controller button or key. Circle / Escape cancel.
  void UpdateCapture() {
    const MenuButtonCapture c = capture_source();
    if (c.cancel) { capturing = false; previous.fill(true); return; }
    if (!capture_armed) { capture_armed = !c.held; return; }
    if (c.pad.empty() && c.key.empty()) return;
    // A controller press changes only the controller button, a key only the key.
    if (!c.pad.empty()) binding.pad = c.pad;
    if (!c.key.empty()) binding.key = c.key;
    SaveMenuButton();
    capturing = false;
    previous.fill(true);  // the new button is held now: don't let it close the menu
  }
  void Step(int delta) {
    switch (CurrentRow()) {
    case Row::Scale:
      scale_index = std::clamp(scale_index + delta, 0, static_cast<int>(scales.size())-1);
      ApplyScale(); break;
    case Row::Aspect: {
      const int count = static_cast<int>(aspects.size());
      aspect_index = (aspect_index + delta + count) % count;
      ApplyAspect(); break;
    }
    case Row::HudLayout: stretch_hud = !stretch_hud; ApplyHudLayout(); break;
    case Row::FXAA: fxaa = !fxaa; ApplyFXAA(); break;
    case Row::VSync: vsync = !vsync; ApplyVSync(); break;
    case Row::MetalHUD: metal_hud = !metal_hud; ApplyMetalHUD(); break;
    case Row::MenuButton:  // Left/Right restore the default R1 / Tab.
      if (binding.pad != MenuButtonBinding{}.pad || binding.key != MenuButtonBinding{}.key) {
        binding = MenuButtonBinding{}; SaveMenuButton();
      }
      break;
    case Row::Exit: break;  // Needs an explicit confirm, never a D-pad nudge.
    }
  }
  void Activate() {
    switch (CurrentRow()) {
    case Row::Exit: confirm_exit = true; exit_choice = 0; break;
    case Row::MenuButton: StartCapture(); break;
    case Row::HudLayout: case Row::FXAA: case Row::VSync: case Row::MetalHUD: Step(1); break;
    default: break;
    }
  }
  void Close() { open = false; confirm_exit = false; capturing = false; drain = true; }
  void FinishExit() {
    exiting = true;
    Close();
    if (request_exit) request_exit();
  }
  const char* RowValue(Row row) const {
    switch (row) {
    case Row::Scale: return scales[scale_index].label;
    case Row::Aspect: return aspects[aspect_index].label;
    case Row::HudLayout: return stretch_hud ? "Stretched" : "Centered (4:3)";
    case Row::FXAA: return fxaa ? "On" : "Off";
    case Row::VSync: return vsync ? "On" : "Off";
    case Row::MetalHUD: return metal_hud ? "On" : "Off";
    case Row::MenuButton:
      if (capturing) return "Press a button or key...";
      binding_text = PadLabel(binding.pad) + " / " + binding.key;
      return binding_text.c_str();
    case Row::Exit: return "";
    }
    return "";
  }
  void HandleInput(const Buttons& pressed) {
    if (confirm_exit) {
      if (pressed[Left]) exit_choice = 0;
      if (pressed[Right]) exit_choice = 1;
      if (pressed[Back]) confirm_exit = false;
      else if (pressed[Confirm]) {
        if (exit_choice == 1) FinishExit();
        else confirm_exit = false;
      }
      return;
    }
    const int count = static_cast<int>(Rows().size());
    if (pressed[Up]) selected = (selected + count - 1) % count;
    if (pressed[Down]) selected = (selected + 1) % count;
    if (pressed[Left]) Step(-1);
    if (pressed[Right]) Step(1);
    if (pressed[Confirm]) Activate();
  }
  void DrawConfirmExit() {
    const Palette& p = kPalette;
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f,io.DisplaySize.y*0.5f),ImGuiCond_Always,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(360.0f, io.DisplaySize.x - 24.0f),0));
    ImGui::SetNextWindowFocus();
    PushTheme();
    ImGui::Begin("Turbulence exit",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
    const char* question = "Are you sure?";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(question).x)*0.5f);
    ImGui::TextColored(p.title, "%s", question);
    ImGui::Spacing(); ImGui::Spacing();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float button = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    for (int choice = 0; choice < 2; ++choice) {
      if (choice) ImGui::SameLine();
      const bool highlighted = exit_choice == choice;
      ImGui::PushStyleColor(ImGuiCol_Button, highlighted ? p.focus : p.button);
      ImGui::PushStyleColor(ImGuiCol_Text, highlighted ? p.focus_text : p.text);
      if (ImGui::Button(choice ? "Yes" : "No", ImVec2(button, 38))) {
        exit_choice = choice;
        if (choice) FinishExit(); else confirm_exit = false;
      }
      ImGui::PopStyleColor(2);
    }
    ImGui::Spacing();
    ImGui::TextDisabled("D-pad: choose     Cross / Enter: confirm");
    ImGui::End();
    PopTheme();
  }
  void Draw() {
    if (!ImGui::GetCurrentContext()) return;
    const Buttons down = input_source();
    Buttons pressed{};
    for (int i=0;i<Count;++i) pressed[i] = down[i] && !previous[i];
    previous = down;
    if (exiting) { s_blocks_input = true; return; }
    if (open_on_first_draw) { open_on_first_draw = false; open = true; ReadSettings(); }
    if (capturing) UpdateCapture();
    else if (pressed[Toggle]) {
      if (open) Close();
      else { open = true; selected = 0; confirm_exit = false; ReadSettings(); }
    } else if (open && pressed[Back] && !confirm_exit) Close();
    else if (open) HandleInput(pressed);
    if (drain && std::none_of(down.begin(), down.end(), [](bool b){return b;})) drain = false;
    s_blocks_input = open || drain;
    if (!open) return;
    const Palette& p = kPalette;
    const auto& io = ImGui::GetIO();
    const float width = std::min(520.0f, io.DisplaySize.x - 24.0f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f,io.DisplaySize.y*0.5f),ImGuiCond_Always,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(width,0));
    PushTheme();
    ImGui::Begin("Turbulence options",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
    ImGui::TextColored(p.title,"TURBULENCE");
    ImGui::SameLine();
    ImGui::TextColored(p.accent,"/  OPTIONS");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    const auto& rows = Rows();
    for (int index=0; index<static_cast<int>(rows.size()); ++index) {
      const Row row = rows[index];
      const bool is_selected = selected == index;
      ImGui::PushID(index);
      ImGui::PushStyleColor(ImGuiCol_Text, is_selected ? p.focus : p.text);
      const bool clicked = row != Row::Exit && ImGui::Selectable(RowLabel(row), is_selected);
      ImGui::PopStyleColor();
      if (clicked) selected = index;
      if (row == Row::MenuButton) {
        ImGui::TextUnformatted(RowValue(row));
        ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-170);
        if (ImGui::Button(capturing ? "Cancel" : "Change",ImVec2(84,32))) {
          selected=index; if (capturing) capturing=false; else StartCapture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset",ImVec2(78,32))) {selected=index; capturing=false; Step(1);}
      } else if (row == Row::Exit) {
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, is_selected ? p.danger : p.button);
        if (ImGui::Button(RowLabel(row), ImVec2(-1,36))) { selected = index; Activate(); }
        ImGui::PopStyleColor();
      } else {
        if (ImGui::Button("<",ImVec2(42,32))) {selected=index; Step(-1);}
        ImGui::SameLine();
        ImGui::TextColored(is_selected ? p.accent : p.text, "%s", RowValue(row));
        ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-42);
        if (ImGui::Button(">",ImVec2(42,32))) {selected=index; Step(1);}
      }
      ImGui::Spacing(); ImGui::PopID();
    }
    ImGui::Separator(); ImGui::Spacing();
    ImGui::TextWrapped("Changes apply immediately and are saved.");
    if (capturing) {
      ImGui::TextWrapped("Press the controller button or key that should open this menu. Circle / Escape: cancel.");
    } else {
      const std::string pad = "D-pad: select / change     Cross: select     " + PadLabel(s_menu_binding.pad) + " or Circle: close";
      const std::string keys = "Keyboard: " + s_menu_binding.key + ", arrow keys, Enter, Escape";
      ImGui::TextDisabled("%s", pad.c_str());
      ImGui::TextDisabled("%s", keys.c_str());
    }
    if (!save_error.empty()) ImGui::TextColored(p.danger, "%s", save_error.c_str());
    if (ImGui::Button("Done",ImVec2(-1,34))) Close();
    ImGui::End();
    PopTheme();
    if (confirm_exit) DrawConfirmExit();
    s_blocks_input = open || drain || exiting;
  }
};

OptionsMenu::OptionsMenu(std::filesystem::path directory, std::function<void()> request_exit,
                         std::function<OptionsInput()> source,
                         std::function<MenuButtonCapture()> capture_source)
  : m_impl(std::make_unique<Impl>(std::move(directory), std::move(request_exit),
                                  std::move(source), std::move(capture_source))) {}
OptionsMenu::~OptionsMenu() = default;
bool OptionsMenuBlocksInput() { return s_blocks_input.load(); }

void PrepareOptions(const std::filesystem::path& directory) {
  Common::IniFile ini;
  ini.Load(TurbulenceIni(directory));
  auto* video = ini.GetOrCreateSection("Video");
  bool stretched = false;
  video->Get("HudStretched", &stretched, false);
  g_turbulence_hud_stretched = stretched;
#ifdef __APPLE__
  bool metal_hud = true;
  video->Get("MetalHUD", &metal_hud, true);
  g_turbulence_metal_hud_enabled = metal_hud;
  // Enable the HUD machinery before Metal device creation, even for a saved
  // Off preference, so the layer can be toggled back on without restarting.
  setenv("MTL_HUD_ENABLED", "1", 0);
#endif
}

// Config/Turbulence.ini, which only Turbulence writes ([Controls] MenuPad, MenuKey).
MenuButtonBinding LoadMenuButtonBinding(const std::filesystem::path& directory) {
  Common::IniFile ini;
  ini.Load(TurbulenceIni(directory));
  MenuButtonBinding binding;
  auto* section = ini.GetOrCreateSection("Controls");
  section->Get("MenuPad", &binding.pad, binding.pad);
  section->Get("MenuKey", &binding.key, binding.key);
  if (binding.pad.empty() || !BindablePad(binding.pad)) binding.pad = MenuButtonBinding{}.pad;
  if (binding.key.empty() || !BindableKey(binding.key)) binding.key = MenuButtonBinding{}.key;
  return binding;
}
bool SaveMenuButtonBinding(const std::filesystem::path& directory, const MenuButtonBinding& binding) {
  return SaveTurbulenceIni(directory, [&](Common::IniFile& ini) {
    auto* section = ini.GetOrCreateSection("Controls");
    section->Set("MenuPad", binding.pad);
    section->Set("MenuKey", binding.key);
  });
}
}  // namespace turbulence
