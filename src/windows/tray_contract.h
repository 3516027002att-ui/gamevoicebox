#ifndef SOUNDBOARD_TRAY_CONTRACT_H
#define SOUNDBOARD_TRAY_CONTRACT_H
#include <stdbool.h>
typedef struct { bool installed,hidden; } TrayState;
typedef struct {
    void *ctx;
    bool (*add)(void *ctx);
    void (*remove)(void *ctx);
    void (*hide)(void *ctx);
    void (*restore)(void *ctx);
} TrayOps;
bool tray_minimize(TrayState *state,const TrayOps *ops);
void tray_restore(TrayState *state,const TrayOps *ops);
bool tray_recover(TrayState *state,const TrayOps *ops);
void tray_remove(TrayState *state,const TrayOps *ops);
#endif
