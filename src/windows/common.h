#ifndef RONGYU_WINDOWS_COMMON_H
#define RONGYU_WINDOWS_COMMON_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <stdbool.h>
UINT win_dpi_for_window(HWND hwnd);
UINT win_dpi_for_system(void);
wchar_t *win_utf8_to_wide(const char *text);
char *win_wide_to_utf8(const wchar_t *text);
void win_error(char error[256],const char *operation,HRESULT hr);
bool win_root_path(wchar_t *out,size_t count,const wchar_t *suffix);
#endif
