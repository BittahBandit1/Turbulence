// Dynamic button prompts: Sonic Riders' prompt textures follow the controller
// you last used (Xbox, PlayStation or GameCube icons).
#pragma once
#include <memory>

namespace turbulence {
// Automatic follows the last-used controller; the others force a style.
enum class PromptPreference { Automatic = 0, GameCube, Xbox, PlayStation };
void SetPromptPreference(PromptPreference preference);
PromptPreference GetPromptPreference();
// Which icons are showing now (for the options menu): 0 GC, 1 Xbox, 2 PS.
int CurrentPromptStyle();

class ButtonPrompts {
public:
  ButtonPrompts();
  ~ButtonPrompts();
  ButtonPrompts(const ButtonPrompts&) = delete;
  ButtonPrompts& operator=(const ButtonPrompts&) = delete;
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace turbulence
