#include "hotkeys.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Fourteen clip bindings and one optional stop binding are one transaction. */
#define EXPECTED_BINDINGS 15

typedef struct {
    bool live[EXPECTED_BINDINGS]; Hotkey keys[EXPECTED_BINDINGS];
    uint32_t conflict_key; uint32_t rollback_conflict;
    bool persist_fail; int fail_unregister_id; int persist_calls,register_calls;
} FakeRegistry;
static bool reg(void *ctx,int id,const Hotkey *key,char error[256]) {
    FakeRegistry *r=ctx; int i=id-HOTKEY_ID_BASE; assert(i>=0 && i<EXPECTED_BINDINGS);
    ++r->register_calls; assert(key->key && (key->modifiers & HOTKEY_NOREPEAT));
    if(r->live[i] || key->key==r->conflict_key || key->key==r->rollback_conflict) {
        strcpy(error,"RegisterHotKey conflict (injected)"); return false;
    }
    r->live[i]=true; r->keys[i]=*key; return true;
}
static bool unreg(void *ctx,int id,char error[256]) {
    FakeRegistry *r=ctx; int i=id-HOTKEY_ID_BASE; assert(i>=0 && i<EXPECTED_BINDINGS);
    if(id==r->fail_unregister_id) { strcpy(error,"UnregisterHotKey failure (injected)"); return false; }
    assert(r->live[i]); r->live[i]=false; return true;
}
static bool persist(void *ctx,const Hotkey next[EXPECTED_BINDINGS],char error[256]) {
    FakeRegistry *r=ctx; (void)next; ++r->persist_calls;
    if(r->persist_fail) { strcpy(error,"Atomic settings save failure (injected)"); return false; }
    return true;
}
static void actual_matches(const FakeRegistry *r,const HotkeyStatus *s) {
    for(int i=0;i<HOTKEY_COUNT;++i) {
        assert(r->live[i]==s->registered[i]);
        if(r->live[i]) assert(hotkey_equal(&r->keys[i],&s->keys[i]));
    }
}
static void all_defaults_unassigned(void) {
    Hotkey keys[EXPECTED_BINDINGS]={{0}}; FakeRegistry r={0}; HotkeyStatus actual={0};
    HotkeyOps ops={&r,reg,unreg,persist}; char error[256];
    hotkey_defaults(keys);
    for(int i=0;i<EXPECTED_BINDINGS;++i) assert(!keys[i].key && !keys[i].modifiers && !keys[i].text[0]);
    assert(hotkeys_validate(keys,error));
    assert(hotkeys_replace(&ops,keys,keys,&actual));
    assert(r.register_calls==0 && r.persist_calls==1); actual_matches(&r,&actual);
    assert(HOTKEY_COUNT==EXPECTED_BINDINGS && HOTKEY_STOP_ID==HOTKEY_ID_BASE+14);
}
static void parsing(void) {
    Hotkey h,keys[EXPECTED_BINDINGS]; char error[256]; hotkey_defaults(keys);
    assert(hotkey_parse("",&h,error) && !h.key && !h.modifiers && !h.text[0]);
    assert(hotkey_parse("   \t",&h,error) && !h.key && !h.modifiers && !h.text[0]);
    assert(hotkey_parse(" shift + CTRL + a ",&h,error));
    assert(!strcmp(h.text,"Ctrl+Shift+A") && h.key=='A');
    assert(hotkey_parse("Win+F11",&h,error));
    assert(hotkey_parse("Ctrl+Alt+F9",&h,error));
    const char *invalid[]={"Esc","Escape","F0","F25","F12","Ctrl+F12","Ctrl++A","A+","+A","Ctrl+Ctrl+A","Ctrl","Alt+Tab","Ctrl+Alt+Delete","Win+L","Unknown+A","é"};
    for(size_t i=0;i<sizeof invalid/sizeof *invalid;++i) { assert(!hotkey_parse(invalid[i],&h,error)); assert(error[0]); }
    assert(hotkeys_validate(keys,error));
    Hotkey malformed[EXPECTED_BINDINGS]; memcpy(malformed,keys,sizeof keys);
    malformed[13].modifiers=HOTKEY_CTRL; assert(!hotkeys_validate(malformed,error));
    memcpy(malformed,keys,sizeof keys); strcpy(malformed[13].text,"F1"); assert(!hotkeys_validate(malformed,error));
    assert(hotkey_parse("Shift+Escape",&h,error)); assert(hotkey_parse("Ctrl+Shift+Escape",&h,error));
    assert(hotkey_parse("F1",&keys[0],error)); keys[14]=keys[0]; assert(!hotkeys_validate(keys,error));
}
static void transactions(void) {
    FakeRegistry r={0}; HotkeyStatus actual={0}; Hotkey old[EXPECTED_BINDINGS],next[EXPECTED_BINDINGS]; char error[256];
    HotkeyOps ops={&r,reg,unreg,persist}; hotkey_defaults(old);
    assert(hotkey_parse("F1",&old[0],error)); assert(hotkey_parse("F2",&old[13],error));
    assert(hotkey_parse("Ctrl+Alt+F9",&old[14],error));
    assert(hotkeys_replace(&ops,old,old,&actual)); actual_matches(&r,&actual); assert(r.register_calls==3);
    memcpy(next,old,sizeof next); assert(hotkey_parse("F3",&next[0],error));
    assert(hotkey_parse("F4",&next[13],error)); assert(hotkey_parse("Ctrl+S",&next[14],error));
    r.conflict_key=next[14].key;
    assert(!hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual);
    assert(actual.error[0] && r.persist_calls==1);
    assert(actual.registered[0] && actual.registered[13] && actual.registered[14]);
    assert(hotkey_equal(&actual.keys[14],&old[14]));
    r.rollback_conflict=old[0].key;
    assert(!hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual);
    assert(!actual.registered[0] && actual.registered[13] && actual.registered[14]); assert(strstr(actual.error,"rollback"));
    r.conflict_key=r.rollback_conflict=0;
    assert(hotkeys_replace(&ops,old,old,&actual)); actual_matches(&r,&actual);
    r.persist_fail=true;
    assert(!hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual); assert(strstr(actual.error,"save"));
    assert(actual.registered[14] && hotkey_equal(&actual.keys[14],&old[14]));
    r.persist_fail=false; r.fail_unregister_id=HOTKEY_ID_BASE+14;
    assert(!hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual);
    assert(actual.registered[0] && hotkey_equal(&actual.keys[0],&old[0]));
    r.fail_unregister_id=0;
    assert(hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual);
    Hotkey duplicate[EXPECTED_BINDINGS]; memcpy(duplicate,next,sizeof next); duplicate[14]=next[13];
    int persisted=r.persist_calls;
    assert(!hotkeys_replace(&ops,next,duplicate,&actual)); actual_matches(&r,&actual); assert(r.persist_calls==persisted);
    /* A persistence failure while clearing stop restores its registration. */
    Hotkey clear_stop[EXPECTED_BINDINGS]; memcpy(clear_stop,next,sizeof next);
    assert(hotkey_parse("",&clear_stop[14],error)); r.persist_fail=true;
    assert(!hotkeys_replace(&ops,next,clear_stop,&actual)); actual_matches(&r,&actual);
    assert(actual.registered[14] && hotkey_equal(&actual.keys[14],&next[14])); r.persist_fail=false;
    /* Clearing the stop and last clip actually unregisters them and persists. */
    assert(hotkey_parse("",&next[13],error)); assert(hotkey_parse("",&next[14],error));
    assert(hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual);
    assert(actual.registered[0] && !actual.registered[13] && !actual.registered[14]);
    hotkey_defaults(next); assert(hotkeys_replace(&ops,old,next,&actual)); actual_matches(&r,&actual);
    for(int i=0;i<EXPECTED_BINDINGS;++i) assert(!actual.registered[i] && !actual.keys[i].key);
}
int main(void) {
    all_defaults_unassigned(); parsing(); transactions();
    puts("hotkeys: 15 optional bindings, no default registration, stop/clip clearing, reserved keys and atomic rollback passed");
    return 0;
}
