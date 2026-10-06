#include "format.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(float)==4,"32-bit float required");
static float safe(float x) { if(!isfinite(x)) return 0; return x>1?1:(x< -1?-1:x); }
static bool fail(char error[256],const char *why) { if(error) snprintf(error,256,"%s",why); return false; }
bool format_validate(const AudioFormat *f,char error[256]) {
    if(error) error[0]='\0';
    if(!f) return fail(error,"Missing endpoint format");
    if(f->rate<8000||f->rate>192000) return fail(error,"Unsupported sample rate (8–192 kHz required)");
    if(f->channels<1||f->channels>8) return fail(error,"Unsupported channel count (1–8 required)");
    if(f->kind==SAMPLE_PCM) {
        if(f->container_bits!=16&&f->container_bits!=24&&f->container_bits!=32) return fail(error,"Unsupported PCM container size");
        if(!f->valid_bits||f->valid_bits>f->container_bits) return fail(error,"Invalid PCM valid bits");
    } else if(f->kind==SAMPLE_FLOAT) {
        if(f->container_bits!=32||f->valid_bits!=32) return fail(error,"Only float32 endpoint samples are supported");
    } else return fail(error,"Unsupported sample kind");
    if(f->block_align!=f->channels*(f->container_bits/8u)) return fail(error,"Invalid block alignment");
    if(f->channel_mask) {
        if(f->channel_mask&~UINT32_C(0x3ffff)) return fail(error,"Unknown speaker-mask bits");
        unsigned count=0; uint32_t mask=f->channel_mask;
        while(mask) { count+=mask&1u; mask>>=1; }
        if(count!=f->channels) return fail(error,"Speaker mask does not match channel count");
    }
    return true;
}
static uint32_t read_le(const unsigned char *p,unsigned bytes) {
    uint32_t v=0; for(unsigned i=0;i<bytes;i++) v|=(uint32_t)p[i]<<(8*i); return v;
}
static void write_le(unsigned char *p,unsigned bytes,uint32_t v) {
    for(unsigned i=0;i<bytes;i++) p[i]=(unsigned char)(v>>(8*i));
}
static float decode(const AudioFormat *f,const unsigned char *p) {
    uint32_t raw=read_le(p,f->container_bits/8u);
    if(f->kind==SAMPLE_FLOAT) { float value; memcpy(&value,&raw,4); return safe(value); }
    raw>>=f->container_bits-f->valid_bits;
    uint64_t sign=UINT64_C(1)<<(f->valid_bits-1u);
    int64_t value=(int64_t)raw;
    if((uint64_t)raw>=sign) value-=(int64_t)(sign*2u);
    return (float)((double)value/(double)sign);
}
void format_decode_stereo(const AudioFormat *f,const void *src,size_t frames,bool silent,float *dst) {
    if(!dst||frames>SIZE_MAX/(2u*sizeof(float))) return;
    if(silent||!src||!format_validate(f,NULL)||frames>SIZE_MAX/f->block_align) {
        memset(dst,0,frames*2u*sizeof(float)); return;
    }
    const unsigned char *p=src; unsigned bytes=f->container_bits/8u;
    for(size_t i=0;i<frames;i++) {
        double sum=0; for(unsigned c=0;c<f->channels;c++) sum+=decode(f,p+i*f->block_align+c*bytes);
        float mono=safe((float)(sum/f->channels)); dst[i*2u]=mono; dst[i*2u+1u]=mono;
    }
}
static void encode(const AudioFormat *f,float value,unsigned char *p) {
    value=safe(value);
    if(f->kind==SAMPLE_FLOAT) { uint32_t raw; memcpy(&raw,&value,4); write_le(p,4,raw); return; }
    uint64_t sign=UINT64_C(1)<<(f->valid_bits-1u);
    double scaled=(double)value*(double)sign;
    int64_t sample;
    if(scaled>=(double)(sign-1u)) sample=(int64_t)(sign-1u);
    else if(scaled<= -(double)sign) sample= -(int64_t)sign;
    else sample=(int64_t)llround(scaled);
    uint64_t raw=((uint64_t)sample&(sign*2u-1u))<<(f->container_bits-f->valid_bits);
    write_le(p,f->container_bits/8u,(uint32_t)raw);
}
void format_encode_stereo(const AudioFormat *f,const float *src,size_t frames,void *dst) {
    if(!dst||!src||!format_validate(f,NULL)||frames>SIZE_MAX/f->block_align||frames>SIZE_MAX/2u) return;
    unsigned char *p=dst; unsigned bytes=f->container_bits/8u;
    for(size_t i=0;i<frames;i++) {
        float left=safe(src[i*2u]),right=safe(src[i*2u+1u]); uint32_t remaining=f->channel_mask;
        for(unsigned c=0;c<f->channels;c++) {
            float value=0;
            if(f->channels==1) value=(left+right)*0.5f;
            else if(!f->channel_mask) value=c==0?left:(c==1?right:0);
            else {
                uint32_t speaker=remaining&(~remaining+1u); remaining&=remaining-1u;
                if(speaker==AUDIO_SPEAKER_FRONT_LEFT) value=left;
                if(speaker==AUDIO_SPEAKER_FRONT_RIGHT) value=right;
            }
            encode(f,value,p+i*f->block_align+c*bytes);
        }
    }
}
