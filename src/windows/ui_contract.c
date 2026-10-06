#include "ui_contract.h"
#include <stdio.h>
#include <string.h>
bool ui_action_for_control(int id,bool checked,UiAction *a) {
    a->enabled=checked; a->clip_index=-1;
    if(id>=UI_PLAY_0 && id<=UI_PLAY_LAST) { a->kind=UI_ACTION_PLAY; a->clip_index=id-UI_PLAY_0; }
    else if(id==UI_STOP) a->kind=UI_ACTION_STOP;
    else if(id==UI_CONNECT) a->kind=UI_ACTION_CONNECT;
    else if(id==UI_SEND) a->kind=UI_ACTION_SEND;
    else if(id==UI_MONITOR) a->kind=UI_ACTION_MONITOR;
    else if(id==UI_MIC_MUTE || id==UI_SOUND_GAIN || id==UI_MIC_GAIN) a->kind=UI_ACTION_GAINS;
    else return false;
    return true;
}
bool ui_action_for_key(unsigned key,bool focused,UiAction *a) { return key==27 && focused && ui_action_for_control(UI_STOP,false,a); }
EndpointRole ui_role_for_control(int id) { return id==UI_MIC_DEVICE?ROLE_MIC:id==UI_SEND_DEVICE?ROLE_SEND:id==UI_MONITOR_DEVICE?ROLE_MONITOR:0; }
const char *ui_clip_label(int index) { return clip_catalog_label(index); }
bool ui_startup_send(void) { return false; }
bool ui_startup_monitor(void) { return false; }
bool ui_effective_enabled(bool desired,bool actual,bool generation_matches) { return desired && actual && generation_matches; }

bool ui_toggle_enabled(bool desired) { return !desired; }

void ui_notice_update(char *out,size_t capacity,const char *message,bool startup,bool success) {
    size_t len,n;
    if(!out || !capacity || !message) return;
    if(startup && out[0]) {
        if(success) return;
        len=strlen(out); n=strlen(message);
        if(len+2+n<capacity) { memcpy(out+len,"\r\n",2); memcpy(out+len+2,message,n+1); }
        return;
    }
    if(strlen(message)>=capacity) snprintf(out,capacity,"Operation message exceeds display limit");
    else memcpy(out,message,strlen(message)+1);
}

void ui_gain_result(Settings *requested,const Settings *before,bool accepted) {
    if(!accepted) {
        requested->sound_gain=before->sound_gain; requested->mic_gain=before->mic_gain; requested->mic_muted=before->mic_muted;
    }
}

static void rect_set(UiRect *r,int x,int y,int width,int height) {
    *r=(UiRect){x,y,width,height};
}
static int button_flow(UiLayout *out,const int *ids,const int *widths,size_t count,int available,int y) {
    const int margin=18,gap=12; int x=margin;
    for(size_t i=0;i<count;++i) {
        int width=widths[i]<available?widths[i]:available;
        if(x>margin && width>margin+available-x) { x=margin; y+=42; }
        rect_set(&out->controls[ids[i]],x,y,width,34); x+=width+gap;
    }
    return y+34;
}
bool ui_layout_compute(int width,UiLayout *out) {
    const int margin=18,gap=12; int available,rows,tile,y;
    if(!out || width<=2*margin) return false;
    memset(out,0,sizeof *out); available=width-2*margin;
    out->columns=width>=1100?4:width>=700?3:width>=480?2:1;
    rows=(CLIP_COUNT+out->columns-1)/out->columns;
    tile=(available-(out->columns-1)*gap)/out->columns;
    for(int i=0;i<CLIP_COUNT;++i) {
        int x=margin+(i%out->columns)*(tile+gap),top=18+(i/out->columns)*112;
        rect_set(&out->controls[UI_PLAY_0+i],x,top,tile,50);
        rect_set(&out->controls[UI_HOTKEY_0+i],x,top+56,tile,26);
        rect_set(&out->clip_status[i],x,top+86,tile,22);
    }
    y=18+rows*112+8;
    if(width>=700) {
        rect_set(&out->controls[UI_STOP],margin,y,160,36);
        rect_set(&out->labels[7],margin+172,y+7,230,24);
        rect_set(&out->controls[UI_HOTKEY_0+HOTKEY_STOP_INDEX],margin+402,y+4,available-402,28);
        y+=48;
    } else {
        rect_set(&out->controls[UI_STOP],margin,y,available,36);
        rect_set(&out->labels[7],margin,y+46,available,24);
        rect_set(&out->controls[UI_HOTKEY_0+HOTKEY_STOP_INDEX],margin,y+74,available,28);
        y+=116;
    }
    {
        int help_height=width>=700?42:72;
        rect_set(&out->labels[5],margin,y,available,help_height); y+=help_height+8;
    }
    rect_set(&out->controls[UI_SAVE_HOTKEYS],margin,y,available<190?available:190,32); y+=46;
    for(int i=0;i<3;++i) {
        if(width>=600) {
            rect_set(&out->labels[i],margin,y+5,155,24);
            rect_set(&out->controls[UI_MIC_DEVICE+i],margin+155,y,available-155,300); y+=58;
        } else {
            rect_set(&out->labels[i],margin,y,available,24);
            rect_set(&out->controls[UI_MIC_DEVICE+i],margin,y+25,available,300); y+=62;
        }
    }
    {
        const int ids[]={UI_CONNECT,UI_SEND,UI_MONITOR,UI_MIC_MUTE},widths[]={150,125,125,140};
        y=button_flow(out,ids,widths,4,available,y+3)+14;
    }
    for(int i=0;i<2;++i) {
        if(width>=480) {
            rect_set(&out->labels[3+i],margin,y+10,130,24);
            rect_set(&out->controls[UI_SOUND_GAIN+i],margin+130,y,available-130,40); y+=48;
        } else {
            rect_set(&out->labels[3+i],margin,y,available,24);
            rect_set(&out->controls[UI_SOUND_GAIN+i],margin,y+24,available,40); y+=72;
        }
    }
    {
        const int ids[]={UI_REFRESH,UI_GUIDE,UI_DETAILS},widths[]={125,125,125};
        y=button_flow(out,ids,widths,3,available,y+6)+12;
    }
    {
        const int ids[]={UI_DRIVER,UI_VENDOR},widths[]={220,180};
        y=button_flow(out,ids,widths,2,available,y)+14;
    }
    {
        int safe_height=available>=600?24:48;
        rect_set(&out->labels[6],margin,y,available,safe_height); y+=safe_height+12;
    }
    int status_height=width>=700?240:320;
    rect_set(&out->controls[UI_STATUS],margin,y,available,status_height); out->content_height=y+status_height+10;
    return true;
}
