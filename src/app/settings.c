#define _POSIX_C_SOURCE 200809L
#include "settings.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static void fail(char error[256], const char *message) {
    if (error) snprintf(error, 256, "%s", message);
}
static bool utf8_valid(const unsigned char *s, size_t len, bool endpoint) {
    size_t i = 0;
    while (i < len) {
        uint32_t cp; unsigned extra; unsigned char c = s[i++];
        if (c < 0x80u) {
            if (!c || (endpoint && (c < 0x20u || c == 0x7fu))) return false;
            continue;
        }
        if (c >= 0xc2u && c <= 0xdfu) { cp = c & 0x1fu; extra = 1; }
        else if (c >= 0xe0u && c <= 0xefu) { cp = c & 0x0fu; extra = 2; }
        else if (c >= 0xf0u && c <= 0xf4u) { cp = c & 0x07u; extra = 3; }
        else return false;
        if (extra > len - i) return false;
        for (unsigned j = 0; j < extra; ++j) {
            c = s[i++]; if ((c & 0xc0u) != 0x80u) return false;
            cp = (cp << 6) | (c & 0x3fu);
        }
        if ((extra == 1 && cp < 0x80u) || (extra == 2 && cp < 0x800u) ||
            (extra == 3 && cp < 0x10000u) || cp > 0x10ffffu ||
            (cp >= 0xd800u && cp <= 0xdfffu)) return false;
    }
    return true;
}
static bool endpoint_valid(const char s[SETTINGS_ENDPOINT_CAP]) {
    const char *end = memchr(s, 0, SETTINGS_ENDPOINT_CAP);
    if (!end) return false;
    return utf8_valid((const unsigned char *)s, (size_t)(end - s), true);
}
void settings_defaults(Settings *out) {
    if (!out) return;
    memset(out, 0, sizeof *out); out->version = SETTINGS_VERSION;
    out->sound_gain = 80; out->mic_gain = 100; hotkey_defaults(out->hotkeys);
}
bool settings_validate(const Settings *s, char error[256]) {
    if (error) error[0] = 0;
    if (!s || s->version != SETTINGS_VERSION) { fail(error, "Unsupported settings version"); return false; }
    if (s->sound_gain < 0 || s->sound_gain > 100 || s->mic_gain < 0 || s->mic_gain > 100) {
        fail(error, "Sound and microphone gains must be integers from 0 to 100"); return false;
    }
    if (!endpoint_valid(s->mic_id) || !endpoint_valid(s->send_id) || !endpoint_valid(s->monitor_id)) {
        fail(error, "Endpoint IDs must be valid UTF-8, at most 4096 bytes, without line breaks or controls"); return false;
    }
    return hotkeys_validate(s->hotkeys, error);
}
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) ++s;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    return s;
}
static bool parse_integer(const char *value, unsigned limit, int *out) {
    if (!*value) return false;
    unsigned v = 0;
    while (*value) {
        if (!isdigit((unsigned char)*value)) return false;
        unsigned digit = (unsigned)(*value++ - '0');
        if (digit > limit || v > (limit - digit) / 10u) return false;
        v = v * 10u + digit;
    }
    *out = (int)v; return true;
}
bool settings_parse(const char *data, size_t len, Settings *out, char error[256]) {
    if (error) error[0] = 0;
    if (!out) { fail(error, "Missing settings output"); return false; }
    settings_defaults(out);
    if (!data || !len || len > SETTINGS_MAX_BYTES || !utf8_valid((const unsigned char *)data, len, false)) {
        fail(error, "Settings are empty, invalid UTF-8, or exceed 64 KiB; defaults restored"); return false;
    }
    char *copy = malloc(len + 1); if (!copy) { fail(error, "Unable to allocate settings buffer"); return false; }
    memcpy(copy, data, len); copy[len] = 0;
    Settings candidate; settings_defaults(&candidate); unsigned seen = 0; bool key_seen[HOTKEY_COUNT] = {false}; bool section = false, ok = true;
    char local_error[256] = "Malformed settings; defaults restored";
    char *line = copy;
    if (len >= 3 && (unsigned char)line[0] == 0xefu && (unsigned char)line[1] == 0xbbu && (unsigned char)line[2] == 0xbfu) line += 3;
    while (*line && ok) {
        char *next = strchr(line, '\n'); if (next) *next++ = 0;
        size_t length = strlen(line); if (length && line[length - 1] == '\r') line[--length] = 0;
        char *start = line; while (*start == ' ' || *start == '\t') ++start;
        if (*start && *start != '#' && *start != ';') {
            if (*start == '[') {
                if (section || strcmp(trim(start), "[settings]")) ok = false;
                else section = true;
            } else if (!section) ok = false;
            else {
                char *equals = strchr(start, '=');
                if (!equals) ok = false;
                else {
                    *equals = 0; char *key = trim(start); char *value = equals + 1;
                    unsigned bit = 0; int number = 0, index = -1;
                    if (!strcmp(key, "stopHotkey")) index = HOTKEY_STOP_INDEX;
                    else if (!strncmp(key, "hotkey", 6)) {
                        const char *suffix = key + 6;
                        if (*suffix != '0' && parse_integer(suffix, CLIP_COUNT, &number) && number >= 1) index = number - 1;
                    }
                    if (index >= 0) {
                        if (key_seen[index]) ok = false;
                        else { key_seen[index] = true; ok = hotkey_parse(trim(value), &candidate.hotkeys[index], local_error); }
                    } else {
                        if (!strcmp(key, "version")) bit = 1u;
                        else if (!strcmp(key, "soundGain")) bit = 2u;
                        else if (!strcmp(key, "micGain")) bit = 4u;
                        else if (!strcmp(key, "micMuted")) bit = 8u;
                        else if (!strcmp(key, "micId")) bit = 16u;
                        else if (!strcmp(key, "sendId")) bit = 32u;
                        else if (!strcmp(key, "monitorId")) bit = 64u;
                        if (!bit || (seen & bit)) ok = false;
                        else {
                            seen |= bit;
                            if (bit >= 16u) {
                                size_t n = strlen(value);
                                if (n > SETTINGS_ENDPOINT_MAX || !utf8_valid((const unsigned char *)value, n, true)) ok = false;
                                else {
                                    char *dst = bit == 16u ? candidate.mic_id : bit == 32u ? candidate.send_id : candidate.monitor_id;
                                    memcpy(dst, value, n + 1);
                                }
                            } else {
                                value = trim(value);
                                if (bit == 1u) ok = parse_integer(value, SETTINGS_VERSION, &number) && number == SETTINGS_VERSION;
                                else if (bit == 2u || bit == 4u) {
                                    ok = parse_integer(value, 100, &number);
                                    if (ok) { if (bit == 2u) candidate.sound_gain = number; else candidate.mic_gain = number; }
                                } else { ok = parse_integer(value, 1, &number); if (ok) candidate.mic_muted = number != 0; }
                            }
                        }
                    }
                }
            }
        }
        if (!next) break;
        line = next;
    }
    if (!section || !(seen & 1u)) ok = false;
    if (ok) ok = settings_validate(&candidate, local_error);
    free(copy);
    if (!ok) { fail(error, local_error); return false; }
    candidate.sending = candidate.monitoring = false; *out = candidate; return true;
}
#ifdef _WIN32
static wchar_t *wide_path(const char *path) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!n) return NULL;
    wchar_t *wide = malloc((size_t)n * sizeof *wide); if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, n)) { free(wide); return NULL; }
    return wide;
}
static FILE *file_open(const char *path, bool writing) {
    wchar_t *wide = wide_path(path); if (!wide) { errno = EINVAL; return NULL; }
    FILE *f = _wfopen(wide, writing ? L"wb" : L"rb"); free(wide); return f;
}
static void file_remove(const char *path) {
    wchar_t *wide = wide_path(path); if (wide) { (void)DeleteFileW(wide); free(wide); }
}
static bool file_replace(const char *tmp, const char *path) {
    wchar_t *a = wide_path(tmp), *b = wide_path(path); bool ok = false;
    if (a && b) ok = MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    free(a); free(b); return ok;
}
#else
static FILE *file_open(const char *path, bool writing) { return fopen(path, writing ? "wb" : "rb"); }
static void file_remove(const char *path) { (void)remove(path); }
static bool file_replace(const char *tmp, const char *path) { return rename(tmp, path) == 0; }
#endif
bool settings_load(const char *path, Settings *out, bool *recovered, char error[256]) {
    if (error) error[0] = 0;
    if (recovered) *recovered = false;
    if (!path || !out || !recovered) { fail(error, "Missing settings load arguments"); return false; }
    settings_defaults(out);
    FILE *f = file_open(path, false);
    if (!f) {
        if (errno == ENOENT) { *recovered = true; fail(error, "Settings file is missing; defaults restored"); return true; }
        fail(error, "Unable to open settings file; defaults restored"); return false;
    }
#ifndef _WIN32
    struct stat st;
    if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode)) {
        fclose(f); fail(error, "Settings path is not a regular readable file"); return false;
    }
