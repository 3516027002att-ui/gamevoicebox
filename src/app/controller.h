#ifndef RONGYU_CONTROLLER_H
#define RONGYU_CONTROLLER_H
#include <stdbool.h>
#include <stdint.h>
#include "settings.h"
typedef enum {
    COMMAND_CONNECT, COMMAND_SEND, COMMAND_MONITOR, COMMAND_PLAY,
    COMMAND_STOP, COMMAND_DEVICE_CHANGED, COMMAND_SHUTDOWN,
    COMMAND_GAINS, COMMAND_ROUTE_UPDATE
} CommandKind;
#define DEVICE_ROLE_MIC 1u
#define DEVICE_ROLE_SEND 2u
#define DEVICE_ROLE_MONITOR 4u
#define DEVICE_ROLE_ALL 7u

typedef struct {
    CommandKind kind;
    uint64_t generation;       /* Relevant route/monitor/play generation. */
    uint64_t sequence;         /* All commands strictly ordered. */
    bool enabled;              /* SEND/MONITOR desired state. */
    int clip_index;            /* PLAY: 0..CLIP_COUNT-1. */
    uint32_t changed_roles;    /* DEVICE_CHANGED or ROUTE_UPDATE. */
    Settings settings;        /* Owned UTF-8 IDs, gains and mute snapshot. */
} Command;
typedef struct {
    bool ready;
    bool sending;
    bool monitoring;
    bool shutting_down;
    uint64_t routing_generation;
    uint64_t monitor_generation;
    uint64_t playback_generation;
    /* Terminal domain results survive unrelated updates/UI coalescing.
       Reset only when that domain starts a new operation; shared last_error
       is presentation only and cannot complete another domain. */
    bool route_complete;
    bool monitor_complete;
    char route_error[256];
    char monitor_error[256];
    char last_error[256];
} ControllerState;
typedef struct {
    void *ctx;
    bool (*submit)(void *ctx, const Command *command, char error[256]);
} BackendOps;
typedef struct {
    ControllerState state;
    Settings settings;
    BackendOps backend;
    uint64_t generation_counter;
    uint64_t command_sequence;
    bool route_pending;
} Controller;
void controller_init(Controller *c, const Settings *settings, BackendOps backend);
uint64_t controller_next_generation(Controller *c);
bool controller_dispatch(Controller *c, const Command *cmd, char error[256]);
/* CONNECT completion never enables SEND. Explicit subsequent SEND is needed.
   SEND_OFF, route edits, mic/send invalidation and shutdown cancel completions. */
void controller_complete_route(Controller *c, uint64_t generation,
                               bool ok, const char *error);
/* Worker callbacks must match each domain generation. */
void controller_apply_backend_state(Controller *c, const ControllerState *state);
#endif
