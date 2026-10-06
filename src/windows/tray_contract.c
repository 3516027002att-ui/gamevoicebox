#include "tray_contract.h"
#include <stddef.h>
static bool valid(const TrayState *state,const TrayOps *ops) {
    return state && ops && ops->add && ops->hide && ops->restore && ops->remove;
}
bool tray_minimize(TrayState *state,const TrayOps *ops) {
    if(!valid(state,ops)) return false;
    if(!state->installed) state->installed=ops->add(ops->ctx);
    if(!state->installed) { state->hidden=false; ops->restore(ops->ctx); return false; }
    state->hidden=true; ops->hide(ops->ctx); return true;
}
void tray_restore(TrayState *state,const TrayOps *ops) {
    if(!valid(state,ops)) return;
    state->hidden=false; ops->restore(ops->ctx);
}
bool tray_recover(TrayState *state,const TrayOps *ops) {
    if(!valid(state,ops)) return false;
    state->installed=ops->add(ops->ctx);
    if(!state->installed && state->hidden) tray_restore(state,ops);
    return state->installed;
}
void tray_remove(TrayState *state,const TrayOps *ops) {
    if(!valid(state,ops)) return;
    if(state->installed) ops->remove(ops->ctx);
    state->installed=state->hidden=false;
}