#endif
    char *data = malloc(SETTINGS_MAX_BYTES + 1u);
    if (!data) { fclose(f); fail(error, "Unable to allocate settings file buffer"); return false; }
    size_t len = fread(data, 1, SETTINGS_MAX_BYTES + 1u, f);
    bool io_ok = !ferror(f); if (fclose(f) != 0) io_ok = false;
    if (!io_ok) { free(data); fail(error, "Unable to read settings file; defaults restored"); return false; }
    bool valid = settings_parse(data, len, out, error); free(data);
    if (!valid) *recovered = true;
    return true;
}
static bool gate(const SettingsFileOps *ops, SettingsFileOperation operation, char error[256]) {
    if (!ops || !ops->allow_operation || ops->allow_operation(ops->ctx, operation)) return true;
    static const char *names[] = {"open", "write", "flush", "close", "replace"};
    if (error) snprintf(error, 256, "Atomic settings save %s failed (injected); previous settings preserved", names[operation]);
    return false;
}
bool settings_save_atomic_with_ops(const char *path, const Settings *s, const SettingsFileOps *ops, char error[256]) {
    if (error) error[0] = 0;
    if (!path || !*path || !settings_validate(s, error)) {
        if (!path || !*path) fail(error, "Invalid settings save path");
        return false;
    }
    size_t pathlen = strlen(path);
    if (pathlen > 32760u) { fail(error, "Settings path is too long"); return false; }
    char *tmp = malloc(pathlen + 5u), *data = malloc(SETTINGS_MAX_BYTES + 1u);
    if (!tmp || !data) { free(tmp); free(data); fail(error, "Unable to allocate atomic settings buffers"); return false; }
    memcpy(tmp, path, pathlen); memcpy(tmp + pathlen, ".tmp", 5);
    Hotkey canonical_keys[HOTKEY_COUNT];
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        /* Canonicalize from validated numeric fields without trusting text. */
        canonical_keys[i] = s->hotkeys[i];
        const char *name = s->hotkeys[i].text;
        if (!memchr(name, 0, HOTKEY_TEXT_CAP) || !hotkey_parse(name, &canonical_keys[i], error) ||
            !hotkey_equal(&canonical_keys[i], &s->hotkeys[i])) {
            free(tmp); free(data); fail(error, "Hotkey text does not match its validated key"); return false;
        }
    }
    int length = snprintf(data, SETTINGS_MAX_BYTES + 1u, "[settings]\nversion=%d\n", SETTINGS_VERSION);
    bool ok = length > 0 && (unsigned)length <= SETTINGS_MAX_BYTES;
    for (int i = 0; i < CLIP_COUNT && ok; ++i) {
        int n = snprintf(data + length, SETTINGS_MAX_BYTES + 1u - (unsigned)length,
                         "hotkey%d=%s\n", i + 1, canonical_keys[i].text);
        ok = n > 0 && (unsigned)n <= SETTINGS_MAX_BYTES - (unsigned)length;
        if (ok) length += n;
    }
    if (ok) {
        int n = snprintf(data + length, SETTINGS_MAX_BYTES + 1u - (unsigned)length,
            "stopHotkey=%s\nsoundGain=%d\nmicGain=%d\nmicMuted=%d\nmicId=%s\nsendId=%s\nmonitorId=%s\n",
            canonical_keys[HOTKEY_STOP_INDEX].text, s->sound_gain, s->mic_gain, s->mic_muted ? 1 : 0,
            s->mic_id, s->send_id, s->monitor_id);
        ok = n > 0 && (unsigned)n <= SETTINGS_MAX_BYTES - (unsigned)length;
        if (ok) length += n;
    }
    FILE *f = NULL;
    if (!ok) fail(error, "Serialized settings exceed 64 KiB");
    if (ok) ok = gate(ops, SETTINGS_FILE_OPEN, error);
    if (ok) { f = file_open(tmp, true); if (!f) { fail(error, "Unable to open temporary settings file; previous settings preserved"); ok = false; } }
    if (ok) ok = gate(ops, SETTINGS_FILE_WRITE, error);
    if (ok && fwrite(data, 1, (size_t)length, f) != (size_t)length) { fail(error, "Atomic settings write failed; previous settings preserved"); ok = false; }
    if (ok) ok = gate(ops, SETTINGS_FILE_FLUSH, error);
    if (ok && fflush(f) != 0) { fail(error, "Atomic settings flush failed; previous settings preserved"); ok = false; }
    if (ok) {
#ifdef _WIN32
        if (_commit(_fileno(f)) != 0) ok = false;
#else
        if (fsync(fileno(f)) != 0) ok = false;
#endif
        if (!ok) fail(error, "Atomic settings durable flush failed; previous settings preserved");
    }
    if (ok) ok = gate(ops, SETTINGS_FILE_CLOSE, error);
    if (f && fclose(f) != 0) { if (ok) fail(error, "Atomic settings close failed; previous settings preserved"); ok = false; }
    if (ok) ok = gate(ops, SETTINGS_FILE_REPLACE, error);
    if (ok && !file_replace(tmp, path)) { fail(error, "Atomic settings replace failed; previous settings preserved"); ok = false; }
    if (!ok) file_remove(tmp);
    free(tmp); free(data); return ok;
}
bool settings_save_atomic(const char *path, const Settings *s, char error[256]) {
    return settings_save_atomic_with_ops(path, s, NULL, error);
}
