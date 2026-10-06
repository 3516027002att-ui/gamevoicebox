#ifndef RONGYU_SETTINGS_H
#define RONGYU_SETTINGS_H
#include <stdbool.h>
#include <stddef.h>
#include "hotkeys.h"
#define SETTINGS_VERSION 1
#define SETTINGS_MAX_BYTES (64u * 1024u)
#define SETTINGS_ENDPOINT_MAX 4096u
#define SETTINGS_ENDPOINT_CAP (SETTINGS_ENDPOINT_MAX + 1u)
typedef struct {
    int version;
    Hotkey hotkeys[HOTKEY_COUNT];
    int sound_gain;
    int mic_gain;
    bool mic_muted;
    char mic_id[SETTINGS_ENDPOINT_CAP];
    char send_id[SETTINGS_ENDPOINT_CAP];
    char monitor_id[SETTINGS_ENDPOINT_CAP];
    /* Runtime only: never serialized and always false after parse/load. */
    bool sending;
    bool monitoring;
} Settings;
/* Explicit fault injection gates default real file operations; NULL uses
   normal operations. Each denied gate occurs before the operation. */
typedef enum {
    SETTINGS_FILE_OPEN, SETTINGS_FILE_WRITE, SETTINGS_FILE_FLUSH,
    SETTINGS_FILE_CLOSE, SETTINGS_FILE_REPLACE
} SettingsFileOperation;
typedef struct {
    void *ctx;
    bool (*allow_operation)(void *ctx, SettingsFileOperation operation);
} SettingsFileOps;
void settings_defaults(Settings *out);
bool settings_validate(const Settings *s, char error[256]);
bool settings_parse(const char *data, size_t len, Settings *out, char error[256]);
bool settings_load(const char *path, Settings *out, bool *recovered, char error[256]);
bool settings_save_atomic(const char *path, const Settings *s, char error[256]);
bool settings_save_atomic_with_ops(const char *path, const Settings *s,
                                   const SettingsFileOps *ops, char error[256]);
#endif
