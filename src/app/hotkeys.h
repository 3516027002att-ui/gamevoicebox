#ifndef RONGYU_HOTKEYS_H
#define RONGYU_HOTKEYS_H
#include <stdbool.h>
#include <stdint.h>
#include "clip_catalog.h"
#define HOTKEY_STOP_INDEX CLIP_COUNT
#define HOTKEY_COUNT (CLIP_COUNT + 1)
#define HOTKEY_TEXT_CAP 64
#define HOTKEY_ID_BASE 0x5200
#define HOTKEY_STOP_ID (HOTKEY_ID_BASE + HOTKEY_STOP_INDEX)
/* Values deliberately match Win32 MOD_* and virtual-key values. */
#define HOTKEY_ALT 0x0001u
#define HOTKEY_CTRL 0x0002u
#define HOTKEY_SHIFT 0x0004u
#define HOTKEY_WIN 0x0008u
#define HOTKEY_NOREPEAT 0x4000u
#define HOTKEY_F1 0x70u
#define HOTKEY_F9 0x78u
#define HOTKEY_F12 0x7bu

typedef struct { uint32_t modifiers; uint32_t key; char text[HOTKEY_TEXT_CAP]; } Hotkey;
typedef struct {
    bool registered[HOTKEY_COUNT];
    Hotkey keys[HOTKEY_COUNT];
    char error[256];
} HotkeyStatus;
typedef struct {
    void *ctx;
    bool (*register_key)(void *ctx, int id, const Hotkey *key, char error[256]);
    bool (*unregister_key)(void *ctx, int id, char error[256]);
    /* Optional atomic settings persistence, after all new keys register. */
    bool (*persist)(void *ctx, const Hotkey next[HOTKEY_COUNT], char error[256]);
} HotkeyOps;
/* An empty Hotkey disables registration. Every default is empty. */
void hotkey_defaults(Hotkey out[HOTKEY_COUNT]);
bool hotkey_parse(const char *text, Hotkey *out, char error[256]);
bool hotkey_equal(const Hotkey *a, const Hotkey *b);
bool hotkeys_validate(const Hotkey keys[HOTKEY_COUNT], char error[256]);
/* actual must be zero-initialized initially, then retained between calls.
   Only successful register/unregister calls change actual; rollback uses
   the actual pre-transaction snapshot, never assumes old is registered. */
bool hotkeys_replace(HotkeyOps *ops, const Hotkey old[HOTKEY_COUNT],
                     const Hotkey next[HOTKEY_COUNT], HotkeyStatus *actual);
#endif
