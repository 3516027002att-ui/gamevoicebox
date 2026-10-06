/* Runs the actual portable production diagnostics, not Win32 delivery/audio. */
#include "../src/windows/hotkey_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Hotkey key(void) {
    Hotkey k={0}; k.key='Z'; memcpy(k.text,"Z",2); return k;
}
static void configured_keys_only(void) {
    HotkeyDiagnostics d={0},before=d; Hotkey z=key(),blank={0};
    assert(!hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE-1,&z,true,8,1,2));
    assert(!hotkey_diagnostics_queue(&d,HOTKEY_STOP_ID+1,&z,true,8,1,2));
    assert(!hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE,&z,false,8,1,2));
    assert(!hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE,&blank,true,8,1,2));
    assert(!hotkey_diagnostics_handled(&d,HOTKEY_ID_BASE,&z,false,8,1,2));
    assert(!hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,true,NULL));
    assert(!memcmp(&d,&before,sizeof d));
}
static void queue_and_dispatch_boundaries(void) {
    HotkeyDiagnostics d={0}; Hotkey z=key(); char out[1536],retained[1536];
    hotkey_diagnostics_format(&d,out,sizeof out);
    assert(strstr(out,"尚未收到") && strstr(out,"接收 0 / 处理 0 / 提交 0 / 拒绝 0"));
    assert(hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE,&z,true,8,1,2));
    assert(d.queue_received==1 && d.wndproc_handled==0 && d.command_accepted==0);
    assert(d.latest_id==HOTKEY_ID_BASE && !strcmp(d.latest_key,"Z"));
    assert(d.result==HOTKEY_DIAGNOSTIC_QUEUED);
    hotkey_diagnostics_format(&d,out,sizeof out);
    assert(strstr(out,"Z 08:01:02") && strstr(out,"等待窗口处理"));
    assert(!hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,true,NULL));
    Hotkey changed=z; changed.key='X'; memcpy(changed.text,"X",2);
    assert(hotkey_diagnostics_handled(&d,HOTKEY_ID_BASE,&changed,true,8,1,3));
    assert(!strcmp(d.latest_key,"Z")); /* name is the binding captured at queue time */
    assert(d.queue_received==1 && d.wndproc_handled==1 && d.result==HOTKEY_DIAGNOSTIC_HANDLED);
    assert(d.second==2); /* initial queue timestamp retained */
    assert(!hotkey_diagnostics_result(&d,HOTKEY_STOP_ID,false,"wrong command"));
    assert(hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,true,NULL));
    assert(d.command_accepted==1 && d.command_rejected==0 && !d.error[0]);
    hotkey_diagnostics_format(&d,out,sizeof out);
    assert(strstr(out,"已提交（不代表已出声）"));
    assert(strstr(out,"接收 1 / 处理 1 / 提交 1 / 拒绝 0"));
    memcpy(retained,out,sizeof out);
    assert(!hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,false,"duplicate result"));
    hotkey_diagnostics_format(&d,out,sizeof out);
    assert(!strcmp(out,retained)); /* unrelated status rendering never clears the event */
    assert(hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE,&z,true,8,2,0));
    assert(hotkey_diagnostics_handled(&d,HOTKEY_ID_BASE,&z,true,8,2,0));
    assert(hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,false,"Enable send or headphone preview before playing a sound"));
    assert(d.queue_received==2 && d.wndproc_handled==2 && d.command_accepted==1 && d.command_rejected==1);
    hotkey_diagnostics_format(&d,out,sizeof out);
    assert(strstr(out,"被拒绝") && strstr(out,"拒绝原因：Enable send or headphone preview"));
    assert(!strstr(out,"不代表已出声"));
}
static void direct_handling_and_bounded_state(void) {
    HotkeyDiagnostics d={0}; Hotkey z=key(); char long_error[600],out[1536],small[9];
    memset(long_error,'e',sizeof long_error-1); long_error[sizeof long_error-1]=0;
    assert(hotkey_diagnostics_handled(&d,HOTKEY_STOP_ID,&z,true,23,59,58));
    assert(d.queue_received==0 && d.wndproc_handled==1 && d.hour==23 && d.second==58);
    assert(hotkey_diagnostics_result(&d,HOTKEY_STOP_ID,false,long_error));
    assert(strlen(d.error)==sizeof d.error-1 && d.error[sizeof d.error-1]==0);
    assert(hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE,&z,true,0,0,0));
    assert(!d.error[0] && d.result==HOTKEY_DIAGNOSTIC_QUEUED);
    hotkey_diagnostics_format(&d,out,sizeof out); assert(strstr(out,"00:00:00"));
    memset(small,'x',sizeof small); hotkey_diagnostics_format(&d,small,sizeof small);
    assert(small[sizeof small-1]==0);
    hotkey_diagnostics_format(&d,NULL,0);
    HotkeyDiagnostics fresh={0}; hotkey_diagnostics_format(&fresh,out,sizeof out);
    assert(!fresh.has_event && strstr(out,"接收 0 / 处理 0 / 提交 0 / 拒绝 0"));
}
static void counters_saturate(void) {
    HotkeyDiagnostics d={0}; Hotkey z=key();
    d.queue_received=d.wndproc_handled=d.command_accepted=d.command_rejected=UINT64_MAX;
    assert(hotkey_diagnostics_queue(&d,HOTKEY_ID_BASE,&z,true,8,0,0));
    assert(hotkey_diagnostics_handled(&d,HOTKEY_ID_BASE,&z,true,8,0,0));
    assert(hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,true,NULL));
    assert(d.queue_received==UINT64_MAX && d.wndproc_handled==UINT64_MAX && d.command_accepted==UINT64_MAX);
    assert(hotkey_diagnostics_handled(&d,HOTKEY_ID_BASE,&z,true,8,0,0));
    assert(hotkey_diagnostics_result(&d,HOTKEY_ID_BASE,false,NULL));
    assert(d.command_rejected==UINT64_MAX && !strcmp(d.error,"未提供拒绝原因"));
}
int main(void) {
    configured_keys_only(); queue_and_dispatch_boundaries(); direct_handling_and_bounded_state(); counters_saturate();
    puts("Hotkey diagnostics: configured IDs, delivery stages, bounded latest result and counters passed (NOT Win32/game/audio runtime)");
    return 0;
}
