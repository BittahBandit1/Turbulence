// Turbulence options menu, ported from ReFare's (src/refare_options.cpp) with
// a Sonic Riders palette. Drawn through Dolphin's ImGui overlay on the render
// thread; reads controllers through Dolphin's controller interface, so it
// respects window focus like the game does.
#include "turbulence_options.hpp"
#include "turbulence_native_text.hpp"
#include "VideoCommon/TurbulenceNativeMenu.h"
#include "turbulence_metal.hpp"
#include "turbulence_prompts.hpp"
#include "Common/Config/Config.h"
#include "Common/IniFile.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Host.h"
#include "Core/System.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <cstdlib>
#include <string>
#include <vector>

// Defined in Dolphin's VideoCommon (HUD layout).
extern bool g_turbulence_hud_stretched;
// Defined by the runtime (dolphin_runtime.cpp): asks the desktop platform to
// enter or leave fullscreen on its own window thread.
void Host_RequestFullscreen(bool fullscreen);

namespace turbulence {
namespace {
std::atomic<bool> s_blocks_input{false};
std::atomic<unsigned> s_automation_buttons{0};

// Sonic Riders palette, sampled from the game: indigo panels and the teal
// selection pill with a bright cyan outline from its own menus (save data,
// Player Entry), the logo's gold for the title, and the orange-red of the
// logo's "R" for Exit.
struct Palette {
  ImVec4 window{0.075f, 0.070f, 0.300f, 0.96f};       // menu indigo #13124D
  ImVec4 border{0.150f, 0.840f, 0.930f, 0.90f};       // selection outline #26D6ED
  ImVec4 title{1.000f, 0.820f, 0.080f, 1.00f};        // logo gold #FFD114
  ImVec4 accent{0.150f, 0.840f, 0.930f, 1.00f};       // cyan
  ImVec4 text{0.960f, 0.970f, 1.000f, 1.00f};
  ImVec4 text_dim{0.600f, 0.660f, 0.860f, 1.00f};
  ImVec4 button{0.140f, 0.150f, 0.440f, 1.00f};
  ImVec4 button_hover{0.130f, 0.430f, 0.500f, 1.00f};
  ImVec4 button_active{0.170f, 0.520f, 0.580f, 1.00f};
  ImVec4 row_selected{0.170f, 0.500f, 0.560f, 0.90f}; // selection pill teal #2B808F
  ImVec4 row_hover{0.170f, 0.500f, 0.560f, 0.45f};
  ImVec4 focus{0.150f, 0.840f, 0.930f, 1.00f};        // cyan
  ImVec4 focus_text{0.050f, 0.050f, 0.200f, 1.00f};   // indigo on cyan
  ImVec4 danger{0.910f, 0.290f, 0.100f, 1.00f};       // logo "R" #E84A1A
  ImVec4 separator{0.150f, 0.840f, 0.930f, 0.35f};
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
// Button prompt preference, as saved in Turbulence.ini and shown in the menu.
constexpr const char* kPromptNames[] = {"auto", "gamecube", "xbox", "playstation"};
constexpr const char* kPromptLabels[] = {"Automatic", "GameCube", "Xbox", "PlayStation"};
// Graphics API row: Dolphin backend names (config.ini [Video] backend) and labels.
constexpr const char* kApiBackends[] = {"Vulkan", "D3D"};
constexpr const char* kApiLabels[] = {"Vulkan", "Direct3D 11"};
// Dolphin post-processing shader shipped in Sys/Shaders/FXAA.glsl.
constexpr const char* kFXAAShader = "FXAA";

enum class Row { Scale, Aspect, Fullscreen, GraphicsApi, HudLayout, FXAA, VSync, MetalHUD, PerfOverlay, Prompts, MenuButton, Exit };
const std::vector<Row>& Rows() {
  static const std::vector<Row> rows = {Row::Scale, Row::Aspect,
#if !defined(TURBULENCE_IOS) && !defined(TURBULENCE_ANDROID)
    Row::Fullscreen,  // desktop only: mobile is always full screen
#endif
#if defined(_WIN32)
    Row::GraphicsApi,  // Vulkan or Direct3D 11, applied at the next launch
#endif
    Row::HudLayout, Row::FXAA,
    Row::VSync,
#ifdef __APPLE__
    Row::MetalHUD,
#endif
    Row::PerfOverlay,
    Row::Prompts,
    Row::Exit};
  return rows;
}
const char* RowLabel(Row row) {
  switch (row) {
  case Row::Scale: return "Resolution scale";
  case Row::Aspect: return "Aspect ratio";
  case Row::Fullscreen: return "Fullscreen";
  case Row::GraphicsApi: return "Graphics API";
  case Row::HudLayout: return "HUD and menus";
  case Row::FXAA: return "FXAA";
  case Row::VSync: return "V-Sync";
  case Row::MetalHUD: return "Metal HUD";
  case Row::PerfOverlay: return "FPS counter";
  case Row::Prompts: return "Button prompts";
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
      result[Up] |= held("Pad N") || held("Left Y+"); result[Down] |= held("Pad S") || held("Left Y-");
      result[Left] |= held("Pad W") || held("Left X-"); result[Right] |= held("Pad E") || held("Left X+");
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
// FPS counter: frame rate plus the interval between new game frames, as the
// renderer presented them (duplicate presents of an unchanged frame are folded
// into the next interval, so a dropped frame shows as a ~33 ms spike).
constexpr const char* kPerfLabels[] = {"Off", "On", "On and frame times"};
class FrameIntervals {
public:
  static constexpr int kSize = 240;  // 4 seconds at 60 fps
  void Record(const PresentInfo& info) {
    if (info.reason == PresentInfo::PresentReason::VideoInterfaceDuplicate) return;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard guard(m_lock);
    if (m_have_last) {
      const float ms = std::chrono::duration<float, std::milli>(now - m_last).count();
      // A pause or load screen gap is not a frame interval.
      if (ms < 250.0f) { m_ring[m_next] = ms; m_next = (m_next + 1) % kSize; m_count = std::min(m_count + 1, kSize); }
    }
    m_last = now; m_have_last = true;
  }
  // Oldest first.
  int Copy(std::array<float, kSize>& out) const {
    std::lock_guard guard(m_lock);
    for (int i = 0; i < m_count; ++i) out[i] = m_ring[(m_next - m_count + i + kSize) % kSize];
    return m_count;
  }
private:
  mutable std::mutex m_lock;
  std::array<float, kSize> m_ring{};
  int m_next = 0, m_count = 0;
  bool m_have_last = false;
  std::chrono::steady_clock::time_point m_last{};
};

void DrawPerfOverlay(const FrameIntervals& intervals, int mode) {
  const Palette& p = kPalette;
  std::array<float, FrameIntervals::kSize> data{};
  const int n = intervals.Copy(data);
  // Last second of intervals for the numbers; the graph shows all of them.
  float sum = 0, low = 1e9f, high = 0;
  const int recent = std::min(n, 60);
  for (int i = n - recent; i < n; ++i) { sum += data[i]; low = std::min(low, data[i]); high = std::max(high, data[i]); }
  const float avg = recent ? sum / recent : 0.0f;
  int over = 0;  // intervals longer than 1.5 frames (visible hitches) in the window
  for (int i = 0; i < n; ++i) if (data[i] > 25.0f) ++over;
  const auto& metrics = Core::System::GetInstance().GetPerfMetrics();
  const double fps = metrics.GetFPS(), vps = metrics.GetVPS(), speed = metrics.GetSpeed();
  const float scale = std::max(1.0f, ImGui::GetIO().DisplaySize.y / 900.0f);
  ImGui::SetNextWindowPos(ImVec2(8 * scale, 8 * scale), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.72f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * scale, 8 * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, p.window);
  ImGui::PushStyleColor(ImGuiCol_Border, p.separator);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_PlotLines, p.accent);
  ImGui::Begin("Turbulence FPS", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
               ImGuiWindowFlags_NoNav);
  ImGui::SetWindowFontScale(scale);
  const ImVec4 fps_color = fps >= 59.0 ? ImVec4(0.45f, 0.95f, 0.45f, 1) : fps >= 50.0 ? p.title : p.danger;
  ImGui::TextColored(fps_color, "%.1f FPS", fps);
  ImGui::SameLine(); ImGui::TextColored(p.text_dim, "%.0f%% speed", speed * 100.0);
  // The video backend actually in use (Vulkan, Direct3D 11, Metal...), to
  // confirm a backend change took effect.
  if (g_video_backend) {
    ImGui::SameLine(); ImGui::TextColored(p.text_dim, "%s", g_video_backend->GetDisplayName().c_str());
  }
  if (mode >= 2) {
    ImGui::TextColored(p.text, "Frame %.2f ms  (min %.2f / max %.2f)", avg, recent ? low : 0.0f, high);
    ImGui::TextColored(over ? p.title : p.text_dim, "VPS %.1f   hitches >25 ms: %d / %ds", vps, over, FrameIntervals::kSize / 60);
    char label[32];
    std::snprintf(label, sizeof(label), "%.1f ms", n ? data[n - 1] : 0.0f);
    const ImVec2 size(std::max(240 * scale, ImGui::GetContentRegionAvail().x), 48 * scale);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::PlotLines("##intervals", data.data(), n, 0, label, 0.0f, 50.0f, size);
    // 16.7 ms reference line (the plot's range is 0-50 ms, drawn bottom up).
    const float y = origin.y + size.y * (1.0f - 16.667f / 50.0f);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + size.x, y),
                                        ImGui::GetColorU32(ImVec4(1, 1, 1, 0.25f)));
  }
  ImGui::End();
  ImGui::PopStyleColor(4);
  ImGui::PopStyleVar(3);
}
}  // namespace

struct OptionsMenu::Impl {
  std::filesystem::path directory;
  void* render_view = nullptr;
  bool hud_applied = false;
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
  NativeMenuText native_text;
  int scale_index = 0;
  int aspect_index = 0;
  bool stretch_hud = false;
  bool fxaa = false;
  bool vsync = true;
  bool fullscreen = false;
  int api_choice = 0;       // saved for the next launch: 0 Vulkan, 1 Direct3D 11
  std::string running_api;  // backend this session started with
  bool metal_hud = true;
  int perf_overlay = 0;  // 0 off, 1 FPS, 2 FPS + frame times
  FrameIntervals intervals;
  Common::EventHook present_hook;
  bool confirm_exit = false;
  int exit_choice = 0;  // 0 = No (left, default), 1 = Yes (right)
  bool exiting = false;
  std::string save_error;
  mutable std::string value_text;
  MenuButtonBinding binding;

