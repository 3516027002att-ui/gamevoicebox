#include "mixer.h"
#include "format.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static float first[]={0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,0.8f};
static float second[]={-0.2f,-0.4f,-0.6f,-0.8f};
static float third[]={1,1,1,1,1,1};
static Clip clips[CLIP_COUNT]={{first,4},{second,2},{third,3}};
static void near(float a,float b) { assert(fabsf(a-b)<0.00001f); }
static void silence(const float *a,size_t frames) { for(size_t i=0;i<frames*2;i++) assert(a[i]==0); }
static void fill(float *p,size_t n,float value) { for(size_t i=0;i<n*2;i++) p[i]=value; }
static void test_independent_cursors_and_replacement(void) {
    Mixer m; mixer_init(&m,clips); float out[16]; assert(m.sound_gain==0.8f&&m.mic_gain==1);
    mixer_play(&m,0,true,true); mixer_render(&m,SEND,out,2,48000); near(out[0],0.08f); near(out[2],0.24f);
    mixer_render(&m,MONITOR,out,1,48000); near(out[0],0.08f);
    mixer_render(&m,SEND,out,3,48000); near(out[0],0.4f); near(out[2],0.56f); silence(out+4,1);
    mixer_render(&m,MONITOR,out,3,48000); near(out[0],0.24f); near(out[4],0.56f);
    mixer_play(&m,1,true,false); mixer_render(&m,SEND,out,1,48000); near(out[0],-0.16f);
    mixer_render(&m,MONITOR,out,1,48000); silence(out,1);
    mixer_play(&m,1,true,true); mixer_render(&m,SEND,out,1,48000); near(out[0],-0.16f);
    mixer_play(&m,0,false,false); mixer_render(&m,SEND,out,4,48000); silence(out,4);
    mixer_render(&m,MONITOR,out,4,48000); silence(out,4);
    mixer_play(&m,0,true,true); mixer_stop_clip(&m); mixer_render(&m,SEND,out,4,48000); silence(out,4);
}
static void test_fourteenth_clip_and_index_bounds(void) {
    Clip full[CLIP_COUNT]={{0}}; full[13]=(Clip){first,4};
    Mixer m; float out[8]; mixer_init(&m,full); mixer_set_gains(&m,1,0,false);
    mixer_play(&m,13,true,true); mixer_render(&m,SEND,out,1,48000); near(out[0],0.1f); near(out[1],0.2f);
    mixer_render(&m,MONITOR,out,1,48000); near(out[0],0.1f);
    mixer_play(&m,14,true,true); mixer_render(&m,SEND,out,4,48000); silence(out,4);
    mixer_play(&m,-1,true,true); mixer_render(&m,MONITOR,out,4,48000); silence(out,4);
}
static void test_gains_mute_monitor_and_stop(void) {
    Mixer m; mixer_init(&m,clips); float mic[4000],out[16]; fill(mic,2000,0.5f);
    mixer_push_mic(&m,mic,2000,48000,false); assert(mixer_mic_queued_frames(&m)>=1920);
    size_t queued=mixer_mic_queued_frames(&m); mixer_render(&m,MONITOR,out,4,48000); silence(out,4);
    assert(mixer_mic_queued_frames(&m)==queued);
    mixer_play(&m,2,true,true); mixer_set_gains(&m,1,1,false); mixer_render(&m,SEND,out,4,48000);
    for(size_t i=0;i<6;i++) assert(out[i]==1);
    near(out[6],0.5f);
    mixer_play(&m,2,true,true); mixer_set_gains(&m,0,0.8f,false); mixer_render(&m,SEND,out,1,48000); near(out[0],0.4f);
    mixer_set_gains(&m,0,0.8f,true); mixer_render(&m,SEND,out,1,48000); silence(out,1); assert(m.mic_gain==0.8f);
    mixer_set_gains(&m,0,0.8f,false); mixer_stop_clip(&m); mixer_render(&m,SEND,out,1,48000); near(out[0],0.4f);
    mixer_set_gains(&m,1,0,false); mixer_play(&m,2,true,true); mixer_render(&m,SEND,out,1,48000); near(out[0],1);
    mixer_set_gains(&m,NAN,INFINITY,false); mixer_render(&m,SEND,out,1,48000); assert(isfinite(out[0]));
    mixer_reset(&m); assert(mixer_mic_queued_frames(&m)==0); mixer_render(&m,SEND,out,4,48000); silence(out,4);
}
static void test_warmup_underflow_overflow_discontinuity(void) {
    Mixer m; mixer_init(&m,clips); float mic[12000],out[12000]; fill(mic,6000,0.25f);
    mixer_push_mic(&m,mic,1000,48000,false); mixer_render(&m,SEND,out,100,48000); silence(out,100);
    mixer_push_mic(&m,mic,1000,48000,false); mixer_render(&m,SEND,out,1,48000); near(out[0],0.25f);
    mixer_render(&m,SEND,out,6000,48000); assert(mixer_mic_queued_frames(&m)==0); silence(out+5000*2,1000);
    mixer_push_mic(&m,mic,1000,48000,false); mixer_render(&m,SEND,out,100,48000); silence(out,100);
    mixer_push_mic(&m,mic,6000,48000,true); assert(mixer_mic_queued_frames(&m)==4800);
    mixer_render(&m,SEND,out,100,48000); near(out[0],0.25f);
    mixer_push_mic(&m,mic,1,48000,true); assert(mixer_mic_queued_frames(&m)<=1);
    mixer_render(&m,SEND,out,100,48000); silence(out,100);
}
static void test_per_output_clear(void) {
    Mixer m; mixer_init(&m,clips); float out[8],mic[4000]; fill(mic,2000,0.5f);
    mixer_push_mic(&m,mic,2000,48000,false); mixer_play(&m,0,true,true);
    mixer_render(&m,MONITOR,out,1,48000); mixer_clear_send(&m);
    assert(mixer_mic_queued_frames(&m)==0);
    mixer_render(&m,SEND,out,2,48000); silence(out,2);
    mixer_render(&m,MONITOR,out,1,48000); near(out[0],0.24f);
    mixer_push_mic(&m,mic,2000,48000,false); mixer_play(&m,0,true,true);
    mixer_clear_monitor(&m); size_t queued=mixer_mic_queued_frames(&m);
    mixer_render(&m,MONITOR,out,2,48000); silence(out,2);
    assert(mixer_mic_queued_frames(&m)==queued);
    mixer_render(&m,SEND,out,1,48000); near(out[0],0.58f);
}

