#include "hotkey_diagnostics.h"
#include <inttypes.h>
#include <stdio.h>
static void increment(uint64_t *count) { if(*count<UINT64_MAX) ++*count; }
static bool configured(int id,const Hotkey *key,bool registered) {
    return id>=HOTKEY_ID_BASE && id<=HOTKEY_STOP_ID && registered && key && key->key && key->text[0];
}
static void latest(HotkeyDiagnostics *d,int id,const Hotkey *key,unsigned hour,unsigned minute,unsigned second) {
    d->has_event=true; d->latest_id=id;
    snprintf(d->latest_key,sizeof d->latest_key,"%.*s",HOTKEY_TEXT_CAP-1,key->text);
    d->hour=hour; d->minute=minute; d->second=second; d->error[0]=0;
}
bool hotkey_diagnostics_queue(HotkeyDiagnostics *d,int id,const Hotkey *key,bool registered,unsigned hour,unsigned minute,unsigned second) {
    if(!d || !configured(id,key,registered)) return false;
    increment(&d->queue_received); latest(d,id,key,hour,minute,second); d->result=HOTKEY_DIAGNOSTIC_QUEUED;
    return true;
}
bool hotkey_diagnostics_handled(HotkeyDiagnostics *d,int id,const Hotkey *key,bool registered,unsigned hour,unsigned minute,unsigned second) {
    if(!d || !configured(id,key,registered)) return false;
    increment(&d->wndproc_handled);
    /* A normal queued event keeps its binding name and initial local time.
       Direct WndProc delivery is visible too, without inventing queue receipt. */
    if(!d->has_event || d->latest_id!=id || d->result!=HOTKEY_DIAGNOSTIC_QUEUED) latest(d,id,key,hour,minute,second);
    d->result=HOTKEY_DIAGNOSTIC_HANDLED; d->error[0]=0;
    return true;
}
bool hotkey_diagnostics_result(HotkeyDiagnostics *d,int id,bool accepted,const char *error) {
    if(!d || !d->has_event || d->latest_id!=id || d->result!=HOTKEY_DIAGNOSTIC_HANDLED) return false;
    if(accepted) { increment(&d->command_accepted); d->result=HOTKEY_DIAGNOSTIC_ACCEPTED; d->error[0]=0; }
    else {
        increment(&d->command_rejected); d->result=HOTKEY_DIAGNOSTIC_REJECTED;
        snprintf(d->error,sizeof d->error,"%s",error && error[0]?error:"未提供拒绝原因");
    }
    return true;
}
void hotkey_diagnostics_format(const HotkeyDiagnostics *d,char *out,size_t capacity) {
    const char *result;
    if(!out || !capacity) return;
    out[0]=0; if(!d) return;
    if(!d->has_event) {
        snprintf(out,capacity,"热键诊断：尚未收到已注册全局键\r\n接收 %" PRIu64 " / 处理 %" PRIu64 " / 提交 %" PRIu64 " / 拒绝 %" PRIu64,d->queue_received,d->wndproc_handled,d->command_accepted,d->command_rejected);
        return;
    }
    result=d->result==HOTKEY_DIAGNOSTIC_QUEUED?"已接收，等待窗口处理":
           d->result==HOTKEY_DIAGNOSTIC_HANDLED?"已处理，等待提交结果":
           d->result==HOTKEY_DIAGNOSTIC_ACCEPTED?"已提交（不代表已出声）":"被拒绝";
    snprintf(out,capacity,"热键诊断：%s %02u:%02u:%02u · %s\r\n接收 %" PRIu64 " / 处理 %" PRIu64 " / 提交 %" PRIu64 " / 拒绝 %" PRIu64 "%s%s",d->latest_key,d->hour,d->minute,d->second,result,d->queue_received,d->wndproc_handled,d->command_accepted,d->command_rejected,d->result==HOTKEY_DIAGNOSTIC_REJECTED?"\r\n拒绝原因：":"",d->result==HOTKEY_DIAGNOSTIC_REJECTED?d->error:"");
}