  Impl(std::filesystem::path path, void* view, std::function<void()> exit,
       std::function<OptionsInput()> source, std::function<MenuButtonCapture()> capture)
      : directory(std::move(path)), render_view(view),
        input_source(std::move(source)),
        request_exit(std::move(exit)),
        capture_source(capture ? std::move(capture) : ReadCapture) {
    TurbulenceNativeMenu::host_ui_available=true;
    s_menu_binding = LoadMenuButtonBinding(directory);
    binding = s_menu_binding;
    metal_hud = LoadMetalHUDPreference(directory);
    perf_overlay = LoadPerfOverlayPreference(directory);
    present_hook = GetVideoEvents().after_present_event.Register(
        [this](const PresentInfo& info) { intervals.Record(info); });
    draw_hook = GetVideoEvents().imgui_draw_event.Register([this] { Draw(); });
    // Unattended tests (scripts/turbulence_agent.sh) can't press the real menu
    // button: automation input drives the emulated GameCube pad.
    if (const char* open_now = std::getenv("TURBULENCE_OPTIONS_OPEN"); open_now && *open_now == '1')
      open_on_first_draw = true;
  }
  bool open_on_first_draw = false;
  ~Impl() {
    draw_hook.reset();present_hook.reset();s_blocks_input=false;
    TurbulenceNativeMenu::host_ui_available=false;
    TurbulenceNativeMenu::settings_page=false;
    TurbulenceNativeMenu::request_settings=false;
  }
  Row CurrentRow() const { return Rows()[selected]; }
  void ReadSettings() {
    stretch_hud = g_turbulence_hud_stretched;
    fxaa = Config::Get(Config::GFX_ENHANCE_POST_SHADER) == kFXAAShader;
    vsync = Config::Get(Config::GFX_VSYNC);
    // Alt+Enter / Cmd+Return / the window menu can change it too.
    fullscreen = Host_RendererIsFullscreen();
    running_api = g_video_backend ? g_video_backend->GetConfigName() : "";
    api_choice = running_api == kApiBackends[1] ? 1 : 0;
    {  // A choice saved earlier this session wins over the running backend.
      Common::IniFile frontend;
      std::string saved;
      if (frontend.Load((directory / "config.ini").string()) &&
          frontend.GetOrCreateSection("Video")->Get("backend", &saved))
        for (int i = 0; i < 2; ++i)
          if (saved == kApiBackends[i]) api_choice = i;
    }
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
    if (std::filesystem::exists(file)) {
      if (!frontend.Load(file)) { save_error = "Could not read settings; change applies this session."; return; }
      frontend.GetOrCreateSection("Video")->Set("resolution", std::string(scales[scale_index].resolution));
      if (!frontend.Save(file)) { save_error = "Could not save resolution; change applies this session."; return; }
    }
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
  // The backend can't change while the game runs: save it for the launcher's
  // next start (config.ini [Video] backend; --graphics still overrides it).
  void ApplyGraphicsApi() {
    Common::IniFile frontend;
    const auto file = (directory / "config.ini").string();
    frontend.Load(file);
    frontend.GetOrCreateSection("Video")->Set("backend", std::string(kApiBackends[api_choice]));
    save_error = frontend.Save(file) ? "" : "Could not save the graphics API.";
  }
  bool RestartPending() const { return !running_api.empty() && running_api != kApiBackends[api_choice]; }
  void ApplyFullscreen() {
    Host_RequestFullscreen(fullscreen);
    save_error = SaveTurbulenceIni(directory, [&](Common::IniFile& ini) {
      ini.GetOrCreateSection("Video")->Set("Fullscreen", fullscreen);
    }) ? "" : "Could not save the fullscreen setting; change applies this session.";
  }
  void ApplyMetalHUD() {
    SetMetalHUDVisible(render_view, metal_hud);
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
    case Row::Fullscreen: fullscreen = !fullscreen; ApplyFullscreen(); break;
    case Row::GraphicsApi: api_choice = 1 - api_choice; ApplyGraphicsApi(); break;
    case Row::HudLayout: stretch_hud = !stretch_hud; ApplyHudLayout(); break;
    case Row::FXAA: fxaa = !fxaa; ApplyFXAA(); break;
    case Row::VSync: vsync = !vsync; ApplyVSync(); break;
    case Row::MetalHUD: metal_hud = !metal_hud; ApplyMetalHUD(); break;
    case Row::PerfOverlay:
      perf_overlay = (perf_overlay + (delta > 0 ? 1 : 2)) % 3;
      save_error = SaveTurbulenceIni(directory, [&](Common::IniFile& ini) {
        ini.GetOrCreateSection("Video")->Set("FPSCounter", perf_overlay);
      }) ? "" : "Could not save the FPS counter; change applies this session.";
      break;
    case Row::Prompts: {
      const int next = (static_cast<int>(GetPromptPreference()) + delta + 4) % 4;
      SetPromptPreference(static_cast<PromptPreference>(next));
      save_error = SaveTurbulenceIni(directory, [&](Common::IniFile& ini) {
        ini.GetOrCreateSection("Controls")->Set("ButtonPrompts", std::string(kPromptNames[next]));
      }) ? "" : "Could not save the prompt style; change applies this session.";
      break;
    }
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
    case Row::Fullscreen: case Row::GraphicsApi: case Row::HudLayout: case Row::FXAA: case Row::VSync: case Row::MetalHUD: case Row::PerfOverlay: case Row::Prompts: Step(1); break;
    default: break;
    }
  }
  void Close() {
    if(TurbulenceNativeMenu::settings_page.load()) TurbulenceNativeMenu::close_settings=true;
    open = false; confirm_exit = false; capturing = false; drain = true;
  }
  void FinishExit() {
    exiting = true;
    Close();
    if (request_exit) request_exit();
  }
  const char* RowValue(Row row) const {
    switch (row) {
    case Row::Scale: return scales[scale_index].label;
    case Row::Aspect: return aspects[aspect_index].label;
    case Row::Fullscreen: return fullscreen ? "On" : "Off";
    case Row::GraphicsApi: return kApiLabels[api_choice];
    case Row::HudLayout: return stretch_hud ? "Stretched" : "Centered (4:3)";
    case Row::FXAA: return fxaa ? "On" : "Off";
    case Row::VSync: return vsync ? "On" : "Off";
    case Row::MetalHUD: return metal_hud ? "On" : "Off";
    case Row::PerfOverlay: return kPerfLabels[perf_overlay];
    case Row::Prompts: {
      static constexpr const char* styles[] = {"GameCube", "Xbox", "PlayStation"};
      if (GetPromptPreference() != PromptPreference::Automatic)
        return kPromptLabels[static_cast<int>(GetPromptPreference())];
      value_text = std::string("Automatic (") + styles[CurrentPromptStyle() % 3] + ")";
      return value_text.c_str();
    }
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
  void DrawNativeSettings() {
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    float w=display.x,h=display.y;
    if(aspect_index>0) {
      const float aspect=float(aspects[aspect_index].width)/aspects[aspect_index].height;
      w=std::min(w,h*aspect);h=std::min(h,w/aspect);
    }
    const float sy=h/480.f,sx=stretch_hud?w/640.f:sy;
    const ImVec2 origin{(display.x-640*sx)*.5f,(display.y-h)*.5f};
    const auto point=[&](float x,float y){return ImVec2{origin.x+x*sx,origin.y+y*sy};};
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("##NativeRecompSettings",nullptr,
      ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
      ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|
      ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto* draw=ImGui::GetWindowDrawList();
    const ImU32 white=IM_COL32_WHITE,cyan=IM_COL32(0,240,238,255);
    const auto text=[&](float x,float y,float size,ImU32 color,const char* value,float max=0) {
      native_text.Draw(draw,point(x,y),size,sx,sy,color,value,max);
    };
    const auto hit=[&](const char* id,float x,float y,float width,float height) {
      ImGui::SetCursorScreenPos(point(x,y));
      return ImGui::InvisibleButton(id,{width*sx,height*sy});
    };
    text(48,38,24,white,"Recomp Settings",275);
    const int first=std::clamp(selected-5,0,std::max(0,int(Rows().size())-6));
    TurbulenceNativeMenu::settings_selection=selected-first;
    for(int i=first;i<std::min(first+6,int(Rows().size()));++i) {
      const float y=130+(i-first)*29.f;
      text(80,y,21,i==selected?cyan:white,RowLabel(Rows()[i]),185);
      if(hit(RowLabel(Rows()[i]),40,y-1,237,28)) selected=i;
    }
    const auto centered=[&](float y,float size,ImU32 color,const char* value) {
      const float width=std::min(211.f,native_text.Width(value,size));
      text(468.f-width*.5f,y,size,color,value,211);
    };
    centered(165,20,cyan,RowLabel(CurrentRow()));
    if(confirm_exit) {
      text(365,202,19,white,"Exit Turbulence?",205);
      text(365,245,19,exit_choice==0?cyan:white,"No");
      text(492,245,19,exit_choice==1?cyan:white,"Yes");
      if(hit("##keep-playing",355,235,95,36)) confirm_exit=false;
      if(hit("##exit-game",480,235,95,36)) FinishExit();
    } else {
      centered(198,19,white,RowValue(CurrentRow()));
      if(CurrentRow()!=Row::Exit) {
        if(hit("##decrease",350,154,112,148)) Step(-1);
        if(hit("##increase",462,154,120,148)) Step(1);
      } else if(hit("##confirm-exit",350,154,232,148)) Activate();
    }
    text(80,307,13,white,(std::to_string(selected+1)+" / "+std::to_string(Rows().size())).c_str());
    text(35,380,17,white,"Left / Right: change. Changes are saved automatically.",580);
    if(!save_error.empty()) text(35,410,14,IM_COL32(255,150,120,255),save_error.c_str(),580);
    ImGui::End();ImGui::PopStyleVar(2);
  }
  void Draw() {
    if (!ImGui::GetCurrentContext()) return;
    if (!hud_applied) {
      PrioritizeCurrentThread();  // Render thread.
      // With MTL_HUD_ENABLED set at launch the HUD is already showing. Only
      // touch the layer for a saved Off (see SetMetalHUDVisible).
      if (!metal_hud) SetMetalHUDVisible(render_view, false);
      hud_applied = true;
    }
    if (exiting) { s_blocks_input = true; return; }
    if (open_on_first_draw) { open_on_first_draw = false; open = true; ReadSettings(); }
    if (TurbulenceNativeMenu::request_settings.exchange(false)) {
      open=true; selected=0; confirm_exit=false; previous.fill(true); ReadSettings();
    }
    // The game opens this page. Poll menu controls only while it owns input,
    // including the release of the Back button after closing.
    if (!open && !drain) {
      // Mobile input providers also drain queued touch events.
      if (input_source) input_source();
      if (perf_overlay) DrawPerfOverlay(intervals, perf_overlay);
      return;
    }
    Buttons down = input_source ? input_source() : ReadButtons();
    const unsigned automation=s_automation_buttons.load();
    for(int i=0;i<Count;++i) down[i]=down[i] || (automation & (1u<<i));
    Buttons pressed{};
    for (int i=0;i<Count;++i) pressed[i] = down[i] && !previous[i];
    previous = down;
    if (capturing) UpdateCapture();
    else if (open && pressed[Back] && !confirm_exit) Close();
    else if (open) HandleInput(pressed);
    if (drain && std::none_of(down.begin(), down.end(), [](bool b){return b;})) drain = false;
    s_blocks_input = open || drain;
    if (perf_overlay) DrawPerfOverlay(intervals, perf_overlay);
    if (!open) return;
    if(TurbulenceNativeMenu::settings_page.load()) { DrawNativeSettings(); return; }
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
      ImGui::PushStyleColor(ImGuiCol_Text, p.text);
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
    if (RestartPending())
      ImGui::TextColored(p.title, "Restart Turbulence to switch to %s.", kApiLabels[api_choice]);
    if (!save_error.empty()) ImGui::TextColored(p.danger, "%s", save_error.c_str());
    if (ImGui::Button("Done",ImVec2(-1,34))) Close();
    ImGui::End();
    PopTheme();
    if (confirm_exit) DrawConfirmExit();
    s_blocks_input = open || drain || exiting;
  }
};

OptionsMenu::OptionsMenu(std::filesystem::path directory, void* render_view,
                         std::function<void()> request_exit,
                         std::function<OptionsInput()> source,
                         std::function<MenuButtonCapture()> capture_source)
  : m_impl(std::make_unique<Impl>(std::move(directory), render_view, std::move(request_exit),
                                  std::move(source), std::move(capture_source))) {}
OptionsMenu::~OptionsMenu() = default;
void SetOptionsAutomationInput(const OptionsInput& input) {
  unsigned buttons=0;
  for(int i=0;i<Count;++i) if(input[i]) buttons|=1u<<i;
  s_automation_buttons=buttons;
}
bool OptionsMenuBlocksInput() { return s_blocks_input.load(); }

void PrepareMetalHUD() {
#ifdef __APPLE__
  // Metal reads this when the device is created. Set it even for a saved Off
  // preference, so the HUD can be switched back on without restarting; the
  // menu hides it on its first frame instead.
  setenv("MTL_HUD_ENABLED", "1", 0);
  KeepGameResponsive();
#endif
}

void PrepareOptions(const std::filesystem::path& directory) {
  Common::IniFile ini;
  ini.Load(TurbulenceIni(directory));
  bool stretched = false;
  ini.GetOrCreateSection("Video")->Get("HudStretched", &stretched, false);
  g_turbulence_hud_stretched = stretched;
  std::string prompts = "auto";
  ini.GetOrCreateSection("Controls")->Get("ButtonPrompts", &prompts, "auto");
  for (int i = 0; i < 4; ++i)
    if (prompts == kPromptNames[i]) SetPromptPreference(static_cast<PromptPreference>(i));
}

int LoadPerfOverlayPreference(const std::filesystem::path& directory) {
  Common::IniFile ini;
  ini.Load(TurbulenceIni(directory));
  int mode = 0;
  ini.GetOrCreateSection("Video")->Get("FPSCounter", &mode, 0);
  if (const char* forced = std::getenv("TURBULENCE_FPS_COUNTER"); forced && *forced) mode = std::atoi(forced);
  return std::clamp(mode, 0, 2);
}

int LoadFullscreenPreference(const std::filesystem::path& directory) {
  Common::IniFile ini;
  ini.Load(TurbulenceIni(directory));
  const auto* section = ini.GetSection("Video");
  bool enabled = false;
  if (!section || !section->Exists("Fullscreen")) return -1;
  section->Get("Fullscreen", &enabled, false);
  return enabled ? 1 : 0;
}

bool LoadMetalHUDPreference(const std::filesystem::path& directory) {
  Common::IniFile ini;
  ini.Load(TurbulenceIni(directory));
  bool enabled = true;
  ini.GetOrCreateSection("Video")->Get("MetalHUD", &enabled, true);
  return enabled;
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
