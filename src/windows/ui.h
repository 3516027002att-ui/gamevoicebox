#ifndef RONGYU_WINDOWS_UI_H
#define RONGYU_WINDOWS_UI_H
#include "common.h"
#include "../app/controller.h"
#include "../app/hotkeys.h"
#define GAMEVOICEBOX_WINDOW_CLASS L"GameVoiceBoxWindow"
LRESULT CALLBACK main_window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp);
void ui_focus_visible(HWND hwnd);
void ui_apply_state(HWND hwnd,const ControllerState *state,const HotkeyStatus *hotkeys);
#endif
