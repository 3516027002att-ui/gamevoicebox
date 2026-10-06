/* Tests actual Win32 adapter against explicit platform substitutes, not Windows. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/windows/tray.h"
#include <shellapi.h>
static bool version_ok=true,add_ok=true,message_ok=true;
static int restores,hides,adds,deletes,posts;
static UINT menu_command,last_post;
BOOL ShowWindow(HWND h,int mode) { assert(h); if(mode==SW_HIDE)++hides;else ++restores;return 1; }
BOOL IsIconic(HWND h) {assert(h);return 1;}
BOOL SetForegroundWindow(HWND h) {assert(h);return 1;}
UINT RegisterWindowMessageW(const wchar_t *s) {assert(!wcscmp(s,L"TaskbarCreated"));return message_ok?0xc000:0;}
HINSTANCE GetModuleHandleW(const wchar_t *s) {(void)s;return (void *)1;}
HICON LoadIconW(HINSTANCE h,const wchar_t *s) {(void)h;(void)s;return (void *)2;}
BOOL GetCursorPos(POINT *p) {p->x=4;p->y=7;return 1;}
HMENU CreatePopupMenu(void) {return (void *)3;}
BOOL AppendMenuW(HMENU h,UINT a,uintptr_t b,const wchar_t *s) {(void)h;(void)a;(void)b;(void)s;return 1;}
BOOL SetMenuDefaultItem(HMENU h,UINT a,UINT b) {(void)h;(void)a;(void)b;return 1;}
UINT TrackPopupMenuEx(HMENU h,UINT a,int x,int y,HWND w,void *p) {(void)h;(void)a;(void)x;(void)y;(void)w;(void)p;return menu_command;}
BOOL DestroyMenu(HMENU h) {(void)h;return 1;}
BOOL PostMessageW(HWND h,UINT a,WPARAM b,LPARAM c) {assert(h);(void)b;(void)c;++posts;last_post=a;if(a==WM_CLOSE)assert(restores>0);return 1;}
BOOL Shell_NotifyIconW(UINT cmd,NOTIFYICONDATAW *n) {
 assert(n->hWnd && n->uID==1);
 if(cmd==NIM_ADD) {++adds;assert(n->hIcon && n->uCallbackMessage==WM_TRAY_ICON && n->szTip[0]);return add_ok;}
 if(cmd==NIM_SETVERSION) {assert(n->uVersion==NOTIFYICON_VERSION_4);return version_ok;}
 if(cmd==NIM_DELETE)++deletes;
 return 1;
}
static void reset_counts(void) {restores=hides=adds=deletes=posts=0;last_post=0;}
int main(void) {
 TrayWindow t={0};bool failed=false;
 assert(!tray_window_minimize(&t));assert(!hides && !adds);
 version_ok=false;assert(tray_window_init(&t,(HWND)1,L"测试音板"));assert(tray_window_minimize(&t));
 reset_counts();assert(tray_window_message(&t,WM_TRAY_ICON,1,WM_LBUTTONUP,&failed));assert(restores==1 && !t.state.hidden);
 tray_window_remove(&t);version_ok=true;assert(tray_window_init(&t,(HWND)1,L"测试音板"));assert(tray_window_minimize(&t));reset_counts();
 assert(tray_window_message(&t,WM_TRAY_ICON,0,((LPARAM)2<<16)|NIN_SELECT,&failed));assert(!restores && t.state.hidden);
 assert(tray_window_message(&t,WM_TRAY_ICON,0,((LPARAM)1<<16)|NIN_KEYSELECT,&failed));assert(restores==1 && !t.state.hidden);
 assert(tray_window_minimize(&t));reset_counts();menu_command=2;
 assert(tray_window_message(&t,WM_TRAY_ICON,0,((LPARAM)1<<16)|WM_CONTEXTMENU,&failed));assert(restores==1 && last_post==WM_CLOSE && posts==2);
 assert(tray_window_minimize(&t));reset_counts();add_ok=false;
 assert(tray_window_message(&t,t.taskbar_created,0,0,&failed));assert(failed && restores==1 && !t.state.hidden && !t.state.installed);
 reset_counts();assert(!tray_window_minimize(&t));assert(restores==1 && !hides);
 add_ok=true;assert(tray_window_minimize(&t));reset_counts();tray_window_remove(&t);assert(deletes==1);tray_window_remove(&t);assert(deletes==1);
 message_ok=false;reset_counts();assert(!tray_window_init(&t,(HWND)1,L"测试音板"));assert(!adds);assert(!tray_window_minimize(&t));assert(!hides);
 puts("actual tray adapter contract: preinit guard, legacy/version4 callbacks, exit visibility, Explorer failure and cleanup passed");
 return 0;
}
