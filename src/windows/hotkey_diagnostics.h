#ifndef RONGYU_HOTKEY_DIAGNOSTICS_H
#define RONGYU_HOTKEY_DIAGNOSTICS_H
#include <stddef.h>
#include "../app/hotkeys.h"
/* UI-thread only; fixed-size latest event and saturating session counters.
   No keyboard hook, typed-content collection, persistence or transmission. */
typedef enum {
    HOTKEY_DIAGNOSTIC_NONE, HOTKEY_DIAGNOSTIC_QUEUED, HOTKEY_DIAGNOSTIC_HANDLED,
    HOTKEY_DIAGNOSTIC_ACCEPTED, HOTKEY_DIAGNOSTIC_REJECTED
} HotkeyDiagnosticResult;
typedef struct {
    uint64_t queue_received,wndproc_handled,command_accepted,command_rejected;
    bool has_event;
    int latest_id;
    char latest_key[HOTKEY_TEXT_CAP];
    unsigned hour,minute,second;
    HotkeyDiagnosticResult result;
    char error[256];
} HotkeyDiagnostics;
bool hotkey_diagnostics_queue(HotkeyDiagnostics *d,int id,const Hotkey *key,bool registered,unsigned hour,unsigned minute,unsigned second);
bool hotkey_diagnostics_handled(HotkeyDiagnostics *d,int id,const Hotkey *key,bool registered,unsigned hour,unsigned minute,unsigned second);
bool hotkey_diagnostics_result(HotkeyDiagnostics *d,int id,bool accepted,const char *error);
void hotkey_diagnostics_format(const HotkeyDiagnostics *d,char *out,size_t capacity);
#endif
