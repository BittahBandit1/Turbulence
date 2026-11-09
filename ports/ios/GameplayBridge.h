#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// All callers use the UI thread. The render view must outlive the worker.
void TurbulenceStart(void* view, void* layer, float scale, const char* game, const char* user);
void TurbulenceStop(void);
void TurbulenceToggleMenu(void);
int TurbulenceMenuOpen(void);
void TurbulenceMenuButtons(uint32_t mask);
void TurbulenceMenuPointer(double x, double y, int down, double scroll);
void TurbulenceDisplayRate(double rate);
void TurbulenceControllerStyle(int style);
void TurbulenceResize(void);
void TurbulencePause(int paused);
void TurbulenceInput(int control, double value);
// 0 idle, 1 starting/running, 2 stopped, 3 failed. Copies the latest message.
int TurbulenceStatus(char* message, int capacity);
#ifdef __cplusplus
}
#endif
