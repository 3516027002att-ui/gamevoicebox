#include "controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { Command last; size_t calls; bool fail; } Recorder;
static bool submit(void *ctx, const Command *cmd, char error[256]) {
    Recorder *r = ctx;
    if (r->fail) { strcpy(error, "backend queue unavailable"); return false; }
    r->last = *cmd; ++r->calls; return true;
}
static void setup(Controller *c, Recorder *r) {
    Settings s; settings_defaults(&s);
    strcpy(s.mic_id, "mic exact opaque"); strcpy(s.send_id, "send exact opaque"); strcpy(s.monitor_id, "monitor exact opaque");
    s.sending = s.monitoring = true;
    BackendOps backend = {r, submit}; controller_init(c, &s, backend);
    assert(!c->state.ready && !c->state.sending && !c->state.monitoring);
}
static uint64_t connect(Controller *c, Recorder *r) {
    Command cmd = {0}; char error[256]; cmd.kind = COMMAND_CONNECT; cmd.settings = c->settings;
    assert(controller_dispatch(c, &cmd, error));
    assert(c->route_pending && !c->state.ready && !c->state.sending);
    assert(r->last.kind == COMMAND_CONNECT && r->last.generation == c->state.routing_generation);
    assert(!strcmp(r->last.settings.mic_id, "mic exact opaque"));
    return r->last.generation;
}
static void stale_completions(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; uint64_t gen;
    setup(&c, &r); gen = connect(&c, &r); cmd.kind = COMMAND_SEND; cmd.enabled = false;
    assert(controller_dispatch(&c, &cmd, error)); controller_complete_route(&c, gen, true, NULL);
    assert(!c.state.ready && !c.state.sending && !c.route_pending);
    gen = connect(&c, &r); cmd.kind = COMMAND_ROUTE_UPDATE; cmd.changed_roles = DEVICE_ROLE_SEND;
    cmd.settings = c.settings; strcpy(cmd.settings.send_id, "new explicit send");
    assert(controller_dispatch(&c, &cmd, error)); controller_complete_route(&c, gen, true, NULL);
    assert(!c.state.ready && !c.state.sending && !strcmp(c.settings.send_id, "new explicit send"));
    gen = connect(&c, &r); cmd.kind = COMMAND_DEVICE_CHANGED; cmd.changed_roles = DEVICE_ROLE_MIC;
    assert(controller_dispatch(&c, &cmd, error)); controller_complete_route(&c, gen, true, NULL);
    assert(!c.state.ready && !c.state.sending && c.state.last_error[0]);
    gen = connect(&c, &r); cmd.kind = COMMAND_SHUTDOWN;
    assert(controller_dispatch(&c, &cmd, error)); controller_complete_route(&c, gen, true, NULL);
    assert(c.state.shutting_down && !c.state.ready && !c.state.sending && !c.state.monitoring);
    cmd.kind = COMMAND_CONNECT; assert(!controller_dispatch(&c, &cmd, error));
}
static void outputs_and_stop(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; setup(&c, &r);
    cmd.kind = COMMAND_SEND; cmd.enabled = true;
    assert(!controller_dispatch(&c, &cmd, error)); assert(r.calls == 0);
    uint64_t gen = connect(&c, &r); controller_complete_route(&c, gen, true, NULL);
    assert(c.state.ready && !c.state.sending);
    assert(controller_dispatch(&c, &cmd, error)); assert(c.state.sending);
    cmd.kind = COMMAND_MONITOR; assert(controller_dispatch(&c, &cmd, error)); assert(c.state.monitoring);
    cmd.kind = COMMAND_PLAY; cmd.clip_index = 13;
    assert(controller_dispatch(&c, &cmd, error)); uint64_t play = c.state.playback_generation;
    size_t accepted_calls = r.calls;
    cmd.clip_index = 14; assert(!controller_dispatch(&c, &cmd, error) && r.calls == accepted_calls);
    cmd.clip_index = -1; assert(!controller_dispatch(&c, &cmd, error) && r.calls == accepted_calls);
    cmd.kind = COMMAND_STOP; assert(controller_dispatch(&c, &cmd, error));
    assert(c.state.playback_generation > play && c.state.sending && c.state.monitoring);
    uint64_t route = c.state.routing_generation;
    cmd.kind = COMMAND_DEVICE_CHANGED; cmd.changed_roles = DEVICE_ROLE_MONITOR;
    assert(controller_dispatch(&c, &cmd, error));
    assert(!c.state.monitoring && c.state.sending && c.state.ready && c.state.routing_generation == route);
    cmd.kind = COMMAND_SEND; cmd.enabled = false;
    assert(controller_dispatch(&c, &cmd, error)); assert(c.state.ready && !c.state.sending);
    ControllerState late = c.state; late.routing_generation = route; late.sending = true;
    controller_apply_backend_state(&c, &late); assert(!c.state.sending);
    cmd.kind = COMMAND_PLAY; cmd.clip_index = 14; assert(!controller_dispatch(&c, &cmd, error));
    cmd.clip_index = 0; size_t calls = r.calls; assert(!controller_dispatch(&c, &cmd, error)); assert(r.calls == calls);
}
static void backend_failure_and_domains(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; setup(&c, &r);
    uint64_t gen = connect(&c, &r); controller_complete_route(&c, gen, false, "capture access denied");
    assert(!c.state.ready && !c.state.sending && strstr(c.state.last_error, "denied"));
    gen = connect(&c, &r); controller_complete_route(&c, gen, true, NULL);
    cmd.kind = COMMAND_SEND; cmd.enabled = true; r.fail = true;
    assert(!controller_dispatch(&c, &cmd, error)); assert(!c.state.sending && error[0]);
    r.fail = false; assert(controller_dispatch(&c, &cmd, error)); assert(c.state.sending);
    cmd.kind = COMMAND_GAINS; cmd.settings = c.settings;
    cmd.settings.sound_gain = 17; cmd.settings.mic_gain = 23; cmd.settings.mic_muted = true;
    uint64_t route = c.state.routing_generation;
    assert(controller_dispatch(&c, &cmd, error));
    assert(c.settings.mic_gain == 23 && c.settings.mic_muted && c.state.sending && c.state.routing_generation == route);
    cmd.settings.mic_gain = 101; assert(!controller_dispatch(&c, &cmd, error));
    cmd.kind = COMMAND_ROUTE_UPDATE; cmd.changed_roles = DEVICE_ROLE_MONITOR; cmd.settings = c.settings;
    strcpy(cmd.settings.monitor_id, "new headphones");
    assert(controller_dispatch(&c, &cmd, error)); assert(c.state.sending && c.state.ready);
    assert(!strcmp(c.settings.mic_id, "mic exact opaque"));
    cmd.kind = COMMAND_DEVICE_CHANGED; cmd.changed_roles = DEVICE_ROLE_SEND;
    r.fail = true; assert(!controller_dispatch(&c, &cmd, error));
    assert(!c.state.ready && !c.state.sending); /* Safety invalidation survives queue failure. */
    r.fail = false;
    ControllerState stale = c.state; stale.routing_generation = route; stale.ready = stale.sending = true;
    controller_apply_backend_state(&c, &stale); assert(!c.state.ready && !c.state.sending);
    uint64_t a = controller_next_generation(&c), b = controller_next_generation(&c); assert(b > a);
}
static void failed_backend_domains_reject_older_snapshots(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; setup(&c, &r);
    uint64_t gen = connect(&c, &r); controller_complete_route(&c, gen, true, NULL);
    cmd.kind = COMMAND_SEND; cmd.enabled = true;
    assert(controller_dispatch(&c, &cmd, error));
    ControllerState earlier = c.state, failure = c.state;
    failure.ready = failure.sending = false; failure.route_complete = true;
    strcpy(failure.route_error, "capture device invalidated");
    strcpy(failure.last_error, "capture device invalidated");
    controller_apply_backend_state(&c, &failure);
    assert(!c.state.ready && !c.state.sending);
    controller_apply_backend_state(&c, &earlier);
    assert(!c.state.ready && !c.state.sending);
    cmd.kind = COMMAND_MONITOR; assert(controller_dispatch(&c, &cmd, error));
    earlier = c.state; failure = c.state; failure.monitoring = false; failure.monitor_complete = true;
    strcpy(failure.monitor_error, "headphone device invalidated");
    strcpy(failure.last_error, "headphone device invalidated");
    controller_apply_backend_state(&c, &failure);
    assert(!c.state.monitoring);
    controller_apply_backend_state(&c, &earlier); assert(!c.state.monitoring);
}
static void send_on_rejects_pre_activation_snapshots(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; setup(&c, &r);
    uint64_t gen = connect(&c, &r); controller_complete_route(&c, gen, true, NULL);
    ControllerState before_send = c.state;
    cmd.kind = COMMAND_SEND; cmd.enabled = true;
    assert(controller_dispatch(&c, &cmd, error)); assert(c.state.sending);
    ControllerState sending = c.state;
    controller_apply_backend_state(&c, &before_send);
    assert(c.state.ready && c.state.sending);
    controller_apply_backend_state(&c, &sending);
    assert(c.state.ready && c.state.sending);
    assert(c.state.routing_generation > before_send.routing_generation);
}
static void coalesced_monitor_success_preserves_route_failure(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; setup(&c, &r);
    uint64_t gen = connect(&c, &r);
    cmd.kind = COMMAND_MONITOR; cmd.enabled = true;
    assert(controller_dispatch(&c, &cmd, error));
    /* The UI receives only the newest mailbox snapshot: monitor succeeded
       after CONNECT failed. Shared last_error was cleared by that success. */
    ControllerState final = c.state;
    final.routing_generation = gen;
    final.ready = final.sending = false;
    final.route_complete = true;
    strcpy(final.route_error, "capture privacy denied for selected endpoint");
    final.monitoring = true; final.monitor_complete = true;
    final.last_error[0] = 0;
    controller_apply_backend_state(&c, &final);
    assert(!c.route_pending && !c.state.ready && !c.state.sending);
    assert(c.state.monitoring);
    assert(strstr(c.state.last_error, "capture privacy denied"));
    assert(strstr(c.state.route_error, "capture privacy denied"));
    cmd.kind = COMMAND_GAINS; cmd.settings = c.settings;
    assert(controller_dispatch(&c, &cmd, error));
    assert(strstr(c.state.last_error, "capture privacy denied"));
    gen = connect(&c, &r);
    assert(!c.state.route_complete && !c.state.route_error[0]);
    controller_complete_route(&c, gen, true, NULL);
    assert(c.state.route_complete && !c.state.route_error[0]);
}
static void independent_preview_error_does_not_complete_pending_route(void) {
    Controller c; Recorder r = {0}; Command cmd = {0}; char error[256]; setup(&c, &r);
    uint64_t gen = connect(&c, &r);
    cmd.kind = COMMAND_MONITOR; cmd.enabled = true;
    assert(controller_dispatch(&c, &cmd, error));
    ControllerState progress = c.state;
    progress.route_complete = false; progress.route_error[0] = 0;
    progress.monitoring = false; progress.monitor_complete = true;
    strcpy(progress.monitor_error, "selected headphones could not be opened");
    strcpy(progress.last_error, progress.monitor_error);
    controller_apply_backend_state(&c, &progress);
    assert(c.route_pending && c.state.routing_generation == gen && !c.state.ready);
    assert(!c.state.monitoring && strstr(c.state.monitor_error, "headphones"));
    assert(!c.state.route_error[0]);
    progress = c.state; progress.routing_generation = gen;
    progress.ready = true; progress.route_complete = true;
    controller_apply_backend_state(&c, &progress);
    assert(!c.route_pending && c.state.ready && !c.state.sending);
    assert(strstr(c.state.last_error, "headphones"));
}
int main(void) {
    stale_completions(); outputs_and_stop(); backend_failure_and_domains(); failed_backend_domains_reject_older_snapshots(); send_on_rejects_pre_activation_snapshots(); coalesced_monitor_success_preserves_route_failure(); independent_preview_error_does_not_complete_pending_route();
    puts("controller: stale completion cancellation, output domain isolation, stop/mute preservation and queue failure safety passed");
    return 0;
}
