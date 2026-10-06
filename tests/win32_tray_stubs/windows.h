#ifndef REVIEW_WINDOWS_H
#define REVIEW_WINDOWS_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
typedef unsigned UINT; typedef int BOOL; typedef int HRESULT; typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef void *HWND; typedef void *HICON; typedef void *HMENU; typedef void *HINSTANCE; typedef struct {int x,y;} POINT;
#define FALSE 0
#define WM_APP 0x8000
#define WM_LBUTTONDBLCLK 0x0203
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONUP 0x0205
#define WM_CONTEXTMENU 0x007b
#define WM_CLOSE 0x0010
#define WM_NULL 0
#define SW_HIDE 0
#define SW_SHOW 5
#define SW_RESTORE 9
#define MF_STRING 0
#define MF_SEPARATOR 0x800
#define TPM_RETURNCMD 0x100
#define TPM_NONOTIFY 0x080
#define TPM_RIGHTBUTTON 0x002
#define LOWORD(x) ((unsigned)(uintptr_t)(x)&0xffff)
#define HIWORD(x) (((unsigned)(uintptr_t)(x)>>16)&0xffff)
#define MAKEINTRESOURCEW(x) ((const wchar_t *)(uintptr_t)(x))
#define IDI_APPLICATION MAKEINTRESOURCEW(32512)
BOOL ShowWindow(HWND,int); BOOL IsIconic(HWND); BOOL SetForegroundWindow(HWND); UINT RegisterWindowMessageW(const wchar_t *); HINSTANCE GetModuleHandleW(const wchar_t *); HICON LoadIconW(HINSTANCE,const wchar_t *); BOOL GetCursorPos(POINT *); HMENU CreatePopupMenu(void); BOOL AppendMenuW(HMENU,UINT,uintptr_t,const wchar_t *); BOOL SetMenuDefaultItem(HMENU,UINT,UINT); UINT TrackPopupMenuEx(HMENU,UINT,int,int,HWND,void *); BOOL DestroyMenu(HMENU); BOOL PostMessageW(HWND,UINT,WPARAM,LPARAM);
#endif
