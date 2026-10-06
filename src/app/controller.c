#include "controller.h"
#include <stdio.h>
#include <string.h>

static void fail(char error[256], const char *message) {
    if (error && error != message) snprintf(error, 256, "%s", message ? message : "Unknown audio error");
}
void controller_init(Controller *c, const Settings *settings, BackendOps backend) {
    if (!c) return;
    memset(c, 0, sizeof *c); c->backend = backend;
    char error[256];
    if (settings && settings_validate(settings, error)) c->settings = *settings;
    else {
        settings_defaults(&c->settings);
        if (settings) fail(c->state.last_error, error);
    }
    c->settings.sending = c->settings.monitoring = false;
}
uint64_t controller_next_generation(Controller *c) {
    if (!c || c->generation_counter == UINT64_MAX) return UINT64_MAX;
    return ++c->generation_counter;
}
static uint64_t new_generation(Controller *c) { return controller_next_generation(c); }
static void refresh_error(Controller *c, const char *fallback) {
    if (c->state.route_error[0]) fail(c->state.last_error, c->state.route_error);
    else if (c->state.monitor_error[0]) fail(c->state.last_error, c->state.monitor_error);
    else if (fallback && *fallback) fail(c->state.last_error, fallback);
    else c->state.last_error[0] = 0;
}
static void reset_route_result(Controller *c, bool complete) {
    c->state.route_complete = complete; c->state.route_error[0] = 0;
}
static void reset_monitor_result(Controller *c, bool complete) {
    c->state.monitor_complete = complete; c->state.monitor_error[0] = 0;
}
static bool invalid(Controller *c, char error[256], const char *message) {
    fail(error, message); if (c) fail(c->state.last_error, message); return false;
}
static void cancel_route(Controller *c, bool clear_ready) {
    c->state.routing_generation = new_generation(c);
    c->route_pending = false; c->state.sending = false;
    if (clear_ready) c->state.ready = false;
}
static void cancel_monitor(Controller *c) {
    c->state.monitor_generation = new_generation(c); c->state.monitoring = false;
}
bool controller_dispatch(Controller *c, const Command *cmd, char error[256]) {
    if (error) error[0] = 0;
    if (!c || !cmd) return invalid(c, error, "Missing controller command");
    if (c->state.shutting_down && cmd->kind != COMMAND_SHUTDOWN) return invalid(c, error, "Audio controller is shutting down");
    if (c->generation_counter > UINT64_MAX - 4 || c->command_sequence == UINT64_MAX)
        return invalid(c, error, "Command generation exhausted; restart the application");
    if (cmd->kind < COMMAND_CONNECT || cmd->kind > COMMAND_ROUTE_UPDATE)
        return invalid(c, error, "Unknown audio command");
    Command send = *cmd; Controller before = *c; uint32_t roles = cmd->changed_roles;
    bool safety_change = false;
    switch (cmd->kind) {
        case COMMAND_CONNECT:
            if (!settings_validate(&cmd->settings, error)) return invalid(c, error, error);
            if (!cmd->settings.mic_id[0] || !cmd->settings.send_id[0]) return invalid(c, error, "Select an explicit microphone and CABLE Input before connecting");
            /* A changed preview selection must invalidate only that domain. */
            if (strcmp(c->settings.monitor_id, cmd->settings.monitor_id)) {
                cancel_monitor(c); reset_monitor_result(c, true);
            }
            c->settings = cmd->settings; c->settings.sending = c->settings.monitoring = false;
            cancel_route(c, true); reset_route_result(c, false);
            c->route_pending = true; send.generation = c->state.routing_generation;
            safety_change = true; break;
        case COMMAND_SEND:
            if (cmd->enabled) {
                if (!c->state.ready || c->route_pending) return invalid(c, error, "Connect the selected route before enabling send");
                /* Status posted before this activation cannot clear its desired state. */
                c->state.routing_generation = new_generation(c);
                reset_route_result(c, false); c->state.sending = true;
            } else {
                bool pending = c->route_pending; cancel_route(c, pending);
                reset_route_result(c, true); safety_change = true;
            }
            send.generation = c->state.routing_generation; break;
        case COMMAND_MONITOR:
            if (cmd->enabled && !c->settings.monitor_id[0]) return invalid(c, error, "Select explicit headphones before enabling preview");
            c->state.monitor_generation = new_generation(c); c->state.monitoring = cmd->enabled;
            reset_monitor_result(c, !cmd->enabled);
            send.generation = c->state.monitor_generation; safety_change = !cmd->enabled; break;
        case COMMAND_PLAY:
            if (cmd->clip_index < 0 || cmd->clip_index >= CLIP_COUNT) return invalid(c, error, "Sound index is outside the bundled catalog");
            if (!c->state.sending && !c->state.monitoring) return invalid(c, error, "Enable send or headphone preview before playing a sound");
            c->state.playback_generation = new_generation(c); send.generation = c->state.playback_generation; break;
        case COMMAND_STOP:
            c->state.playback_generation = new_generation(c); send.generation = c->state.playback_generation;
            safety_change = true; break;
        case COMMAND_DEVICE_CHANGED:
            if (!roles || (roles & ~DEVICE_ROLE_ALL)) return invalid(c, error, "Invalid changed-device role");
            if (roles & (DEVICE_ROLE_MIC | DEVICE_ROLE_SEND)) {
                cancel_route(c, true); reset_route_result(c, true);
                fail(c->state.route_error, "Selected audio endpoint is unavailable; select it and reconnect explicitly");
            }
            if (roles & DEVICE_ROLE_MONITOR) {
                cancel_monitor(c); reset_monitor_result(c, true);
                fail(c->state.monitor_error, "Selected headphone endpoint is unavailable; select it and enable preview explicitly");
            }
            send.generation = (roles & (DEVICE_ROLE_MIC | DEVICE_ROLE_SEND)) ? c->state.routing_generation : c->state.monitor_generation;
            fail(c->state.last_error, "Selected audio endpoint is unavailable; select it and reconnect explicitly");
            safety_change = true; break;
        case COMMAND_SHUTDOWN:
            cancel_route(c, true); cancel_monitor(c);
            reset_route_result(c, true); reset_monitor_result(c, true);
            c->state.playback_generation = new_generation(c);
            c->state.shutting_down = true; send.generation = c->state.routing_generation;
            safety_change = true; break;
        case COMMAND_GAINS:
            if (!settings_validate(&cmd->settings, error)) return invalid(c, error, error);
            c->settings.sound_gain = cmd->settings.sound_gain;
            c->settings.mic_gain = cmd->settings.mic_gain;
            c->settings.mic_muted = cmd->settings.mic_muted;
            send.generation = c->state.routing_generation; break;
        case COMMAND_ROUTE_UPDATE:
            if (!settings_validate(&cmd->settings, error)) return invalid(c, error, error);
            if (!roles) {
                if (strcmp(c->settings.mic_id, cmd->settings.mic_id)) roles |= DEVICE_ROLE_MIC;
                if (strcmp(c->settings.send_id, cmd->settings.send_id)) roles |= DEVICE_ROLE_SEND;
                if (strcmp(c->settings.monitor_id, cmd->settings.monitor_id)) roles |= DEVICE_ROLE_MONITOR;
            }
            if (roles & ~DEVICE_ROLE_ALL) return invalid(c, error, "Invalid endpoint-selection role");
            if (roles & DEVICE_ROLE_MIC) memcpy(c->settings.mic_id, cmd->settings.mic_id, sizeof c->settings.mic_id);
            if (roles & DEVICE_ROLE_SEND) memcpy(c->settings.send_id, cmd->settings.send_id, sizeof c->settings.send_id);
            if (roles & DEVICE_ROLE_MONITOR) memcpy(c->settings.monitor_id, cmd->settings.monitor_id, sizeof c->settings.monitor_id);
            if (roles & (DEVICE_ROLE_MIC | DEVICE_ROLE_SEND)) { cancel_route(c, true); reset_route_result(c, true); }
            if (roles & DEVICE_ROLE_MONITOR) { cancel_monitor(c); reset_monitor_result(c, true); }
            send.changed_roles = roles;
            send.generation = (roles & (DEVICE_ROLE_MIC | DEVICE_ROLE_SEND)) ? c->state.routing_generation : c->state.monitor_generation;
            safety_change = true; break;
    }
    send.sequence = ++c->command_sequence;
    send.settings = c->settings;
    send.settings.sending = c->state.sending; send.settings.monitoring = c->state.monitoring;
    char backend_error[256] = "Backend command submission failed";
    if (!c->backend.submit || !c->backend.submit(c->backend.ctx, &send, backend_error)) {
        if (!safety_change) {
            /* Keep sequence/generation monotonic despite rejected submission. */
            uint64_t generation = c->generation_counter, sequence = c->command_sequence;
            *c = before; c->generation_counter = generation; c->command_sequence = sequence;
        } else if (cmd->kind == COMMAND_CONNECT) c->route_pending = false;
        fail(c->state.last_error, backend_error); fail(error, backend_error); return false;
    }
    refresh_error(c, NULL);
    c->settings.sending = c->state.sending; c->settings.monitoring = c->state.monitoring;
    return true;
}
void controller_complete_route(Controller *c, uint64_t generation, bool ok, const char *error) {
    if (!c || c->state.shutting_down || !c->route_pending || generation != c->state.routing_generation) return;
    c->route_pending = false; c->state.ready = ok; c->state.sending = false;
    c->settings.sending = false; c->state.route_complete = true;
    if (ok) c->state.route_error[0] = 0;
    else {
        fail(c->state.route_error, (error && *error) ? error : "Selected audio route could not be opened");
        cancel_route(c, true);
    }
    refresh_error(c, NULL);
}
void controller_apply_backend_state(Controller *c, const ControllerState *state) {
    if (!c || !state || c->state.shutting_down) return;
    bool routing = state->routing_generation == c->state.routing_generation;
    bool monitor = state->monitor_generation == c->state.monitor_generation;
    bool playback = state->playback_generation == c->state.playback_generation;
    if (routing) {
        bool pending = c->route_pending;
        bool failed = !state->ready && (state->route_complete || state->route_error[0]) &&
            (pending || c->state.ready || c->state.sending);
        if (state->route_complete || state->route_error[0]) {
            c->state.route_complete = true;
            fail(c->state.route_error, state->route_error);
        }
        if (failed) {
            if (!c->state.route_error[0]) fail(c->state.route_error,
                "Selected audio route could not be opened");
            c->state.route_complete = true; cancel_route(c, true);
        } else {
            if (state->route_complete || state->ready || state->sending) {
                c->state.ready = state->ready;
                /* A CONNECT completion cannot implicitly enable send. */
                c->state.sending = pending ? false : (c->state.sending && state->sending && state->ready);
            }
            if (pending && (state->route_complete || state->ready)) {
                c->route_pending = false; c->state.route_complete = true;
            }
        }
    }
    if (monitor) {
        if (state->monitor_complete || state->monitor_error[0]) {
            c->state.monitor_complete = true;
            fail(c->state.monitor_error, state->monitor_error);
        }
        if (c->state.monitoring && !state->monitoring &&
            (state->monitor_complete || state->monitor_error[0])) {
            if (!c->state.monitor_error[0]) fail(c->state.monitor_error,
                "Selected headphone preview could not be opened");
            c->state.monitor_complete = true; cancel_monitor(c);
        } else if (state->monitor_complete || state->monitoring) {
            c->state.monitoring = c->state.monitoring && state->monitoring;
        }
    }
    if (routing || monitor || playback) refresh_error(c, (routing && monitor) ? state->last_error : NULL);
    c->settings.sending = c->state.sending; c->settings.monitoring = c->state.monitoring;
}
