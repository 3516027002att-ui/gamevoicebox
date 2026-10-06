#include "ui.h"
#include "ui_contract.h"
#include "devices.h"
#include "wasapi.h"
#include "status_contract.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define WM_AUDIO_STATUS (WM_APP+1)
#define SETTINGS_TIMER 1u
#define FIRST_LABEL 200
#define FIRST_CLIP_STATUS 300
#define LABEL_COUNT UI_LAYOUT_LABEL_COUNT

typedef struct Ui {
    HWND hwnd;
    HWND controls[UI_STATUS+1];
    HWND labels[LABEL_COUNT];
    HWND clip_status[CLIP_COUNT];
    HFONT font,big_font;
    UINT dpi;
    int scroll;
    Settings settings;
    char *settings_path;
    DeviceList devices;
    AudioWorker worker;
    Controller controller;
    HotkeyStatus hotkeys;
    bool closing,initialized,initializing;
    CRITICAL_SECTION status_lock;
    AudioStatusMailbox mailbox;
    ControllerState last_backend;
    char notice[2048];
} Ui;
static Ui *get_ui(HWND h) { return (Ui *)GetWindowLongPtrW(h,GWLP_USERDATA); }
static void notice_with_kind(Ui *u,const wchar_t *message,bool success) {
    char *utf8=win_wide_to_utf8(message);
    if(utf8) { ui_notice_update(u->notice,sizeof(u->notice),utf8,u->initializing,success); free(utf8); }
    ui_apply_state(u->hwnd,&u->controller.state,&u->hotkeys);
}
static void notice(Ui *u,const wchar_t *message) { notice_with_kind(u,message,false); }
static void notice_success(Ui *u,const wchar_t *message) { notice_with_kind(u,message,true); }
static void show_error(Ui *u,const char *error) {
    ui_notice_update(u->notice,sizeof(u->notice),error,u->initializing,false);
    ui_apply_state(u->hwnd,&u->controller.state,&u->hotkeys);
}
static bool submit(void *ctx,const Command *cmd,char error[256]) { Ui *u=ctx; return audio_worker_submit(&u->worker,cmd,error); }
static void audio_status(void *ctx,const ControllerState *state) {
    Ui *u=ctx; bool post;
    /* Copy only POD. There are no COM pointers in this mailbox. */
    EnterCriticalSection(&u->status_lock); post=audio_mailbox_write(&u->mailbox,state);
    LeaveCriticalSection(&u->status_lock);
    if(post) PostMessageW(u->hwnd,WM_AUDIO_STATUS,0,0);
}
static bool save_settings(Ui *u) {
    char error[256];
    u->settings.sending=u->settings.monitoring=false;
    if(!u->settings_path || !settings_save_atomic(u->settings_path,&u->settings,error)) { show_error(u,u->settings_path?error:"Settings path unavailable"); return false; }
    return true;
}
static void defer_save(Ui *u) { SetTimer(u->hwnd,SETTINGS_TIMER,500,NULL); }
static bool reg_key(void *ctx,int id,const Hotkey *key,char error[256]) {
    Ui *u=ctx;
    if(!RegisterHotKey(u->hwnd,id,key->modifiers|MOD_NOREPEAT,key->key)) {
        snprintf(error,256,"Cannot register %s (0x%08lx); it may already be occupied",key->text,(unsigned long)GetLastError()); return false;
    }
    return true;
}
static bool unreg_key(void *ctx,int id,char error[256]) {
    Ui *u=ctx;
    if(!UnregisterHotKey(u->hwnd,id)) { snprintf(error,256,"Cannot unregister hotkey ID %d (0x%08lx)",id,(unsigned long)GetLastError()); return false; }
    return true;
}
static bool persist_keys(void *ctx,const Hotkey keys[HOTKEY_COUNT],char error[256]) {
    Ui *u=ctx; Settings next=u->settings; memcpy(next.hotkeys,keys,sizeof(next.hotkeys));
    next.sending=next.monitoring=false;
    if(!u->settings_path || !settings_save_atomic(u->settings_path,&next,error)) return false;
    u->settings=next; memcpy(u->controller.settings.hotkeys,keys,sizeof(next.hotkeys)); return true;
}
static void create_settings_path(Ui *u) {
    wchar_t *appdata=NULL,path[32768]; bool recovered=false; char error[256]={0};
    settings_defaults(&u->settings);
    if(FAILED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData,KF_FLAG_CREATE,NULL,&appdata))) { notice(u,L"无法找到 AppData；本次设置不会保存"); return; }
    if(wcslen(appdata)+64>=sizeof(path)/sizeof(path[0])) { CoTaskMemFree(appdata); notice(u,L"AppData 路径过长；本次设置不会保存"); return; }
    swprintf(path,sizeof(path)/sizeof(path[0]),L"%ls\\GameVoiceBox",appdata); CoTaskMemFree(appdata);
    if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) { notice(u,L"无法创建设置目录；本次设置不会保存"); return; }
    wcscat(path,L"\\settings.ini"); u->settings_path=win_wide_to_utf8(path);
    if(!u->settings_path) { notice(u,L"无法编码设置路径；本次设置不会保存"); return; }
    if(!settings_load(u->settings_path,&u->settings,&recovered,error)) show_error(u,error);
    else if(recovered) notice(u,L"设置文件无效或缺失，已使用默认值；发送和试听保持关闭");
}
static HWND control(Ui *u,int id,const wchar_t *class_name,const wchar_t *text,DWORD style) {
    HWND h=CreateWindowExW(wcscmp(class_name,WC_EDITW)==0?WS_EX_CLIENTEDGE:0,class_name,text,
        WS_CHILD|WS_VISIBLE|style,0,0,0,0,u->hwnd,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    if(id>=0 && id<=UI_STATUS) u->controls[id]=h;
    return h;
}
static void controls_create(Ui *u) {
    static const wchar_t *const label_text[]={
        L"真实麦克风",L"发送到 CABLE Input",L"耳机试听（仅音效）",L"音效音量",L"麦克风音量",
        L"所有全局快捷键默认留空；可填写 Ctrl / Alt / Shift / Win 组合，清空后保存可解除绑定",
        L"每次启动发送与试听均关闭；停止只停止音效，麦克风继续",
        L"停止音效快捷键（可留空）"};
    int i;
    for(i=0;i<CLIP_COUNT;++i) {
        wchar_t *label=win_utf8_to_wide(ui_clip_label(i));
        control(u,UI_PLAY_0+i,WC_BUTTONW,label?label:L"",BS_PUSHBUTTON|BS_MULTILINE|WS_TABSTOP); free(label);
        control(u,UI_HOTKEY_0+i,WC_EDITW,L"",ES_AUTOHSCROLL|WS_TABSTOP);
        u->clip_status[i]=control(u,FIRST_CLIP_STATUS+i,WC_STATICW,L"未设置快捷键",SS_LEFT);
    }
    control(u,UI_STOP,WC_BUTTONW,L"停止音效",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_HOTKEY_0+HOTKEY_STOP_INDEX,WC_EDITW,L"",ES_AUTOHSCROLL|WS_TABSTOP);
    control(u,UI_SAVE_HOTKEYS,WC_BUTTONW,L"保存全局快捷键",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_MIC_DEVICE,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP);
    control(u,UI_SEND_DEVICE,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP);
    control(u,UI_MONITOR_DEVICE,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP);
    control(u,UI_CONNECT,WC_BUTTONW,L"连接所选设备",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_SEND,WC_BUTTONW,L"开启发送",BS_AUTOCHECKBOX|WS_TABSTOP);
    control(u,UI_MONITOR,WC_BUTTONW,L"耳机试听",BS_AUTOCHECKBOX|WS_TABSTOP);
    control(u,UI_MIC_MUTE,WC_BUTTONW,L"麦克风静音",BS_AUTOCHECKBOX|WS_TABSTOP);
    control(u,UI_SOUND_GAIN,TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP);
    control(u,UI_MIC_GAIN,TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP);
    control(u,UI_REFRESH,WC_BUTTONW,L"刷新设备",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_GUIDE,WC_BUTTONW,L"使用教程",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_DETAILS,WC_BUTTONW,L"设备详情",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_DRIVER,WC_BUTTONW,L"安装官方 VB-CABLE",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_VENDOR,WC_BUTTONW,L"VB-Audio 官网",BS_PUSHBUTTON|WS_TABSTOP);
    control(u,UI_STATUS,WC_STATICW,L"",SS_LEFT);
    for(i=0;i<LABEL_COUNT;++i) u->labels[i]=control(u,FIRST_LABEL+i,WC_STATICW,label_text[i],SS_LEFT);
    SendMessageW(u->controls[UI_SOUND_GAIN],TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    SendMessageW(u->controls[UI_MIC_GAIN],TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    SendMessageW(u->controls[UI_SOUND_GAIN],TBM_SETPOS,TRUE,u->settings.sound_gain);
    SendMessageW(u->controls[UI_MIC_GAIN],TBM_SETPOS,TRUE,u->settings.mic_gain);
    SendMessageW(u->controls[UI_MIC_MUTE],BM_SETCHECK,u->settings.mic_muted?BST_CHECKED:BST_UNCHECKED,0);
    for(i=0;i<HOTKEY_COUNT;++i) {
        wchar_t *wide=win_utf8_to_wide(u->settings.hotkeys[i].text);
        SendMessageW(u->controls[UI_HOTKEY_0+i],EM_SETLIMITTEXT,HOTKEY_TEXT_CAP-1,0);
        SendMessageW(u->controls[UI_HOTKEY_0+i],EM_SETCUEBANNER,FALSE,(LPARAM)L"未设置（可留空）");
        SetWindowTextW(u->controls[UI_HOTKEY_0+i],wide?wide:L""); free(wide);
    }
}
static int px(Ui *u,int n) { return MulDiv(n,(int)u->dpi,96); }
static void move(Ui *u,HWND h,int x,int y,int width,int height) { MoveWindow(h,px(u,x),px(u,y-u->scroll),px(u,width),px(u,height),TRUE); }
static void layout(Ui *u) {
    RECT r; UiLayout geometry; int width,height,i;
    GetClientRect(u->hwnd,&r); width=MulDiv(r.right,96,(int)u->dpi); height=MulDiv(r.bottom,96,(int)u->dpi);
    if(!ui_layout_compute(width,&geometry)) return;
    {
        SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,geometry.content_height-1,0,0,0};
        si.nPage=(UINT)height;
        if(u->scroll>geometry.content_height-height) u->scroll=geometry.content_height-height;
        if(u->scroll<0) u->scroll=0;
        si.nPos=u->scroll; SetScrollInfo(u->hwnd,SB_VERT,&si,TRUE);
    }
    for(i=UI_PLAY_0;i<=UI_STATUS;++i) {
        UiRect box=geometry.controls[i];
        if(u->controls[i]) move(u,u->controls[i],box.x,box.y,box.width,box.height);
    }
    for(i=0;i<LABEL_COUNT;++i) {
        UiRect box=geometry.labels[i]; move(u,u->labels[i],box.x,box.y,box.width,box.height);
    }
    for(i=0;i<CLIP_COUNT;++i) {
        UiRect box=geometry.clip_status[i]; move(u,u->clip_status[i],box.x,box.y,box.width,box.height);
    }
    for(i=0;i<3;++i) SendMessageW(u->controls[UI_MIC_DEVICE+i],CB_SETDROPPEDWIDTH,px(u,1200),0);
}
static void fonts(Ui *u) {
    HFONT old=u->font,old_big=u->big_font; int i;
    u->font=CreateFontW(-px(u,16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    u->big_font=CreateFontW(-px(u,21),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(i=UI_PLAY_0;i<=UI_STATUS;++i) if(u->controls[i]) SendMessageW(u->controls[i],WM_SETFONT,(WPARAM)(i<=UI_PLAY_LAST?u->big_font:u->font),TRUE);
    for(i=0;i<LABEL_COUNT;++i) SendMessageW(u->labels[i],WM_SETFONT,(WPARAM)u->font,TRUE);
    for(i=0;i<CLIP_COUNT;++i) SendMessageW(u->clip_status[i],WM_SETFONT,(WPARAM)u->font,TRUE);
    if(old) DeleteObject(old);
    if(old_big) DeleteObject(old_big);
}
void ui_apply_state(HWND hwnd,const ControllerState *state,const HotkeyStatus *keys) {
    Ui *u=get_ui(hwnd); wchar_t text[4096],one[180],stop_text[180]; int i,assigned=0,registered=0; wchar_t *error,*notice_text,*stop_name;
    if(!u || !u->controls[UI_STATUS]) return;
    SendMessageW(u->controls[UI_SEND],BM_SETCHECK,ui_effective_enabled(state->sending,u->last_backend.sending,state->routing_generation==u->last_backend.routing_generation)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(u->controls[UI_MONITOR],BM_SETCHECK,ui_effective_enabled(state->monitoring,u->last_backend.monitoring,state->monitor_generation==u->last_backend.monitor_generation)?BST_CHECKED:BST_UNCHECKED,0);
    SetWindowTextW(u->controls[UI_SEND],state->sending && !u->last_backend.sending?L"发送开启中":L"开启发送");
    SetWindowTextW(u->controls[UI_MONITOR],state->monitoring && !u->last_backend.monitoring?L"试听开启中":L"耳机试听");
    EnableWindow(u->controls[UI_SEND],state->ready && !state->shutting_down);
    EnableWindow(u->controls[UI_CONNECT],!u->closing && u->worker.impl!=NULL);
    for(i=0;i<HOTKEY_COUNT;++i) {
        const Hotkey *key=keys->registered[i]?&keys->keys[i]:&u->settings.hotkeys[i];
        wchar_t *name=win_utf8_to_wide(key->text);
        if(u->settings.hotkeys[i].key) ++assigned;
        if(keys->registered[i]) ++registered;
        if(i<CLIP_COUNT) {
            if(!key->key) swprintf(one,sizeof(one)/sizeof(one[0]),L"未设置快捷键");
            else swprintf(one,sizeof(one)/sizeof(one[0]),L"%ls（%ls）",name?name:L"",keys->registered[i]?L"已注册":L"未注册");
            SetWindowTextW(u->clip_status[i],one);
        }
        free(name);
    }
    stop_name=win_utf8_to_wide(keys->registered[HOTKEY_STOP_INDEX]?keys->keys[HOTKEY_STOP_INDEX].text:u->settings.hotkeys[HOTKEY_STOP_INDEX].text);
    if(!u->settings.hotkeys[HOTKEY_STOP_INDEX].key && !keys->registered[HOTKEY_STOP_INDEX]) swprintf(stop_text,sizeof(stop_text)/sizeof(stop_text[0]),L"停止全局快捷键：未设置");
    else swprintf(stop_text,sizeof(stop_text)/sizeof(stop_text[0]),L"停止全局快捷键：%ls（%ls）",stop_name?stop_name:L"",keys->registered[HOTKEY_STOP_INDEX]?L"已注册":L"未注册");
    free(stop_name);
    error=win_utf8_to_wide(state->last_error); notice_text=win_utf8_to_wide(u->notice);
    swprintf(text,sizeof(text)/sizeof(text[0]),L"连接：%ls  发送：%ls  试听：%ls；全局快捷键 %d/%d 已注册\r\n%ls；Esc 仅本窗口停止音效\r\n%ls%ls%ls",state->ready?L"就绪":L"未连接",ui_effective_enabled(state->sending,u->last_backend.sending,state->routing_generation==u->last_backend.routing_generation)?L"开启":state->sending?L"开启中（再点取消）":L"关闭",ui_effective_enabled(state->monitoring,u->last_backend.monitoring,state->monitor_generation==u->last_backend.monitor_generation)?L"开启":state->monitoring?L"开启中（再点取消）":L"关闭",registered,assigned,stop_text,notice_text?notice_text:L"",u->notice[0] && error && error[0]?L"\r\n":L"",error?error:L"");
    SetWindowTextW(u->controls[UI_STATUS],text); free(error); free(notice_text);
    swprintf(one,sizeof(one)/sizeof(one[0]),L"音效音量 %d%%",u->settings.sound_gain); SetWindowTextW(u->labels[3],one);
    swprintf(one,sizeof(one)/sizeof(one[0]),L"麦克风音量 %d%%",u->settings.mic_gain); SetWindowTextW(u->labels[4],one);
}
static bool device_present(Ui *u,const char *id) { size_t i; for(i=0;i<u->devices.count;++i) if(!strcmp(u->devices.items[i].id,id)) return true; return false; }
static void fill_combo(Ui *u,int control_id,EndpointRole role,const char *selected) {
    HWND combo=u->controls[control_id]; size_t i; LRESULT index; char error[256];
    SendMessageW(combo,CB_RESETCONTENT,0,0);
    for(i=0;i<u->devices.count;++i) {
        DeviceInfo *d=&u->devices.items[i]; EndpointView view={d->id,d->name,d->direction,d->active,d->cable}; wchar_t *name; char *label; bool duplicate=false; size_t j;
        if(!endpoint_validate(&view,role,error)) continue;
        for(j=0;j<u->devices.count;++j) if(j!=i && !strcmp(d->name,u->devices.items[j].name)) { duplicate=true; break; }
        label=endpoint_display_label(d->name,d->id,duplicate); name=win_utf8_to_wide(label?label:d->name); free(label);
        index=SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)(name?name:L"")); free(name);
        if(index>=0) {
            SendMessageW(combo,CB_SETITEMDATA,(WPARAM)index,(LPARAM)i);
            if(!strcmp(d->id,selected)) SendMessageW(combo,CB_SETCURSEL,(WPARAM)index,0);
        }
    }
}
static void refresh_devices(Ui *u) {
    DeviceList next={0}; char error[256]={0}; uint32_t roles=0; Command cmd={0};
    if(!devices_snapshot(&next,error)) { show_error(u,error); return; }
    devices_free(&u->devices); u->devices=next;
    fill_combo(u,UI_MIC_DEVICE,ROLE_MIC,u->settings.mic_id); fill_combo(u,UI_SEND_DEVICE,ROLE_SEND,u->settings.send_id); fill_combo(u,UI_MONITOR_DEVICE,ROLE_MONITOR,u->settings.monitor_id);
    if(u->settings.mic_id[0] && !device_present(u,u->settings.mic_id)) roles|=DEVICE_ROLE_MIC;
    if(u->settings.send_id[0] && !device_present(u,u->settings.send_id)) roles|=DEVICE_ROLE_SEND;
    if(u->settings.monitor_id[0] && !device_present(u,u->settings.monitor_id)) roles|=DEVICE_ROLE_MONITOR;
    if(roles && u->worker.impl) { cmd.kind=COMMAND_DEVICE_CHANGED; cmd.changed_roles=roles; (void)controller_dispatch(&u->controller,&cmd,error); }
    if(roles) notice(u,L"已选设备缺失；未自动改用默认设备。重新选择并主动连接");
    else notice_success(u,L"设备列表已刷新；不会自动开启发送或试听");
}
static bool dispatch(Ui *u,Command *cmd) {
    char error[256]; cmd->settings=u->settings;
    if(!controller_dispatch(&u->controller,cmd,error)) { show_error(u,error); return false; }
    u->notice[0]=0; ui_apply_state(u->hwnd,&u->controller.state,&u->hotkeys); return true;
}
static void selection_changed(Ui *u,int id) {
    LRESULT sel=SendMessageW(u->controls[id],CB_GETCURSEL,0,0),item; char *target; Command cmd={0}; EndpointRole role=ui_role_for_control(id);
    if(sel==CB_ERR) return;
    item=SendMessageW(u->controls[id],CB_GETITEMDATA,(WPARAM)sel,0); if(item<0 || (size_t)item>=u->devices.count) return;
    target=role==ROLE_MIC?u->settings.mic_id:role==ROLE_SEND?u->settings.send_id:u->settings.monitor_id;
    if(!strcmp(target,u->devices.items[item].id)) return;
    snprintf(target,SETTINGS_ENDPOINT_CAP,"%s",u->devices.items[item].id);
    cmd.kind=COMMAND_ROUTE_UPDATE; cmd.changed_roles=(uint32_t)role; dispatch(u,&cmd); defer_save(u);
}
static void update_gains(Ui *u) {
    Command cmd={0}; Settings before=u->settings; bool accepted;
    u->settings.sound_gain=(int)SendMessageW(u->controls[UI_SOUND_GAIN],TBM_GETPOS,0,0);
    u->settings.mic_gain=(int)SendMessageW(u->controls[UI_MIC_GAIN],TBM_GETPOS,0,0);
    u->settings.mic_muted=SendMessageW(u->controls[UI_MIC_MUTE],BM_GETCHECK,0,0)==BST_CHECKED;
    cmd.kind=COMMAND_GAINS; accepted=dispatch(u,&cmd); ui_gain_result(&u->settings,&before,accepted);
    if(!accepted) {
        SendMessageW(u->controls[UI_SOUND_GAIN],TBM_SETPOS,TRUE,u->settings.sound_gain);
        SendMessageW(u->controls[UI_MIC_GAIN],TBM_SETPOS,TRUE,u->settings.mic_gain);
        SendMessageW(u->controls[UI_MIC_MUTE],BM_SETCHECK,u->settings.mic_muted?BST_CHECKED:BST_UNCHECKED,0);
        ui_apply_state(u->hwnd,&u->controller.state,&u->hotkeys);
    } else defer_save(u);
}
static void hotkey_save_error(Ui *u,const char *error) {
    wchar_t *message=win_utf8_to_wide(error);
    show_error(u,error);
    MessageBoxW(u->hwnd,message?message:L"快捷键设置失败，请查看状态说明",L"开麦匣：快捷键设置失败",MB_OK|MB_ICONERROR);
    free(message);
}
static void save_hotkeys(Ui *u) {
    Hotkey next[HOTKEY_COUNT]; HotkeyOps ops={u,reg_key,unreg_key,persist_keys}; char error[256]; wchar_t text[HOTKEY_TEXT_CAP]; int i;
    SetWindowTextW(u->controls[UI_SAVE_HOTKEYS],L"保存全局快捷键");
    for(i=0;i<HOTKEY_COUNT;++i) {
        char *utf8; GetWindowTextW(u->controls[UI_HOTKEY_0+i],text,HOTKEY_TEXT_CAP); utf8=win_wide_to_utf8(text);
        if(!utf8 || !hotkey_parse(utf8,&next[i],error)) { bool had_text=utf8!=NULL; free(utf8); hotkey_save_error(u,had_text?error:"Invalid hotkey text"); return; }
        free(utf8);
    }
    if(!hotkeys_replace(&ops,u->settings.hotkeys,next,&u->hotkeys)) hotkey_save_error(u,u->hotkeys.error);
    else {
        for(i=0;i<HOTKEY_COUNT;++i) { wchar_t *wide=win_utf8_to_wide(u->settings.hotkeys[i].text); SetWindowTextW(u->controls[UI_HOTKEY_0+i],wide?wide:L""); free(wide); }
        SetWindowTextW(u->controls[UI_SAVE_HOTKEYS],L"全局快捷键已保存");
        notice_success(u,L"全局快捷键设置已保存；空白项不注册快捷键");
    }
    ui_apply_state(u->hwnd,&u->controller.state,&u->hotkeys);
}
static bool cable_available(Ui *u) { size_t i; for(i=0;i<u->devices.count;++i) if(u->devices.items[i].active && u->devices.items[i].cable==CABLE_STANDARD_INPUT && u->devices.items[i].direction==DEVICE_RENDER) return true; return false; }
static void official_setup(Ui *u) {
    wchar_t path[32768],root[32768]; SHELLEXECUTEINFOW info={0}; HKEY service=NULL;
    refresh_devices(u);
    if(cable_available(u) || RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\VBAudioVACMME",0,KEY_READ,&service)==ERROR_SUCCESS) {
        if(service) RegCloseKey(service);
        MessageBoxW(u->hwnd,L"检测到标准 CABLE 端点或官方驱动服务记录，建议跳过重复安装。服务记录不保证端点可用；请刷新设备，必要时重启后重新检查。",L"官方 VB-CABLE",MB_OK|MB_ICONINFORMATION); return;
    }
    if(MessageBoxW(u->hwnd,L"将请求 UAC 管理员许可，打开随程序附带的官方 VB-CABLE x64 交互安装器。驱动为 Donationware。取消 UAC 或关闭安装器不会被当作安装成功；安装完成后按官方提示重启，再刷新设备。继续？",L"安装官方 VB-CABLE",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK) return;
    if(!win_root_path(path,sizeof(path)/sizeof(path[0]),L"vendor\\official\\VBCABLE_Setup_x64.exe") || !win_root_path(root,sizeof(root)/sizeof(root[0]),L"vendor\\official")) { notice(u,L"无法定位固定官方安装器路径"); return; }
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES) { notice(u,L"未找到随包官方安装器；请从完整安装包重新安装本程序"); return; }
    info.cbSize=sizeof(info); info.fMask=SEE_MASK_NOCLOSEPROCESS; info.hwnd=u->hwnd; info.lpVerb=L"runas"; info.lpFile=path; info.lpDirectory=root; info.nShow=SW_SHOWNORMAL;
    /* No caller-supplied command, no flags or arguments; exit is not readiness. */
    if(!ShellExecuteExW(&info)) {
        if(GetLastError()==ERROR_CANCELLED) notice(u,L"已取消 UAC；未确认驱动安装。发送和试听仍由你主动开启");
        else notice(u,L"无法启动官方安装器；未确认驱动安装");
    } else { if(info.hProcess) CloseHandle(info.hProcess); notice(u,L"官方安装器已打开；完成后按提示重启并刷新设备。尚未确认驱动可用"); }
}
static void open_guide(Ui *u) {
    wchar_t path[32768];
    if(win_root_path(path,sizeof(path)/sizeof(path[0]),L"docs\\中文使用教程.html")) {
        INT_PTR result=(INT_PTR)ShellExecuteW(u->hwnd,L"open",path,NULL,NULL,SW_SHOWNORMAL);
        if(result<=32) notice(u,L"无法打开教程；可在安装目录 docs 中打开中文使用教程.html");
    }
}
static LRESULT CALLBACK details_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        CREATESTRUCTW *create=(CREATESTRUCTW *)lp;
        HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,WC_EDITW,(const wchar_t *)create->lpCreateParams,
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,
            10,10,600,300,hwnd,(HMENU)1,GetModuleHandleW(NULL),NULL);
        HWND close=CreateWindowExW(0,WC_BUTTONW,L"关闭",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,10,320,100,32,hwnd,(HMENU)IDCANCEL,GetModuleHandleW(NULL),NULL);
        SendMessageW(edit,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
        SendMessageW(close,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE); return 0;
    }
    case WM_SIZE: {
        int width=LOWORD(lp),height=HIWORD(lp);
        MoveWindow(GetDlgItem(hwnd,1),10,10,width-20,height-64,TRUE);
        MoveWindow(GetDlgItem(hwnd,IDCANCEL),width-112,height-44,100,32,TRUE); return 0;
    }
    case WM_COMMAND: if(LOWORD(wp)==IDCANCEL) { DestroyWindow(hwnd); return 0; } break;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void show_device_details(Ui *u) {
    static const wchar_t *const class_name=L"GameVoiceBoxDeviceDetails";
    WNDCLASSW wc={0}; const char *ids[]={u->settings.mic_id,u->settings.send_id,u->settings.monitor_id};
    const char *labels[]={"真实麦克风","发送到 CABLE Input","耳机试听（仅音效）"};
    DeviceInfo *selected[3]={0}; size_t length=2048,i,j,used=0; char *text; wchar_t *wide; HWND dialog;
    for(i=0;i<3;++i) {
        length+=strlen(ids[i]);
        for(j=0;j<u->devices.count;++j) if(!strcmp(ids[i],u->devices.items[j].id)) {
            DeviceInfo *d=&u->devices.items[j]; selected[i]=d;
            length+=strlen(d->name)+(d->description?strlen(d->description):0)+(d->adapter?strlen(d->adapter):0); break;
        }
    }
    text=malloc(length); if(!text) { notice(u,L"无法分配设备详情显示缓冲"); return; }
    for(i=0;i<3;++i) {
        DeviceInfo *d=selected[i];
        used+=(size_t)snprintf(text+used,length-used,"%s\r\n完整名称：%s\r\n完整端点 ID：%s\r\n设备描述：%s\r\n音频适配器：%s\r\n\r\n",labels[i],d?d->name:"未选择或当前不可用",ids[i],d && d->description?d->description:"未知",d && d->adapter?d->adapter:"未知");
    }
    wide=win_utf8_to_wide(text); free(text); if(!wide) return;
    wc.lpfnWndProc=details_proc; wc.hInstance=GetModuleHandleW(NULL); wc.hCursor=LoadCursorW(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.lpszClassName=class_name;
    RegisterClassW(&wc);
    dialog=CreateWindowExW(WS_EX_CONTROLPARENT,class_name,L"设备详情（完整名称和 ID，可横向/纵向滚动）",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,px(u,700),px(u,430),u->hwnd,NULL,GetModuleHandleW(NULL),wide);
    free(wide); if(dialog) ShowWindow(dialog,SW_SHOW);
}
static void control_action(Ui *u,int id) {
    UiAction a; Command cmd={0}; bool checked=SendMessageW(u->controls[id],BM_GETCHECK,0,0)==BST_CHECKED;
    if(id==UI_SEND) checked=ui_toggle_enabled(u->controller.state.sending);
    if(id==UI_MONITOR) checked=ui_toggle_enabled(u->controller.state.monitoring);
    if(ui_action_for_control(id,checked,&a)) {
        cmd.enabled=a.enabled; cmd.clip_index=a.clip_index;
        switch(a.kind) {
        case UI_ACTION_PLAY: cmd.kind=COMMAND_PLAY; break;
        case UI_ACTION_STOP: cmd.kind=COMMAND_STOP; break;
        case UI_ACTION_CONNECT: cmd.kind=COMMAND_CONNECT; break;
        case UI_ACTION_SEND: cmd.kind=COMMAND_SEND; break;
        case UI_ACTION_MONITOR: cmd.kind=COMMAND_MONITOR; break;
        case UI_ACTION_GAINS: update_gains(u); return;
        }
        dispatch(u,&cmd); return;
    }
    if(id==UI_SAVE_HOTKEYS) save_hotkeys(u);
    else if(id==UI_REFRESH) refresh_devices(u);
    else if(id==UI_GUIDE) open_guide(u);
    else if(id==UI_DETAILS) show_device_details(u);
    else if(id==UI_DRIVER) official_setup(u);
    else if(id==UI_VENDOR) ShellExecuteW(u->hwnd,L"open",L"https://vb-audio.com/Cable/index.htm",NULL,NULL,SW_SHOWNORMAL);
}
static void cleanup(Ui *u) {
    int i; char error[256];
    KillTimer(u->hwnd,SETTINGS_TIMER);
    for(i=0;i<HOTKEY_COUNT;++i) if(u->hotkeys.registered[i]) { if(unreg_key(u,HOTKEY_ID_BASE+i,error)) u->hotkeys.registered[i]=false; }
    audio_worker_shutdown(&u->worker);
    devices_free(&u->devices); free(u->settings_path);
    if(u->font) DeleteObject(u->font);
    if(u->big_font) DeleteObject(u->big_font);
    DeleteCriticalSection(&u->status_lock);
}
void ui_focus_visible(HWND hwnd) {
    Ui *u=get_ui(hwnd); HWND focus=GetFocus(); RECT control_rect,client; int delta=0;
    if(!u || !focus || !IsChild(hwnd,focus)) return;
    GetWindowRect(focus,&control_rect); MapWindowPoints(NULL,hwnd,(POINT *)&control_rect,2); GetClientRect(hwnd,&client);
    if(control_rect.top<px(u,8)) delta=control_rect.top-px(u,8);
    else if(control_rect.bottom>client.bottom-px(u,8)) delta=control_rect.bottom-client.bottom+px(u,8);
    if(delta) { u->scroll+=MulDiv(delta,96,(int)u->dpi); layout(u); }
}
LRESULT CALLBACK main_window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    Ui *u=get_ui(hwnd);
    switch(msg) {
    case WM_CREATE: {
        char error[256]; Hotkey defaults[HOTKEY_COUNT]; HotkeyOps ops; BackendOps backend; Command gains={0};
        u=calloc(1,sizeof(*u)); if(!u) return -1;
        u->hwnd=hwnd; u->initializing=true; u->dpi=win_dpi_for_window(hwnd); if(!u->dpi) u->dpi=96;
        InitializeCriticalSection(&u->status_lock); SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)u);
        create_settings_path(u); controls_create(u); fonts(u); layout(u);
        backend=(BackendOps){u,submit}; controller_init(&u->controller,&u->settings,backend);
        if(!audio_worker_start(&u->worker,audio_status,u,error)) { show_error(u,error); EnableWindow(u->controls[UI_CONNECT],FALSE); }
        else { gains.kind=COMMAND_GAINS; gains.settings=u->settings; controller_dispatch(&u->controller,&gains,error); }
        hotkey_defaults(defaults); ops=(HotkeyOps){u,reg_key,unreg_key,NULL};
        if(!hotkeys_replace(&ops,defaults,u->settings.hotkeys,&u->hotkeys)) show_error(u,u->hotkeys.error);
        refresh_devices(u); u->initialized=true; u->initializing=false;
        ui_apply_state(hwnd,&u->controller.state,&u->hotkeys); return 0;
    }
    case WM_AUDIO_STATUS:
        if(u) {
            ControllerState state;
            EnterCriticalSection(&u->status_lock); audio_mailbox_drain(&u->mailbox,&state); LeaveCriticalSection(&u->status_lock);
            u->last_backend=state; controller_apply_backend_state(&u->controller,&state);
            if(!u->closing) ui_apply_state(hwnd,&u->controller.state,&u->hotkeys);
        }
        return 0;
    case WM_COMMAND:
        if(!u || u->closing || u->initializing) return 0;
        if(HIWORD(wp)==EN_CHANGE && LOWORD(wp)>=UI_HOTKEY_0 && LOWORD(wp)<=UI_HOTKEY_LAST) SetWindowTextW(u->controls[UI_SAVE_HOTKEYS],L"保存全局快捷键");
        else if(HIWORD(wp)==CBN_SELCHANGE && ui_role_for_control(LOWORD(wp))) selection_changed(u,LOWORD(wp));
        else if(HIWORD(wp)==BN_CLICKED) control_action(u,LOWORD(wp));
        return 0;
    case WM_HSCROLL: if(u && !u->closing && !u->initializing) update_gains(u); return 0;
    case WM_HOTKEY:
        if(u && !u->closing) {
            Command cmd={0};
            if(wp==HOTKEY_STOP_ID) cmd.kind=COMMAND_STOP;
            else if(wp>=HOTKEY_ID_BASE && wp<HOTKEY_ID_BASE+CLIP_COUNT) { cmd.kind=COMMAND_PLAY; cmd.clip_index=(int)wp-HOTKEY_ID_BASE; }
            else return 0;
            dispatch(u,&cmd);
        }
        return 0;
    case WM_KEYDOWN:
        if(u && !u->closing) { UiAction a; if(ui_action_for_key((unsigned)wp,GetForegroundWindow()==hwnd,&a)) { Command cmd={0}; cmd.kind=COMMAND_STOP; dispatch(u,&cmd); return 0; } }
        break;
    case WM_TIMER: if(u && !u->closing && wp==SETTINGS_TIMER) { KillTimer(hwnd,SETTINGS_TIMER); save_settings(u); } return 0;
    case WM_VSCROLL:
        if(u) {
            SCROLLINFO si={sizeof(si),SIF_ALL,0,0,0,0,0}; int next;
            GetScrollInfo(hwnd,SB_VERT,&si); next=u->scroll;
            switch(LOWORD(wp)) {
            case SB_LINEUP: next-=24; break;
            case SB_LINEDOWN: next+=24; break;
            case SB_PAGEUP: next-=(int)si.nPage; break;
            case SB_PAGEDOWN: next+=(int)si.nPage; break;
            case SB_THUMBTRACK: next=si.nTrackPos; break;
            case SB_TOP: next=0; break;
            case SB_BOTTOM: next=si.nMax; break;
            }
            u->scroll=next; layout(u);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if(u) { u->scroll-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*72; layout(u); }
        return 0;
    case WM_SIZE: if(u) layout(u); return 0;
    case WM_GETMINMAXINFO:
        if(u) {
            MINMAXINFO *info=(MINMAXINFO *)lp; MONITORINFO monitor={0}; monitor.cbSize=sizeof(monitor);
            info->ptMinTrackSize.x=px(u,320); info->ptMinTrackSize.y=px(u,240);
            if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)) {
                int width=monitor.rcWork.right-monitor.rcWork.left,height=monitor.rcWork.bottom-monitor.rcWork.top;
                if(info->ptMinTrackSize.x>width) info->ptMinTrackSize.x=width;
                if(info->ptMinTrackSize.y>height) info->ptMinTrackSize.y=height;
            }
        }
        return 0;
    case WM_DPICHANGED:
        if(u) { RECT *r=(RECT *)lp; u->dpi=HIWORD(wp); fonts(u); SetWindowPos(hwnd,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); layout(u); }
        return 0;
    case WM_CLOSE:
        if(u && (u->initializing || u->closing)) return 0;
        if(u && !u->closing) {
            u->closing=true;
            audio_worker_shutdown(&u->worker);
            if(u->initialized && !save_settings(u) && MessageBoxW(hwnd,L"设置保存失败。继续关闭会丢失本次未保存的修改；取消可保留窗口并查看错误。是否仍要关闭？",L"设置未保存",MB_OKCANCEL|MB_ICONWARNING)!=IDOK) {
                char error[256]; Command gains={0}; BackendOps backend={u,submit};
                u->closing=false; u->initializing=true; memset(&u->last_backend,0,sizeof(u->last_backend));
                EnterCriticalSection(&u->status_lock); audio_mailbox_reset(&u->mailbox); LeaveCriticalSection(&u->status_lock);
                controller_init(&u->controller,&u->settings,backend);
                if(audio_worker_start(&u->worker,audio_status,u,error)) {
                    gains.kind=COMMAND_GAINS; gains.settings=u->settings; controller_dispatch(&u->controller,&gains,error);
                    notice(u,L"已取消退出；发送和试听已安全关闭。需重新连接并主动开启");
                } else show_error(u,error);
                u->initializing=false; ui_apply_state(hwnd,&u->controller.state,&u->hotkeys); return 0;
            }
            cleanup(u);
        }
        if(u) { SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); free(u); }
        DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
