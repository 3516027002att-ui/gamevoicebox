/* These portable action contracts do not execute Win32 controls or hotkeys. */
#include "../src/windows/ui_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void rect_fits(const UiRect *r,int width,int height) {
    assert(r->width>0 && r->height>0 && r->x>=0 && r->y>=0);
    assert(r->x+r->width<=width && r->y+r->height<=height);
}
static void responsive_layout(void) {
    const int widths[]={320,450,550,740,1100},columns[]={1,1,2,3,4};
    int narrow_height=0,normal_height=0;
    for(size_t n=0;n<sizeof widths/sizeof *widths;++n) {
        UiLayout layout; assert(ui_layout_compute(widths[n],&layout)); assert(layout.columns==columns[n]);
        for(int i=UI_PLAY_0;i<=UI_STATUS;++i) rect_fits(&layout.controls[i],widths[n],layout.content_height);
        for(int i=0;i<UI_LAYOUT_LABEL_COUNT;++i) rect_fits(&layout.labels[i],widths[n],layout.content_height);
        for(int i=0;i<CLIP_COUNT;++i) {
            rect_fits(&layout.clip_status[i],widths[n],layout.content_height);
            assert(layout.controls[UI_PLAY_0+i].width>=9*21+16); /* longest 9 CJK glyph title + native padding */
        }
        printf("UI geometry: %d DIP, %d columns, %d DIP scroll extent; all controls fit\n",widths[n],layout.columns,layout.content_height);
        assert(layout.controls[UI_HOTKEY_0+14].width>=180);
        assert(layout.controls[UI_PLAY_0+13].y+layout.controls[UI_PLAY_0+13].height<layout.controls[UI_STOP].y);
        if(widths[n]==450) narrow_height=layout.content_height;
        if(widths[n]==740) normal_height=layout.content_height;
    }
    assert(narrow_height>normal_height);
    UiLayout invalid; assert(!ui_layout_compute(0,&invalid) && !ui_layout_compute(450,NULL));
}
int main(void) {
    responsive_layout();
    UiAction a;
    Settings before={0},requested={0};
    before.sound_gain=80; before.mic_gain=100; before.mic_muted=false;
    requested.sound_gain=50; requested.mic_gain=20; requested.mic_muted=true;
    ui_gain_result(&requested,&before,false);
    assert(requested.sound_gain==80 && requested.mic_gain==100 && !requested.mic_muted);
    requested.mic_muted=true; ui_gain_result(&requested,&before,true); assert(requested.mic_muted);
    char notices[256]={0};
    ui_notice_update(notices,sizeof(notices),"Recovered corrupt settings",true,false);
    ui_notice_update(notices,sizeof(notices),"F6 occupied",true,false);
    ui_notice_update(notices,sizeof(notices),"Devices refreshed",true,true);
    assert(strstr(notices,"Recovered corrupt settings") && strstr(notices,"F6 occupied"));
    assert(!strstr(notices,"Devices refreshed"));
    ui_notice_update(notices,sizeof(notices),"Devices refreshed",false,true);
    assert(!strcmp(notices,"Devices refreshed"));
    assert(ui_action_for_control(UI_PLAY_0,true,&a) && a.kind==UI_ACTION_PLAY && a.clip_index==0);
    assert(ui_action_for_control(UI_PLAY_1,true,&a) && a.clip_index==1);
    assert(ui_action_for_control(UI_PLAY_2,true,&a) && a.clip_index==2);
    assert(strcmp(ui_clip_label(0),"操操操")==0 && strcmp(ui_clip_label(1),"卡了")==0 && strcmp(ui_clip_label(2),"vamos!")==0);
    assert(ui_action_for_control(UI_PLAY_0+13,true,&a) && a.kind==UI_ACTION_PLAY && a.clip_index==13);
    assert(!strcmp(ui_clip_label(13),"资本") && !ui_clip_label(14)[0] && !ui_clip_label(-1)[0]);
    assert(ui_action_for_control(UI_STOP,true,&a) && a.kind==UI_ACTION_STOP);
    assert(ui_action_for_key(27,true,&a) && a.kind==UI_ACTION_STOP);
    assert(!ui_action_for_key(27,false,&a));
    assert(ui_action_for_control(UI_SEND,false,&a) && a.kind==UI_ACTION_SEND && !a.enabled);
    assert(ui_action_for_control(UI_MONITOR,true,&a) && a.kind==UI_ACTION_MONITOR && a.enabled);
    assert(ui_action_for_control(UI_MIC_MUTE,true,&a) && a.kind==UI_ACTION_GAINS && a.enabled);
    assert(ui_role_for_control(UI_MIC_DEVICE)==ROLE_MIC);
    assert(ui_role_for_control(UI_SEND_DEVICE)==ROLE_SEND);
    assert(ui_role_for_control(UI_MONITOR_DEVICE)==ROLE_MONITOR);
    assert(!ui_startup_send() && !ui_startup_monitor());
    assert(!ui_toggle_enabled(true)); /* second click cancels even before backend ack */
    assert(ui_toggle_enabled(false));
    assert(!ui_effective_enabled(true,false,true));
    assert(!ui_effective_enabled(true,true,false));
    assert(ui_effective_enabled(true,true,true));
    assert(!ui_effective_enabled(false,true,true));
    puts("UI shared adapters: responsive geometry/action/label/startup contracts passed (NOT Win32 runtime)"); return 0;
}
