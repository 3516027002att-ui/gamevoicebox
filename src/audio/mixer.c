#include "mixer.h"
#include <math.h>
#include <string.h>

static float finite_sample(float x) { if(!isfinite(x)) return 0; return x>1?1:(x< -1?-1:x); }
static float gain(float x) { if(!isfinite(x)) return 0; return x<0?0:(x>1?1:x); }
static void clear_mic(Mixer *m) {
    m->mic_head=0; m->mic_count=0; m->mic_phase=0; m->mic_warm=false;
    m->capture_rate=0; m->capture_previous_valid=false; m->capture_next=0;
    m->capture_previous[0]=0; m->capture_previous[1]=0;
}
void mixer_init(Mixer *m,const Clip clips[CLIP_COUNT]) {
    if(!m) return;
    memset(m,0,sizeof *m); m->clips=clips; m->sound_gain=0.8f; m->mic_gain=1;
    m->clip_index[SEND]=-1; m->clip_index[MONITOR]=-1;
}
void mixer_stop_clip(Mixer *m) {
    if(!m) return;
    m->clip_index[SEND]=-1; m->clip_index[MONITOR]=-1;
    m->clip_phase[SEND]=0; m->clip_phase[MONITOR]=0; ++m->play_generation;
}
void mixer_clear_send(Mixer *m) {
    if(!m) return;
    m->clip_index[SEND]=-1; m->clip_phase[SEND]=0; clear_mic(m); ++m->play_generation;
}
void mixer_clear_monitor(Mixer *m) {
    if(!m) return;
    m->clip_index[MONITOR]=-1; m->clip_phase[MONITOR]=0; ++m->play_generation;
}
void mixer_reset(Mixer *m) { if(m) { mixer_stop_clip(m); clear_mic(m); } }
void mixer_play(Mixer *m,int clip_index,bool send_active,bool monitor_active) {
    if(!m) return;
    mixer_stop_clip(m);
    if(!m->clips||clip_index<0||clip_index>=CLIP_COUNT||!m->clips[clip_index].samples||!m->clips[clip_index].frames) return;
    if(send_active) m->clip_index[SEND]=clip_index;
    if(monitor_active) m->clip_index[MONITOR]=clip_index;
}
void mixer_set_gains(Mixer *m,float sound,float mic,bool mic_muted) {
    if(!m) return;
    m->sound_gain=gain(sound); m->mic_gain=gain(mic); m->mic_muted=mic_muted;
}
static void enqueue(Mixer *m,float left,float right) {
    if(m->mic_count==MIXER_MIC_CAPACITY) {
        /* Drop the oldest complete frame and reset render interpolation at
         * the gap. Never interpolate across data discarded for latency. */
        m->mic_head=(m->mic_head+1u)%MIXER_MIC_CAPACITY; --m->mic_count; m->mic_phase=0;
    }
    size_t tail=(m->mic_head+m->mic_count)%MIXER_MIC_CAPACITY;
    m->mic_queue[tail*2u]=left; m->mic_queue[tail*2u+1u]=right; ++m->mic_count;
}
void mixer_push_mic(Mixer *m,const float *stereo,size_t frames,uint32_t capture_rate,bool discontinuity) {
    if(!m) return;
    if(discontinuity||(m->capture_rate&&m->capture_rate!=capture_rate)) clear_mic(m);
    if(!stereo||frames>SIZE_MAX/2u||capture_rate<8000||capture_rate>192000) return;
    m->capture_rate=capture_rate; double step=(double)capture_rate/MIXER_BUS_RATE;
    for(size_t i=0;i<frames;i++) {
        float left=finite_sample(stereo[i*2u]),right=finite_sample(stereo[i*2u+1u]);
        if(!m->capture_previous_valid) {
            enqueue(m,left,right); m->capture_previous_valid=true; m->capture_next=step;
        } else {
            /* next is measured relative to the previous input frame. Carry
             * the fractional time into the next packet without restarting. */
            while(m->capture_next<=1.0) {
                double alpha=m->capture_next;
                enqueue(m,(float)(m->capture_previous[0]+(left-m->capture_previous[0])*alpha),
                          (float)(m->capture_previous[1]+(right-m->capture_previous[1])*alpha));
                m->capture_next+=step;
            }
            m->capture_next-=1.0;
        }
        m->capture_previous[0]=left; m->capture_previous[1]=right;
    }
}
static void read_clip(Mixer *m,OutputKind out,double step,float *left,float *right) {
    *left=0; *right=0; int index=m->clip_index[out];
    if(index<0||!m->clips) return;
    const Clip *clip=&m->clips[index]; double phase=m->clip_phase[out];
    if(phase>=(double)clip->frames) { m->clip_index[out]=-1; return; }
    size_t frame=(size_t)phase; size_t next=frame+1u<clip->frames?frame+1u:frame;
    double alpha=phase-(double)frame;
    *left=(float)(finite_sample(clip->samples[frame*2u])*(1.0-alpha)+finite_sample(clip->samples[next*2u])*alpha);
    *right=(float)(finite_sample(clip->samples[frame*2u+1u])*(1.0-alpha)+finite_sample(clip->samples[next*2u+1u])*alpha);
    m->clip_phase[out]+=step;
    if(m->clip_phase[out]>=(double)clip->frames) m->clip_index[out]=-1;
}
static void mic_underflow(Mixer *m) { m->mic_count=0; m->mic_head=0; m->mic_phase=0; m->mic_warm=false; }
static void read_mic(Mixer *m,double step,float *left,float *right) {
    *left=0; *right=0;
    if(!m->mic_warm) return;
    if(!m->mic_count||(m->mic_phase>0&&m->mic_count<2u)) { mic_underflow(m); return; }
    size_t first=m->mic_head,second=(first+1u)%MIXER_MIC_CAPACITY;
    if(m->mic_count==1u) second=first;
    double alpha=m->mic_phase;
    *left=(float)(m->mic_queue[first*2u]*(1.0-alpha)+m->mic_queue[second*2u]*alpha);
    *right=(float)(m->mic_queue[first*2u+1u]*(1.0-alpha)+m->mic_queue[second*2u+1u]*alpha);
    m->mic_phase+=step; size_t consumed=(size_t)m->mic_phase;
    if(consumed>=m->mic_count) { mic_underflow(m); return; }
    m->mic_head=(m->mic_head+consumed)%MIXER_MIC_CAPACITY;
    m->mic_count-=consumed; m->mic_phase-=(double)consumed;
}
void mixer_render(Mixer *m,OutputKind out,float *stereo,size_t frames,uint32_t render_rate) {
    if(!stereo||frames>SIZE_MAX/(2u*sizeof(float))) return;
    if(!m||(out!=SEND&&out!=MONITOR)||render_rate<8000||render_rate>192000) {
        memset(stereo,0,frames*2u*sizeof(float)); return;
    }
    double clip_step=(double)MIXER_BUS_RATE/render_rate;
    double correction=1.0+((double)m->mic_count-MIXER_MIC_TARGET)*0.0000025;
    if(correction<0.995) correction=0.995;
    if(correction>1.005) correction=1.005;
    if(out==SEND&&!m->mic_warm&&m->mic_count>=MIXER_MIC_TARGET) m->mic_warm=true;
    double mic_step=clip_step*correction;
    for(size_t i=0;i<frames;i++) {
        float clip_left,clip_right,mic_left=0,mic_right=0;
        read_clip(m,out,clip_step,&clip_left,&clip_right);
        if(out==SEND) read_mic(m,mic_step,&mic_left,&mic_right);
        float mic_gain=m->mic_muted?0:m->mic_gain;
        stereo[i*2u]=finite_sample(clip_left*m->sound_gain+mic_left*mic_gain);
        stereo[i*2u+1u]=finite_sample(clip_right*m->sound_gain+mic_right*mic_gain);
    }
}
size_t mixer_mic_queued_frames(const Mixer *m) { return m?m->mic_count:0; }
