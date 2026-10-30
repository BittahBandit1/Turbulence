// macOS helpers for the Metal HUD, responsiveness and thread priority
// (ported from ReFare's refare_metal_hud).
#pragma once
namespace turbulence {
#ifdef __APPLE__
// Show or hide Apple's Metal performance HUD on the render view's layer.
void SetMetalHUDVisible(void* render_view, bool enabled);
// Keep App Nap and timer throttling off while the game runs, including when
// another app (Spotlight, a notification) takes focus.
void KeepGameResponsive();
// Run the calling thread at user-interactive QoS (CPU and render threads).
void PrioritizeCurrentThread();
#else
inline void SetMetalHUDVisible(void*, bool) {}
inline void KeepGameResponsive() {}
inline void PrioritizeCurrentThread() {}
#endif
}  // namespace turbulence
