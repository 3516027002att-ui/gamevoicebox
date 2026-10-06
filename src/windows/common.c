#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
wchar_t *win_utf8_to_wide(const char *text) {
    int n; wchar_t *result;
    if(!text || !(n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0))) return NULL;
    result=malloc((size_t)n*sizeof(*result));
    if(result && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,result,n)) { free(result); return NULL; }
    return result;
}
char *win_wide_to_utf8(const wchar_t *text) {
    int n; char *result;
    if(!text || !(n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL))) return NULL;
    result=malloc((size_t)n);
    if(result && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,result,n,NULL,NULL)) { free(result); return NULL; }
    return result;
}
void win_error(char error[256],const char *op,HRESULT hr) {
    snprintf(error,256,"%s failed (0x%08lx)",op,(unsigned long)hr);
}
bool win_root_path(wchar_t *out,size_t count,const wchar_t *suffix) {
    DWORD n=GetModuleFileNameW(NULL,out,(DWORD)count); wchar_t *slash;
    if(!n || n>=count || !(slash=wcsrchr(out,L'\\'))) return false;
    slash[1]=0;
    if(wcslen(out)+wcslen(suffix)+1>count) return false;
    wcscat(out,suffix); return true;
}

UINT win_dpi_for_window(HWND hwnd) {
    typedef UINT (WINAPI *DpiFn)(HWND);
    union { FARPROC raw; DpiFn fn; } proc;
    HMODULE user=GetModuleHandleW(L"user32.dll"); HDC dc; UINT dpi=96;
    proc.raw=GetProcAddress(user,"GetDpiForWindow");
    if(proc.fn) return proc.fn(hwnd);
    dc=GetDC(hwnd); if(dc) { dpi=(UINT)GetDeviceCaps(dc,LOGPIXELSX); ReleaseDC(hwnd,dc); }
    return dpi?dpi:96;
}
UINT win_dpi_for_system(void) {
    typedef UINT (WINAPI *DpiFn)(void);
    union { FARPROC raw; DpiFn fn; } proc;
    HMODULE user=GetModuleHandleW(L"user32.dll");
    proc.raw=GetProcAddress(user,"GetDpiForSystem");
    return proc.fn?proc.fn():win_dpi_for_window(NULL);
}
