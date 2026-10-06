#include "format.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static AudioFormat pcm(unsigned bits, unsigned valid, unsigned channels) {
    AudioFormat f = {48000, (uint16_t)channels, SAMPLE_PCM, (uint16_t)bits,
                     (uint16_t)valid, (uint16_t)(channels * bits / 8), 0};
    return f;
}
static void close_to(float got, float expected) { assert(fabsf(got - expected) < 0.000002f); }
static void test_endpoint_extrema(void) {
    char error[256]; float out[6];
    const unsigned char in16[] = {0,128, 0,0, 255,127};
    AudioFormat f = pcm(16,16,1); assert(format_validate(&f,error));
    format_decode_stereo(&f,in16,3,false,out);
    close_to(out[0],-1); close_to(out[2],0); close_to(out[4],32767.0f/32768.0f);
    const unsigned char in24[] = {0,0,128, 0,0,0, 255,255,127};
    f=pcm(24,24,1); format_decode_stereo(&f,in24,3,false,out);
    close_to(out[0],-1); close_to(out[2],0); close_to(out[4],8388607.0f/8388608.0f);
    const unsigned char in32[] = {0,0,0,128, 0,0,0,0, 255,255,255,127};
    f=pcm(32,32,1); format_decode_stereo(&f,in32,3,false,out);
    close_to(out[0],-1); close_to(out[2],0); close_to(out[4],1);
    /* 24 valid bits in a 32-bit container are left-aligned, low padding ignored. */
    const unsigned char ext[] = {255,0,0,128, 17,0,0,0, 255,255,255,127};
    f=pcm(32,24,1); format_decode_stereo(&f,ext,3,false,out);
    close_to(out[0],-1); close_to(out[2],0); close_to(out[4],8388607.0f/8388608.0f);
    unsigned char encoded[12]; const float samples[]={-1,-1,0,0,1,1};
    format_encode_stereo(&f,samples,3,encoded);
    const unsigned char expected[]={0,0,0,128,0,0,0,0,0,255,255,127};
    assert(memcmp(encoded,expected,sizeof expected)==0);
    f=pcm(24,24,1); format_encode_stereo(&f,samples,3,encoded);
    const unsigned char expected24[]={0,0,128,0,0,0,255,255,127};
    assert(memcmp(encoded,expected24,sizeof expected24)==0);
    f=pcm(16,16,1); format_encode_stereo(&f,samples,3,encoded);
    assert(memcmp(encoded,in16,sizeof in16)==0);
}
static void test_capture_downmix_and_silent(void) {
    AudioFormat f={48000,2,SAMPLE_FLOAT,32,32,8,3};
    float in[]={0.75f,-0.25f,NAN,INFINITY,-INFINITY,0.5f}; float out[6];
    format_decode_stereo(&f,in,3,false,out);
    close_to(out[0],0.25f); close_to(out[1],0.25f);
    close_to(out[2],0); close_to(out[4],0.25f);
    format_decode_stereo(&f,NULL,3,true,out);
    for(size_t i=0;i<6;i++) assert(out[i]==0);
    f.channels=6; f.block_align=24; f.channel_mask=0x3f;
    float surround[]={1,0,0,0,0,0};
    format_decode_stereo(&f,surround,1,false,out);
    close_to(out[0],1.0f/6); close_to(out[1],1.0f/6);
}
static void test_render_channel_mapping_and_finite(void) {
    AudioFormat f={48000,6,SAMPLE_FLOAT,32,32,24,0x3f};
    float in[]={0.8f,-0.4f}; float out[12];
    format_encode_stereo(&f,in,1,out);
    close_to(out[0],0.8f); close_to(out[1],-0.4f);
    for(size_t i=2;i<6;i++) assert(out[i]==0);
    f.channels=2; f.block_align=8; f.channel_mask=0x30; /* back pair */
    format_encode_stereo(&f,in,1,out); assert(out[0]==0&&out[1]==0);
    f.channels=1; f.block_align=4; f.channel_mask=4;
    format_encode_stereo(&f,in,1,out); close_to(out[0],0.2f);
    f.channels=2; f.block_align=8; f.channel_mask=3;
    float unsafe[]={NAN,INFINITY,-INFINITY,8};
    format_encode_stereo(&f,unsafe,2,out);
    assert(out[0]==0&&out[1]==0&&out[2]==0&&out[3]==1);
    f=pcm(32,32,2); unsigned char integers[16];
    format_encode_stereo(&f,unsafe,2,integers);
    const unsigned char expected[]={0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,127};
    assert(memcmp(integers,expected,sizeof expected)==0);
}
static void test_validation(void) {
    char error[256]; AudioFormat f=pcm(16,16,2);
    assert(format_validate(&f,error)); assert(error[0]=='\0');
    f.rate=7999; assert(!format_validate(&f,error)); assert(error[0]);
    f.rate=192001; assert(!format_validate(&f,error)); f.rate=8000; assert(format_validate(&f,error));
    f.rate=192000; assert(format_validate(&f,error)); f.channels=0; assert(!format_validate(&f,error));
    f.channels=9; assert(!format_validate(&f,error)); f=pcm(16,16,2);
    f.block_align=3; assert(!format_validate(&f,error)); f.block_align=4;
    f.valid_bits=0; assert(!format_validate(&f,error)); f.valid_bits=17; assert(!format_validate(&f,error));
    f=pcm(8,8,1); assert(!format_validate(&f,error)); f=pcm(16,16,2);
    f.kind=SAMPLE_FLOAT; assert(!format_validate(&f,error)); f=pcm(32,24,2); f.kind=SAMPLE_FLOAT;
    assert(!format_validate(&f,error)); f.valid_bits=32; assert(format_validate(&f,error));
    f.channel_mask=1; assert(!format_validate(&f,error)); f.channel_mask=UINT32_C(0x80000003);
    assert(!format_validate(&f,error)); assert(!format_validate(NULL,error));
}
int main(void) {
    test_endpoint_extrema(); test_capture_downmix_and_silent();
    test_render_channel_mapping_and_finite(); test_validation();
    puts("format: 4 groups passed (PCM/extensible/float, capture, speaker mapping, validation)");
}
