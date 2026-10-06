#ifndef REVIEW_SHELLAPI_H
#define REVIEW_SHELLAPI_H
#include <windows.h>
typedef struct {UINT cbSize; HWND hWnd; UINT uID,uFlags,uCallbackMessage; HICON hIcon; wchar_t szTip[128]; UINT uVersion;} NOTIFYICONDATAW;
#define NIF_MESSAGE 1
#define NIF_ICON 2
#define NIF_TIP 4
#define NIF_SHOWTIP 128
#define NIM_ADD 0
#define NIM_SETVERSION 4
#define NIM_DELETE 2
#define NIM_SETFOCUS 3
#define NOTIFYICON_VERSION_4 4
#define NIN_SELECT 0x0400
#define NIN_KEYSELECT 0x0401
BOOL Shell_NotifyIconW(UINT,NOTIFYICONDATAW *);
#endif
