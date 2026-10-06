#ifndef SOUNDBOARD_TRAY_H
#define SOUNDBOARD_TRAY_H
#include "common.h"
#include "tray_contract.h"
#define APP_ICON_ID 101
#define WM_APP_RESTORE (WM_APP+2)
#define WM_TRAY_ICON (WM_APP+3)
typedef struct {
    HWND hwnd;
    HICON icon;
    UINT taskbar_created;
    bool version4;
    const wchar_t *name;
    TrayState state;
} TrayWindow;
bool tray_window_init(TrayWindow *tray,HWND hwnd,const wchar_t *name);
bool tray_window_minimize(TrayWindow *tray);
void tray_window_restore(TrayWindow *tray);
void tray_window_remove(TrayWindow *tray);
bool tray_window_message(TrayWindow *tray,UINT message,WPARAM wp,LPARAM lp,bool *recovery_failed);
#endif
