// Turbulence options menu (ported from ReFare's, by the same authors).
#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace turbulence {
enum OptionsButton { Toggle, Up, Down, Left, Right, Back, Confirm, Count };
using OptionsInput = std::array<bool, Count>;
// The controller button and keyboard key that open the options menu, by
// Dolphin input name (SDL controller / macOS keyboard).
struct MenuButtonBinding { std::string pad = "Shoulder R"; std::string key = "Tab"; };
// What is held right now while the menu waits for a new menu button.
struct MenuButtonCapture {
  bool cancel = false;  // Circle or Escape
  bool held = false;    // anything at all is held
  std::string pad;      // a bindable controller button, if one is held
  std::string key;      // a bindable key, if one is held
};
class OptionsMenu {
public:
  // request_exit is called (from the render thread) after the player confirms
  // "Exit The Game". It must only request a shutdown, never block.
  // render_view: the platform's render window (macOS NSView), for the Metal HUD.
  explicit OptionsMenu(std::filesystem::path user_directory, void* render_view,
                       std::function<void()> request_exit = {},
                       std::function<OptionsInput()> input_source = {},
                       std::function<MenuButtonCapture()> capture_source = {});
  ~OptionsMenu();
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
bool OptionsMenuBlocksInput();
// Explicit automation pad commands also navigate the host settings page.
void SetOptionsAutomationInput(const OptionsInput& input);
// First thing at startup, before Metal is touched: enables the Metal HUD
// machinery (shown or hidden later from the saved preference) and keeps macOS
// from napping or throttling the game.
void PrepareMetalHUD();
// Before the video backend is created: saved Turbulence.ini preferences the
// renderer reads at startup.
void PrepareOptions(const std::filesystem::path& user_directory);
MenuButtonBinding LoadMenuButtonBinding(const std::filesystem::path& user_directory);
bool LoadMetalHUDPreference(const std::filesystem::path& user_directory);
// FPS counter: 0 off, 1 FPS, 2 FPS + frame-interval graph ([Video] FPSCounter).
int LoadPerfOverlayPreference(const std::filesystem::path& user_directory);
bool SaveMenuButtonBinding(const std::filesystem::path& user_directory, const MenuButtonBinding& binding);
}  // namespace turbulence
