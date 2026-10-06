#include "ui.h"
#include "tray.h"
#include <commctrl.h>
#include <objbase.h>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command_line,int show) {
    HANDLE mutex; WNDCLASSEXW wc={0}; HWND window; MSG msg; HRESULT hr; INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};
    (void)previous; (void)command_line;
    mutex=CreateMutexW(NULL,FALSE,L"Local\\GameVoiceBoxSingleInstance");
    if(!mutex) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        window=FindWindowW(GAMEVOICEBOX_WINDOW_CLASS,NULL);
        if(window) { ShowWindow(window,IsIconic(window)?SW_RESTORE:SW_SHOW); SetForegroundWindow(window); SendMessageW(window,WM_APP_RESTORE,0,0); }
        CloseHandle(mutex); return 0;
    }
    hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    if(FAILED(hr)) { MessageBoxW(NULL,L"无法初始化原生界面 COM",L"开麦匣 GameVoiceBox",MB_OK|MB_ICONERROR); CloseHandle(mutex); return 1; }
    InitCommonControlsEx(&controls);
    wc.cbSize=sizeof(wc); wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(APP_ICON_ID)); wc.hIconSm=wc.hIcon; wc.lpfnWndProc=main_window_proc; wc.hInstance=instance; wc.hCursor=LoadCursorW(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.lpszClassName=GAMEVOICEBOX_WINDOW_CLASS;
    if(!RegisterClassExW(&wc)) { CoUninitialize(); CloseHandle(mutex); return 1; }
    {
        UINT dpi=win_dpi_for_system(); RECT work={0}; int width=MulDiv(840,(int)dpi,96),height=MulDiv(960,(int)dpi,96);
        SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        if(width>work.right-work.left) width=work.right-work.left;
        if(height>work.bottom-work.top) height=work.bottom-work.top;
        window=CreateWindowExW(0,GAMEVOICEBOX_WINDOW_CLASS,L"开麦匣 GameVoiceBox",WS_OVERLAPPEDWINDOW|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,width,height,NULL,NULL,instance,NULL);
    }
    if(!window) { CoUninitialize(); CloseHandle(mutex); return 1; }
    ShowWindow(window,show); UpdateWindow(window);
    while(GetMessageW(&msg,NULL,0,0)>0) {
        /* Observe only registered app hotkeys before dialog navigation filtering. */
        if(msg.message==WM_HOTKEY && msg.hwnd==window) ui_hotkey_queue_received(window,(int)msg.wParam);
        /* IsDialogMessage provides native keyboard navigation. Esc is routed
           only while this application's window is foreground, never global. */
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE && GetForegroundWindow()==window) { SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,msg.lParam); continue; }
        {
            HWND root=GetAncestor(msg.hwnd,GA_ROOT); bool tab=msg.message==WM_KEYDOWN && msg.wParam==VK_TAB;
            if(!root) root=window;
            if(!IsDialogMessageW(root,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if(tab && root==window) ui_focus_visible(window);
        }
    }
    CoUninitialize(); CloseHandle(mutex); return (int)msg.wParam;
}
