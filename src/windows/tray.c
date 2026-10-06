#include "tray.h"
#include <shellapi.h>
#include <string.h>
#include <wchar.h>
#define TRAY_ICON_ID 1u
#define TRAY_RESTORE 1u
#define TRAY_EXIT 2u
static NOTIFYICONDATAW notification(TrayWindow *t) {
    NOTIFYICONDATAW n={0}; n.cbSize=sizeof(n); n.hWnd=t->hwnd; n.uID=TRAY_ICON_ID;
    return n;
}
static bool add(void *ctx) {
    TrayWindow *t=ctx; NOTIFYICONDATAW n=notification(t);
    n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP; n.uCallbackMessage=WM_TRAY_ICON; n.hIcon=t->icon;
    if(!t->hwnd || !t->name) return false;
    wcsncpy(n.szTip,t->name,sizeof(n.szTip)/sizeof(n.szTip[0])-1);
    if(!t->icon || !t->taskbar_created || !Shell_NotifyIconW(NIM_ADD,&n)) return false;
    n.uVersion=NOTIFYICON_VERSION_4;
    t->version4=Shell_NotifyIconW(NIM_SETVERSION,&n)!=FALSE;
    return true;
}
static void remove_icon(void *ctx) {
    TrayWindow *t=ctx; NOTIFYICONDATAW n=notification(t);
    Shell_NotifyIconW(NIM_DELETE,&n);
}
static void hide(void *ctx) { TrayWindow *t=ctx; ShowWindow(t->hwnd,SW_HIDE); }
static void restore(void *ctx) {
    TrayWindow *t=ctx;
    ShowWindow(t->hwnd,IsIconic(t->hwnd)?SW_RESTORE:SW_SHOW);
    SetForegroundWindow(t->hwnd);
}
static TrayOps operations(TrayWindow *t) { TrayOps ops={t,add,remove_icon,hide,restore}; return ops; }
bool tray_window_init(TrayWindow *t,HWND hwnd,const wchar_t *name) {
    memset(t,0,sizeof(*t)); t->hwnd=hwnd; t->name=name;
    t->taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");
    t->icon=LoadIconW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(APP_ICON_ID));
    if(!t->icon) t->icon=LoadIconW(NULL,IDI_APPLICATION);
    t->state.installed=add(t); return t->state.installed;
}
bool tray_window_minimize(TrayWindow *t) { if(!t || !t->hwnd || !t->name) return false; TrayOps ops=operations(t); return tray_minimize(&t->state,&ops); }
void tray_window_restore(TrayWindow *t) { TrayOps ops=operations(t); tray_restore(&t->state,&ops); }
void tray_window_remove(TrayWindow *t) { TrayOps ops=operations(t); tray_remove(&t->state,&ops); }
static void menu(TrayWindow *t) {
    POINT point; HMENU popup; UINT command; NOTIFYICONDATAW n;
    if(!GetCursorPos(&point)) { tray_window_restore(t); return; }
    popup=CreatePopupMenu();
    if(!popup) { tray_window_restore(t); return; }
    AppendMenuW(popup,MF_STRING,TRAY_RESTORE,L"恢复窗口");
    AppendMenuW(popup,MF_SEPARATOR,0,NULL);
    AppendMenuW(popup,MF_STRING,TRAY_EXIT,L"退出");
    SetMenuDefaultItem(popup,TRAY_RESTORE,FALSE);
    SetForegroundWindow(t->hwnd);
    command=TrackPopupMenuEx(popup,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,t->hwnd,NULL);
    DestroyMenu(popup); PostMessageW(t->hwnd,WM_NULL,0,0);
    if(command==TRAY_RESTORE) tray_window_restore(t);
    else if(command==TRAY_EXIT) {
        /* Ensure a failed-save/cancel-exit dialog cannot leave a hidden app. */
        tray_window_restore(t); PostMessageW(t->hwnd,WM_CLOSE,0,0);
    } else { n=notification(t); Shell_NotifyIconW(NIM_SETFOCUS,&n); }
}
bool tray_window_message(TrayWindow *t,UINT message,WPARAM wp,LPARAM lp,bool *recovery_failed) {
    UINT event;
    (void)wp; if(recovery_failed) *recovery_failed=false;
    if(t->taskbar_created && message==t->taskbar_created) {
        TrayOps ops=operations(t);
        if(!tray_recover(&t->state,&ops) && recovery_failed) *recovery_failed=true;
        return true;
    }
    if(message!=WM_TRAY_ICON) return false;
    event=t->version4?LOWORD(lp):(UINT)lp;
    if(t->version4 && HIWORD(lp)!=TRAY_ICON_ID) return true;
    if(event==WM_LBUTTONDBLCLK || (!t->version4 && event==WM_LBUTTONUP) || event==NIN_SELECT || event==NIN_KEYSELECT) tray_window_restore(t);
    else if(event==WM_CONTEXTMENU || event==WM_RBUTTONUP) menu(t);
    return true;
}
