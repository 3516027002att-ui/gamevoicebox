#include "hotkeys.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void fail(char error[256], const char *message) {
    if (error) snprintf(error, 256, "%s", message);
}
static bool same_text(const char *a, const char *b) {
    while (*a && *b) {
        if (toupper((unsigned char)*a++) != toupper((unsigned char)*b++)) return false;
    }
    return !*a && !*b;
}
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) ++s;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}
typedef struct { const char *name; uint32_t vk; } NamedKey;
static const NamedKey named_keys[] = {
    {"Space", 0x20u}, {"Tab", 0x09u}, {"Enter", 0x0du}, {"Escape", 0x1bu},
    {"Backspace", 0x08u}, {"Insert", 0x2du}, {"Delete", 0x2eu},
    {"Home", 0x24u}, {"End", 0x23u}, {"PageUp", 0x21u}, {"PageDown", 0x22u},
    {"Left", 0x25u}, {"Up", 0x26u}, {"Right", 0x27u}, {"Down", 0x28u}
};
static bool key_name(uint32_t key, char out[16]) {
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
        out[0] = (char)key; out[1] = 0; return true;
    }
    if (key >= HOTKEY_F1 && key < HOTKEY_F1 + 24) {
        snprintf(out, 16, "F%u", key - HOTKEY_F1 + 1); return true;
    }
    for (size_t i = 0; i < sizeof named_keys / sizeof *named_keys; ++i) {
        if (named_keys[i].vk == key) { snprintf(out, 16, "%s", named_keys[i].name); return true; }
    }
    return false;
}
static bool supported(const Hotkey *h, char error[256]) {
    if (!h->key) {
        if (!h->modifiers && !h->text[0]) return true;
        fail(error, "An unassigned hotkey must be completely empty"); return false;
    }
    char name[16]; const uint32_t mask = HOTKEY_CTRL | HOTKEY_ALT | HOTKEY_SHIFT | HOTKEY_WIN | HOTKEY_NOREPEAT;
    if ((h->modifiers & ~mask) || !key_name(h->key, name)) {
        fail(error, "Invalid hotkey: use Ctrl/Alt/Shift/Win with a letter, digit or supported key"); return false;
    }
    uint32_t mods = h->modifiers & ~HOTKEY_NOREPEAT;
    if (h->key == 0x1bu && !mods) {
        fail(error, "Unmodified Escape is reserved for stopping effects only inside this window"); return false;
    }
    if (h->key == HOTKEY_F12) { fail(error, "F12 is reserved by the Windows debugger"); return false; }
    if ((h->key == 0x09u && mods == HOTKEY_ALT) ||
        (h->key == 0x2eu && mods == (HOTKEY_CTRL | HOTKEY_ALT)) ||
        (h->key == 'L' && (mods & HOTKEY_WIN)) ||
        (h->key == 0x1bu && mods == HOTKEY_CTRL)) {
        fail(error, "This combination is reserved by Windows"); return false;
    }
    return true;
}
static void canonical(Hotkey *h) {
    char name[16];
    if (!h->key) { h->text[0] = 0; return; }
    (void)key_name(h->key, name);
    snprintf(h->text, sizeof h->text, "%s%s%s%s%s",
        (h->modifiers & HOTKEY_CTRL) ? "Ctrl+" : "",
        (h->modifiers & HOTKEY_ALT) ? "Alt+" : "",
        (h->modifiers & HOTKEY_SHIFT) ? "Shift+" : "",
        (h->modifiers & HOTKEY_WIN) ? "Win+" : "", name);
}
bool hotkey_parse(const char *text, Hotkey *out, char error[256]) {
    char buffer[HOTKEY_TEXT_CAP]; Hotkey h = {0}; bool found_key = false;
    if (error) error[0] = 0;
    if (!text || !out || strlen(text) >= sizeof buffer) { fail(error, "Invalid or oversized hotkey text"); return false; }
    strcpy(buffer, text); char *cursor = trim(buffer);
    if (!*cursor) { *out = h; return true; }
    h.modifiers = HOTKEY_NOREPEAT;
    for (;;) {
        char *plus = strchr(cursor, '+');
        if (plus) *plus = 0;
        char *token = trim(cursor); uint32_t modifier = 0;
        if (!*token) { fail(error, "Hotkey contains an empty key or modifier"); return false; }
        if (same_text(token, "Ctrl")) modifier = HOTKEY_CTRL;
        else if (same_text(token, "Alt")) modifier = HOTKEY_ALT;
        else if (same_text(token, "Shift")) modifier = HOTKEY_SHIFT;
        else if (same_text(token, "Win")) modifier = HOTKEY_WIN;
        if (modifier) {
            if (found_key || (h.modifiers & modifier)) { fail(error, "Duplicate or misplaced hotkey modifier"); return false; }
            h.modifiers |= modifier;
        } else {
            if (found_key || plus) { fail(error, "Hotkey must end with exactly one key"); return false; }
            size_t n = strlen(token);
            if (n == 1 && (isdigit((unsigned char)token[0]) ||
                           ((unsigned char)token[0] < 128 && isalpha((unsigned char)token[0])))) {
                h.key = (uint32_t)toupper((unsigned char)token[0]);
            } else if ((token[0] == 'f' || token[0] == 'F') && n >= 2 && n <= 3) {
                unsigned number = 0;
                for (size_t i = 1; i < n; ++i) {
                    if (!isdigit((unsigned char)token[i])) { fail(error, "Invalid function key"); return false; }
                    number = number * 10 + (unsigned)(token[i] - '0');
                }
                if (number < 1 || number > 24 || token[1] == '0') { fail(error, "Function keys must be F1 through F24"); return false; }
                h.key = HOTKEY_F1 + number - 1;
            } else {
                if (same_text(token, "Esc")) h.key = 0x1bu;
                for (size_t i = 0; i < sizeof named_keys / sizeof *named_keys; ++i)
                    if (same_text(token, named_keys[i].name)) h.key = named_keys[i].vk;
                if (!h.key) { fail(error, "Unsupported hotkey key name"); return false; }
            }
            found_key = true;
        }
        if (!plus) break;
        cursor = plus + 1;
    }
    if (!found_key || !supported(&h, error)) { if (!found_key) fail(error, "Hotkey is missing a key"); return false; }
    canonical(&h); *out = h; return true;
}
void hotkey_defaults(Hotkey out[HOTKEY_COUNT]) {
    if (out) memset(out, 0, sizeof(Hotkey) * HOTKEY_COUNT);
}
bool hotkey_equal(const Hotkey *a, const Hotkey *b) {
    return a && b && a->key == b->key &&
        (a->modifiers & ~HOTKEY_NOREPEAT) == (b->modifiers & ~HOTKEY_NOREPEAT);
}
bool hotkeys_validate(const Hotkey keys[HOTKEY_COUNT], char error[256]) {
    if (error) error[0] = 0;
    if (!keys) { fail(error, "Missing hotkeys"); return false; }
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        if (!supported(&keys[i], error)) return false;
        if (!keys[i].key) continue;
        for (int j = 0; j < i; ++j) if (keys[j].key && hotkey_equal(&keys[i], &keys[j])) {
            fail(error, "Duplicate sound hotkeys are not allowed"); return false;
        }
    }
    return true;
}
static void append_error(char dst[256], const char *prefix, const char *detail) {
    size_t n = strlen(dst);
    if (n < 255) snprintf(dst + n, 256 - n, "%s%s", prefix, detail);
}
static void rollback(HotkeyOps *ops, const HotkeyStatus *before, HotkeyStatus *actual) {
    char error[256];
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        if (actual->registered[i] && (!before->registered[i] || !hotkey_equal(&actual->keys[i], &before->keys[i]))) {
            error[0] = 0;
            if (ops->unregister_key(ops->ctx, HOTKEY_ID_BASE + i, error)) actual->registered[i] = false;
            else append_error(actual->error, "; rollback unregister failed: ", error[0] ? error : "unknown failure");
        }
    }
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        if (before->registered[i] && !actual->registered[i]) {
            Hotkey key = before->keys[i]; key.modifiers |= HOTKEY_NOREPEAT; canonical(&key); error[0] = 0;
            if (ops->register_key(ops->ctx, HOTKEY_ID_BASE + i, &key, error)) {
                actual->registered[i] = true; actual->keys[i] = key;
            } else append_error(actual->error, "; rollback register failed: ", error[0] ? error : "unknown conflict");
        }
    }
    for (int i = 0; i < HOTKEY_COUNT; ++i) if (!actual->registered[i]) actual->keys[i] = before->keys[i];
}
bool hotkeys_replace(HotkeyOps *ops, const Hotkey old[HOTKEY_COUNT], const Hotkey next[HOTKEY_COUNT], HotkeyStatus *actual) {
    char error[256]; (void)old;
    if (!actual) return false;
    actual->error[0] = 0;
    if (!ops || !ops->register_key || !ops->unregister_key || !hotkeys_validate(next, error)) {
        fail(actual->error, (!ops || !ops->register_key || !ops->unregister_key) ? "Missing hotkey registration operations" : error); return false;
    }
    HotkeyStatus before = *actual;
    for (int i = 0; i < HOTKEY_COUNT; ++i) if (actual->registered[i]) {
        error[0] = 0;
        if (!ops->unregister_key(ops->ctx, HOTKEY_ID_BASE + i, error)) {
            fail(actual->error, error[0] ? error : "UnregisterHotKey failed"); rollback(ops, &before, actual); return false;
        }
        actual->registered[i] = false;
    }
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        if (!next[i].key) { actual->keys[i] = next[i]; continue; }
        Hotkey key = next[i]; key.modifiers |= HOTKEY_NOREPEAT; canonical(&key); error[0] = 0;
        if (!ops->register_key(ops->ctx, HOTKEY_ID_BASE + i, &key, error)) {
            fail(actual->error, error[0] ? error : "RegisterHotKey failed (possibly a Windows-reserved key)");
            rollback(ops, &before, actual); return false;
        }
        actual->registered[i] = true; actual->keys[i] = key;
    }
    error[0] = 0;
    if (ops->persist && !ops->persist(ops->ctx, next, error)) {
        fail(actual->error, error[0] ? error : "Atomic settings save failed"); rollback(ops, &before, actual); return false;
    }
    return true;
}
