#ifndef RONGYU_UI_CONTRACT_H
#define RONGYU_UI_CONTRACT_H
#include <stdbool.h>
#include <stddef.h>
#include "device_contract.h"
#include "../app/settings.h"
enum {
    UI_PLAY_0=100,UI_PLAY_1,UI_PLAY_2,UI_PLAY_LAST=UI_PLAY_0+CLIP_COUNT-1,
    UI_STOP,UI_MIC_DEVICE,UI_SEND_DEVICE,UI_MONITOR_DEVICE,UI_CONNECT,UI_SEND,
    UI_MONITOR,UI_MIC_MUTE,UI_SOUND_GAIN,UI_MIC_GAIN,
    UI_HOTKEY_0,UI_HOTKEY_1,UI_HOTKEY_2,UI_HOTKEY_LAST=UI_HOTKEY_0+HOTKEY_COUNT-1,
    UI_SAVE_HOTKEYS,UI_REFRESH,UI_GUIDE,UI_DRIVER,UI_VENDOR,UI_DETAILS,UI_STATUS
};
#define UI_LAYOUT_LABEL_COUNT 8
typedef struct { int x,y,width,height; } UiRect;
typedef struct {
    int columns,content_height;
    UiRect controls[UI_STATUS+1];
    UiRect labels[UI_LAYOUT_LABEL_COUNT];
    UiRect clip_status[CLIP_COUNT];
} UiLayout;
/* Logical DIP geometry, shared with the actual native control placement. */
bool ui_layout_compute(int client_width,UiLayout *out);
typedef enum { UI_ACTION_PLAY,UI_ACTION_STOP,UI_ACTION_CONNECT,UI_ACTION_SEND,
               UI_ACTION_MONITOR,UI_ACTION_GAINS } UiActionKind;
typedef struct { UiActionKind kind; int clip_index; bool enabled; } UiAction;
bool ui_action_for_control(int id,bool checked,UiAction *out);
bool ui_action_for_key(unsigned key,bool window_focused,UiAction *out);
EndpointRole ui_role_for_control(int id);
const char *ui_clip_label(int index);
void ui_gain_result(Settings *requested,const Settings *before,bool accepted);
void ui_notice_update(char *out,size_t capacity,const char *message,bool startup,bool success);
bool ui_toggle_enabled(bool desired);
bool ui_effective_enabled(bool desired,bool actual,bool generation_matches);
bool ui_startup_send(void);
bool ui_startup_monitor(void);
#endif