static void test_capture_packet_continuity_and_rates(void) {
    const uint32_t rates[]={44100,48000,96000}; float input[4400];
    for(size_t i=0;i<2200;i++) { input[i*2]=(float)i/4400; input[i*2+1]=-(float)i/4400; }
    for(size_t r=0;r<3;r++) {
        Mixer whole,split; mixer_init(&whole,clips); mixer_init(&split,clips);
        mixer_push_mic(&whole,input,2200,rates[r],false);
        size_t pos=0; const size_t blocks[]={1,7,219,503,3,1467};
        for(size_t i=0;i<sizeof blocks/sizeof blocks[0];i++) {
            mixer_push_mic(&split,input+pos*2,blocks[i],rates[r],false); pos+=blocks[i];
        }
        assert(pos==2200); assert(whole.mic_count==split.mic_count);
        assert(whole.capture_next==split.capture_next);
        assert(memcmp(whole.mic_queue,split.mic_queue,sizeof whole.mic_queue)==0);
        size_t expected=(size_t)floor(2199.0*48000/rates[r])+1;
        assert(whole.mic_count==expected||whole.mic_count+1==expected);
        /* Bus positions represent linear interpolation in the source clock. */
        for(size_t i=0;i<whole.mic_count;i++) {
            near(whole.mic_queue[i*2],(float)((double)i*rates[r]/48000/4400));
            near(whole.mic_queue[i*2+1],-whole.mic_queue[i*2]);
        }
    }
}
static void test_output_resampling_and_block_continuity(void) {
    float ramp[40]; for(size_t i=0;i<20;i++) { ramp[i*2]=(float)i/40; ramp[i*2+1]=-(float)i/40; }
    Clip ramps[CLIP_COUNT]={{ramp,20},{second,2},{third,3}}; const uint32_t rates[]={44100,48000,96000};
    for(size_t r=0;r<3;r++) {
        Mixer one,many; mixer_init(&one,ramps); mixer_init(&many,ramps);
        mixer_set_gains(&one,1,0,false); mixer_set_gains(&many,1,0,false);
        mixer_play(&one,0,true,true); mixer_play(&many,0,true,true);
        float a[100],b[100]; mixer_render(&one,SEND,a,50,rates[r]);
        mixer_render(&many,SEND,b,7,rates[r]); mixer_render(&many,SEND,b+14,19,rates[r]);
        mixer_render(&many,SEND,b+52,24,rates[r]); assert(memcmp(a,b,sizeof a)==0);
        for(size_t i=0;i<50;i++) {
            double phase=(double)i*48000/rates[r];
            float value=phase>=20?0:(phase>19?ramp[38]:(float)(phase/40));
            near(a[i*2],value); near(a[i*2+1],-value);
        }
    }
    Mixer m; mixer_init(&m,ramps); mixer_set_gains(&m,1,0,false); mixer_play(&m,0,true,true);
    float send[40],monitor[80]; mixer_render(&m,SEND,send,20,48000); mixer_render(&m,MONITOR,monitor,40,96000);
    for(size_t i=0;i<20;i++) { near(send[i*2],monitor[i*4]); near(send[i*2+1],monitor[i*4+1]); }
}
static void test_microphone_combined_endpoint_rates(void) {
    const uint32_t rates[]={44100,48000,96000}; float input[8192],output[1920]; fill(input,4096,0.25f);
    for(size_t capture=0;capture<3;capture++) for(size_t render=0;render<3;render++) {
        Mixer m; mixer_init(&m,clips);
        mixer_push_mic(&m,input,(size_t)rates[capture]*40/1000,rates[capture],false);
        for(size_t block=0;block<1000;block++) {
            mixer_push_mic(&m,input,rates[capture]/100,rates[capture],false);
            mixer_render(&m,SEND,output,rates[render]/100,rates[render]);
            assert(mixer_mic_queued_frames(&m)<=4800);
            for(size_t i=0;i<(size_t)rates[render]/100*2;i++) near(output[i],0.25f);
        }
    }
}
static void test_pcm_validation_edges_and_invalid_calls(void) {
    Mixer m; mixer_init(&m,clips); float samples[]={NAN,INFINITY,-INFINITY,2},out[8];
    mixer_push_mic(&m,samples,2,48000,false);
    assert(m.mic_queue[0]==0&&m.mic_queue[1]==0&&m.mic_queue[2]==0&&m.mic_queue[3]==1);
    mixer_push_mic(&m,samples,2,7999,false); assert(mixer_mic_queued_frames(&m)==0);
    mixer_render(&m,SEND,out,4,7999); silence(out,4);
    mixer_play(&m,99,true,true); mixer_render(&m,SEND,out,4,48000); silence(out,4);
    mixer_render(&m,(OutputKind)9,out,4,48000); silence(out,4);
}
static void test_thirty_minute_clock_drift(int ppm) {
    Mixer m; mixer_init(&m,clips); float input[4000],output[960]; fill(input,2000,0.25f);
    mixer_push_mic(&m,input,1920,48000,false);
    double fraction=0,early=0,late=0; size_t early_n=0,late_n=0,min=4800,max=0;
    for(size_t block=0;block<180000;block++) { /* 1800 seconds, 10 ms per block */
        fraction+=480.0*(1.0+(double)ppm/1000000.0);
        size_t capture=(size_t)floor(fraction); fraction-=capture;
        mixer_push_mic(&m,input,capture,48000,false);
        assert(mixer_mic_queued_frames(&m)<=4800);
        mixer_render(&m,SEND,output,480,48000);
        size_t q=mixer_mic_queued_frames(&m); assert(q<=4800);
        if(block>=5000) { if(q<min) min=q; if(q>max) max=q; }
        if(block>=5000&&block<6000) { early+=q; early_n++; }
        if(block>=179000) { late+=q; late_n++; }
        for(size_t i=0;i<960;i++) { assert(isfinite(output[i])); near(output[i],0.25f); }
    }
    assert(min>1000&&max<2000); assert(fabs(early/early_n-late/late_n)<3.0);
    printf("drift %+d ppm: 30 simulated minutes, queue %zu..%zu, first/last stable mean %.2f/%.2f frames\n",
           ppm,min,max,early/early_n,late/late_n);
}
static void put16(FILE *f,uint16_t value) { fputc(value&255,f); fputc(value>>8,f); }
static void put32(FILE *f,uint32_t value) { for(unsigned i=0;i<4;i++) fputc((int)((value>>(8*i))&255),f); }
static void wav_header(FILE *f,uint32_t frames) {
    fwrite("RIFF",1,4,f); put32(f,frames*4+36); fwrite("WAVEfmt ",1,8,f); put32(f,16);
    put16(f,1); put16(f,2); put32(f,48000); put32(f,192000); put16(f,4); put16(f,16);
    fwrite("data",1,4,f); put32(f,frames*4);
}
static void test_generate_same_code_verification_wave(void) {
    Clip real[CLIP_COUNT]={{0}}; char error[256],path[128];
    size_t total=0; for(size_t i=0;i<CLIP_COUNT;i++) { snprintf(path,sizeof path,"assets/%s.wav",clip_catalog_id((int)i)); assert(clip_load_wav(path,&real[i],error)); total+=real[i].frames+4800; }
    FILE *send=fopen("build/tests/mixer-send-verification.wav","wb"),*monitor=fopen("build/tests/mixer-monitor-verification.wav","wb");
    assert(send&&monitor); wav_header(send,(uint32_t)total); wav_header(monitor,(uint32_t)total);
    Mixer m; mixer_init(&m,real); float mic[4000],a[960],b[960]; unsigned char encoded[1920]; fill(mic,2000,0.125f);
    mixer_push_mic(&m,mic,1920,48000,false); AudioFormat pcm={48000,2,SAMPLE_PCM,16,16,4,3};
    size_t nonzero_mic_tail=0;
    for(size_t clip=0;clip<CLIP_COUNT;clip++) {
        mixer_play(&m,(int)clip,true,true); size_t elapsed=0,frames=real[clip].frames+4800;
        while(elapsed<frames) {
            size_t count=frames-elapsed<480?frames-elapsed:480;
            mixer_push_mic(&m,mic,count,48000,false); size_t queued=mixer_mic_queued_frames(&m);
            mixer_render(&m,MONITOR,b,count,48000); assert(mixer_mic_queued_frames(&m)==queued);
            mixer_render(&m,SEND,a,count,48000);
            for(size_t j=0;j<count;j++) {
                if(elapsed+j>=real[clip].frames) { assert(b[j*2]==0&&b[j*2+1]==0); near(a[j*2],0.125f); nonzero_mic_tail++; }
                else { near(b[j*2],real[clip].samples[(elapsed+j)*2]*0.8f); near(b[j*2+1],real[clip].samples[(elapsed+j)*2+1]*0.8f); }
            }
            format_encode_stereo(&pcm,a,count,encoded); assert(fwrite(encoded,4,count,send)==count);
            format_encode_stereo(&pcm,b,count,encoded); assert(fwrite(encoded,4,count,monitor)==count);
            elapsed+=count;
        }
    }
    assert(nonzero_mic_tail==67200); assert(fclose(send)==0&&fclose(monitor)==0);
    for(size_t i=0;i<CLIP_COUNT;i++) clip_free(&real[i]);
    printf("verification WAV: %zu frames, monitor mic leakage = 0 (67200 tail frames)\n",total);
}

int main(void) {
    test_fourteenth_clip_and_index_bounds(); test_independent_cursors_and_replacement(); test_gains_mute_monitor_and_stop();
    test_warmup_underflow_overflow_discontinuity(); test_per_output_clear();
    test_capture_packet_continuity_and_rates(); test_output_resampling_and_block_continuity();
    test_microphone_combined_endpoint_rates(); test_pcm_validation_edges_and_invalid_calls(); test_thirty_minute_clock_drift(500);
    test_thirty_minute_clock_drift(-500); test_generate_same_code_verification_wave();
    puts("mixer: 12 groups passed (playback/gains/bounds/rates/isolation, drift, verification WAV)");
}
