#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *path = "build/tests/settings-fixture.ini";
static void write_bytes(const char *data, size_t len) {
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fwrite(data, 1, len, f) == len); assert(fclose(f) == 0);
}
static void read_bytes(char *data, size_t cap) {
    FILE *f = fopen(path, "rb"); size_t n; assert(f);
    n = fread(data, 1, cap - 1, f); data[n] = '\0'; assert(fclose(f) == 0);
}
static bool inject_failure(void *ctx, SettingsFileOperation op) {
    return op != *(SettingsFileOperation *)ctx;
}
static void defaults_and_parse(void) {
    Settings s; char error[256];
    settings_defaults(&s);
    assert(s.version == 1 && s.sound_gain == 80 && s.mic_gain == 100);
    assert(!s.mic_muted && !s.sending && !s.monitoring);
    for (int i=0;i<HOTKEY_COUNT;++i) assert(!s.hotkeys[i].key && !s.hotkeys[i].text[0]);
    assert(settings_validate(&s, error));
    const char *ini = "[settings]\nversion=1\nhotkey1=alt+shift+k\nhotkey2=F7\nhotkey3=F8\nsoundGain=0\nmicGain=99\nmicMuted=1\nmicId={设备}=a\\b\nsendId=opaque render\nmonitorId=headphones\n";
    assert(settings_parse(ini, strlen(ini), &s, error));
    assert(s.sound_gain == 0 && s.mic_gain == 99 && s.mic_muted);
    assert(!strcmp(s.hotkeys[0].text, "Alt+Shift+K"));
    assert(!strcmp(s.mic_id, "{设备}=a\\b"));
    assert(!strcmp(s.send_id, "opaque render"));
    assert(!s.sending && !s.monitoring && !error[0]);
    assert(settings_parse("[settings]\nversion=1\n", 21, &s, error));
    assert(s.sound_gain == 80);
    for(int i=0;i<HOTKEY_COUNT;++i) assert(!s.hotkeys[i].key);
}
static void all_bindings_roundtrip(void) {
    Settings s,loaded; char error[256],key[16],bytes[20000]; bool recovered;
    settings_defaults(&s);
    for(int i=0;i<14;++i) { snprintf(key,sizeof key,"Ctrl+%c",'A'+i); assert(hotkey_parse(key,&s.hotkeys[i],error)); }
    assert(hotkey_parse("Ctrl+Alt+F9",&s.hotkeys[14],error));
    assert(settings_save_atomic(path,&s,error)); assert(settings_load(path,&loaded,&recovered,error) && !recovered);
    for(int i=0;i<15;++i) assert(hotkey_equal(&s.hotkeys[i],&loaded.hotkeys[i]));
    read_bytes(bytes,sizeof bytes); assert(strstr(bytes,"hotkey4=Ctrl+D\n") && strstr(bytes,"hotkey14=Ctrl+N\n"));
    remove(path);
}
static void optional_stop_and_fourteen_roundtrip(void) {
    Settings s, loaded; char error[256], bytes[20000]; bool recovered;
    const char *ini="[settings]\nversion=1\nhotkey1=\nhotkey14=Ctrl+Z\nstopHotkey=Ctrl+Alt+F9\n";
    assert(settings_parse(ini,strlen(ini),&s,error));
    assert(!s.hotkeys[0].key && !strcmp(s.hotkeys[13].text,"Ctrl+Z") && !strcmp(s.hotkeys[14].text,"Ctrl+Alt+F9"));
    assert(settings_save_atomic(path,&s,error)); read_bytes(bytes,sizeof bytes);
    assert(strstr(bytes,"hotkey14=Ctrl+Z\n") && strstr(bytes,"stopHotkey=Ctrl+Alt+F9\n"));
    assert(settings_load(path,&loaded,&recovered,error) && !recovered);
    for(int i=0;i<15;++i) assert(hotkey_equal(&s.hotkeys[i],&loaded.hotkeys[i]));
    assert(hotkey_parse("",&s.hotkeys[13],error)); assert(hotkey_parse("",&s.hotkeys[14],error));
    assert(settings_save_atomic(path,&s,error));
    assert(settings_load(path,&loaded,&recovered,error) && !recovered);
    assert(!loaded.hotkeys[13].key && !loaded.hotkeys[14].key);
    const char *bad[]={"[settings]\nversion=1\nhotkey15=F1\n","[settings]\nversion=1\nhotkey014=F1\n","[settings]\nversion=1\nhotkey14=F1\nhotkey14=F2\n","[settings]\nversion=1\nstopHotkey=F1\nstopHotkey=F2\n","[settings]\nversion=1\nhotkey14=F1\nstopHotkey=F1\n"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;++i) assert(!settings_parse(bad[i],strlen(bad[i]),&loaded,error));
    remove(path);
}
static void malformed_defaults(void) {
    const char *invalid[] = {
        "", "garbage", "[settings]\nversion=2\n", "[settings]\nsoundGain=1\n",
        "[settings]\nversion=1\nsoundGain=101\n", "[settings]\nversion=1\nmicGain=-1\n",
        "[settings]\nversion=1\nmicGain=7x\n", "[settings]\nversion=1\nmicMuted=true\n",
        "[settings]\nversion=1\nunknown=value\n", "[settings]\nversion=1\nsending=1\n",
        "[settings]\nversion=1\nhotkey1=F12\n", "[settings]\nversion=1\nhotkey1=F7\nhotkey2=F7\n",
        "[settings]\nversion=1\nversion=1\n", "[other]\nversion=1\n",
        "[settings]\nversion=1\nmicId=\xc0\xaf\n",
        "[settings]\nversion=1\nmicId=\xed\xa0\x80\n",
        "[settings]\nversion=1\nmicId=\xf4\x90\x80\x80\n"
    };
    Settings s; char error[256];
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        assert(!settings_parse(invalid[i], strlen(invalid[i]), &s, error));
        assert(error[0] && s.sound_gain == 80 && !s.sending && !s.monitoring);
    }
    char *large = malloc(SETTINGS_MAX_BYTES + 2); assert(large);
    memset(large, 'x', SETTINGS_MAX_BYTES + 1);
    assert(!settings_parse(large, SETTINGS_MAX_BYTES + 1, &s, error));
    free(large);
    char *long_id = malloc(SETTINGS_ENDPOINT_MAX + 100); assert(long_id);
    strcpy(long_id, "[settings]\nversion=1\nmicId=");
    size_t prefix = strlen(long_id);
    memset(long_id + prefix, 'a', SETTINGS_ENDPOINT_MAX + 1);
    long_id[prefix + SETTINGS_ENDPOINT_MAX + 1] = '\0';
    assert(!settings_parse(long_id, strlen(long_id), &s, error)); free(long_id);
    const char embedded[] = "[settings]\nversion=1\nmicId=a\0b\n";
    assert(!settings_parse(embedded, sizeof embedded - 1, &s, error));
    settings_defaults(&s); s.mic_id[0] = '\n'; s.mic_id[1] = 0;
    assert(!settings_validate(&s, error));
}
static void inclusive_bounds_and_opaque_ids(void) {
    Settings s, parsed; char error[256];
    settings_defaults(&s);
    memset(s.mic_id, 'a', SETTINGS_ENDPOINT_MAX); s.mic_id[SETTINGS_ENDPOINT_MAX] = 0;
    strcpy(s.send_id, "  opaque=ID\\{render}  ");
    strcpy(s.monitor_id, "😀/耳机");
    assert(settings_validate(&s, error));
    assert(settings_save_atomic(path, &s, error));
    bool recovered; assert(settings_load(path, &parsed, &recovered, error)); assert(!recovered);
    assert(strlen(parsed.mic_id) == SETTINGS_ENDPOINT_MAX);
    assert(!strcmp(parsed.send_id, s.send_id) && !strcmp(parsed.monitor_id, s.monitor_id));
    char *ini = malloc(SETTINGS_MAX_BYTES); assert(ini);
    const char *prefix = "\xef\xbb\xbf[settings]\r\nversion=1\r\n;";
    size_t n = strlen(prefix); memcpy(ini, prefix, n);
    memset(ini + n, 'c', SETTINGS_MAX_BYTES - n);
    assert(settings_parse(ini, SETTINGS_MAX_BYTES, &parsed, error));
    assert(parsed.sound_gain == 80); free(ini);
    unsigned random = 1;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        char bytes[128];
        for (size_t i = 0; i < sizeof bytes; ++i) {
            random = random * 1664525u + 1013904223u;
            bytes[i] = (char)(random >> 24);
        }
        bool valid = settings_parse(bytes, sizeof bytes, &parsed, error);
        assert(settings_validate(&parsed, error));
        assert(!parsed.sending && !parsed.monitoring);
        if (!valid) assert(parsed.sound_gain == 80 && parsed.mic_gain == 100);
    }
    remove(path);
}
static void load_recovery_and_atomic(void) {
    Settings s, loaded; bool recovered; char error[256], before[20000], after[20000];
    remove(path);
    assert(settings_load(path, &loaded, &recovered, error));
    assert(recovered && !loaded.sending && !loaded.monitoring && error[0]);
    write_bytes("broken", 6);
    assert(settings_load(path, &loaded, &recovered, error)); assert(recovered && error[0]);
    settings_defaults(&s); strcpy(s.mic_id, "端点/{capture}");
    strcpy(s.send_id, "opaque={render}"); strcpy(s.monitor_id, "耳机");
    s.sound_gain = 42; s.mic_muted = true; s.sending = true; s.monitoring = true;
    assert(settings_save_atomic(path, &s, error)); assert(!error[0]);
    assert(settings_load(path, &loaded, &recovered, error));
    assert(!recovered && !loaded.sending && !loaded.monitoring);
    assert(loaded.sound_gain == 42 && loaded.mic_muted);
    assert(!strcmp(loaded.mic_id, s.mic_id) && !strcmp(loaded.send_id, s.send_id));
    read_bytes(before, sizeof before);
    assert(!strstr(before, "sending=") && !strstr(before, "monitoring="));
    s.sound_gain = 73;
    for (int i = SETTINGS_FILE_OPEN; i <= SETTINGS_FILE_REPLACE; ++i) {
        SettingsFileOperation fault = (SettingsFileOperation)i;
        SettingsFileOps ops = { &fault, inject_failure };
        assert(!settings_save_atomic_with_ops(path, &s, &ops, error));
        assert(error[0]); read_bytes(after, sizeof after); assert(!strcmp(before, after));
    }
    Settings bad = s; bad.mic_gain = 200;
    assert(!settings_save_atomic(path, &bad, error));
    read_bytes(after, sizeof after); assert(!strcmp(before, after));
    assert(settings_save_atomic(path, &s, error));
    assert(settings_load(path, &loaded, &recovered, error)); assert(loaded.sound_gain == 73);
    char *oversized = malloc(SETTINGS_MAX_BYTES + 1); assert(oversized);
    memset(oversized, 'x', SETTINGS_MAX_BYTES + 1); write_bytes(oversized, SETTINGS_MAX_BYTES + 1);
    free(oversized); assert(settings_load(path, &loaded, &recovered, error)); assert(recovered);
    assert(!settings_load("build/tests", &loaded, &recovered, error));
    remove(path);
}
int main(void) {
    defaults_and_parse(); all_bindings_roundtrip(); optional_stop_and_fourteen_roundtrip(); malformed_defaults(); inclusive_bounds_and_opaque_ids(); load_recovery_and_atomic();
    puts("settings: bounded UTF-8 parse/recovery and all 5 atomic-save failure gates passed");
    return 0;
}
