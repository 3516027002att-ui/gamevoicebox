#include "clip.h"
#include "../src/app/clip_catalog.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void u16(unsigned char *p,uint16_t x) { p[0]=(unsigned char)x; p[1]=(unsigned char)(x>>8); }
static void u32(unsigned char *p,uint32_t x) { for(unsigned i=0;i<4;i++) p[i]=(unsigned char)(x>>(8*i)); }
static size_t wave(unsigned char *b,bool odd) {
    memset(b,0,96); memcpy(b,"RIFF",4); memcpy(b+8,"WAVE",4); size_t p=12;
    if(odd) { memcpy(b+p,"JUNK",4); u32(b+p+4,1); b[p+8]=42; p+=10; }
    memcpy(b+p,"fmt ",4); u32(b+p+4,16); u16(b+p+8,1); u16(b+p+10,2);
    u32(b+p+12,48000); u32(b+p+16,192000); u16(b+p+20,4); u16(b+p+22,16); p+=24;
    memcpy(b+p,"data",4); u32(b+p+4,8); u16(b+p+8,0x8000); u16(b+p+10,0x7fff);
    u16(b+p+12,0); u16(b+p+14,0x4000); p+=16; u32(b+4,(uint32_t)(p-8)); return p;
}
static void write_fixture(const unsigned char *b,size_t len) {
    FILE *f=fopen("build/tests/clip-fixture.wav","wb"); assert(f);
    assert(fwrite(b,1,len,f)==len); assert(fclose(f)==0);
}
static void rejects(const unsigned char *b,size_t len) {
    write_fixture(b,len); Clip c={NULL,73}; char error[256];
    assert(!clip_load_wav("build/tests/clip-fixture.wav",&c,error));
    assert(error[0]&&c.samples==NULL&&c.frames==73);
}
static void test_complete_parse(void) {
    unsigned char b[96]; size_t len=wave(b,true); write_fixture(b,len);
    Clip c={0}; char error[256]; assert(clip_load_wav("build/tests/clip-fixture.wav",&c,error));
    assert(c.frames==2&&c.samples); assert(c.samples[0]==-1); assert(c.samples[1]==32767.0f/32768);
    assert(c.samples[2]==0&&c.samples[3]==0.5f); clip_free(&c); assert(!c.samples&&!c.frames);
    clip_free(&c); clip_free(NULL);
    /* Missing odd-chunk pad must not silently shift following chunks. */
    len=wave(b,true); memmove(b+21,b+22,len-22); u32(b+4,(uint32_t)(len-9)); rejects(b,len-1);
}
static void test_malformed_bounds(void) {
    unsigned char b[96]; size_t len=wave(b,false); u32(b+4,(uint32_t)(len-9)); rejects(b,len);
    len=wave(b,false); u32(b+4,(uint32_t)(len-7)); rejects(b,len);
    len=wave(b,false); rejects(b,len-1);
    len=wave(b,false); u32(b+40,UINT32_MAX); rejects(b,len);
    len=wave(b,false); u32(b+16,UINT32_MAX); rejects(b,len);
    len=wave(b,false); memcpy(b+36,"JUNK",4); rejects(b,len);
    len=wave(b,false); memcpy(b+12,"JUNK",4); rejects(b,len);
    len=wave(b,false); u16(b+32,3); rejects(b,len);
    len=wave(b,false); u32(b+28,1); rejects(b,len);
    len=wave(b,false); u32(b+24,44100); rejects(b,len);
    len=wave(b,false); u16(b+22,1); rejects(b,len);
    len=wave(b,false); u16(b+34,24); rejects(b,len);
    len=wave(b,false); u16(b+20,3); rejects(b,len);
    len=wave(b,false); u32(b+40,7); u32(b+4,(uint32_t)(len-9)); rejects(b,len-1);
    len=wave(b,false); b[len]=0; rejects(b,len+1);
    Clip c={0}; char error[256]; assert(!clip_load_wav("no-such-file.wav",&c,error));
    assert(!clip_load_wav(NULL,&c,error)); assert(!clip_load_wav("x",NULL,error));
}
static void test_format_extension_integrity(void) {
    unsigned char b[96]; size_t len=wave(b,false);
    memmove(b+38,b+36,len-36); b[36]=0; b[37]=0; u32(b+16,18); u32(b+4,(uint32_t)(len-6));
    write_fixture(b,len+2); Clip c={0}; char error[256];
    assert(clip_load_wav("build/tests/clip-fixture.wav",&c,error)); clip_free(&c);
    b[36]=1; rejects(b,len+2); /* extension claims a byte that is absent */
    b[36]=0; u32(b+16,17); rejects(b,len+2); /* half cbSize, even padded */
    len=wave(b,false); memcpy(b+len,b+36,16); len+=16; u32(b+4,(uint32_t)(len-8)); rejects(b,len);
    len=wave(b,false); memcpy(b+len,b+12,24); len+=24; u32(b+4,(uint32_t)(len-8)); rejects(b,len);
}
static void test_utf8_filename(void) {
    unsigned char b[96]; size_t len=wave(b,false); write_fixture(b,len);
    const char *unicode="build/tests/声音（中文）.wav";
    assert(rename("build/tests/clip-fixture.wav",unicode)==0);
    Clip c={0}; char error[256]; assert(clip_load_wav(unicode,&c,error)); assert(c.frames==2);
    clip_free(&c); assert(remove(unicode)==0);
#ifndef _WIN32
    /* POSIX can name a real file with invalid bytes; the UTF-8 API rejects it. */
    const char invalid[]="build/tests/\xc0\xaf.wav"; FILE *f=fopen(invalid,"wb"); assert(f);
    assert(fwrite(b,1,len,f)==len); assert(fclose(f)==0);
    assert(!clip_load_wav(invalid,&c,error)); assert(error[0]&&!c.samples); assert(remove(invalid)==0);
#endif
}
static void test_real_assets(void) {
    const size_t original_frames[]={81503,37617,124134};
    for(int i=0;i<CLIP_COUNT;i++) { Clip c={0}; char error[256],name[128];
        snprintf(name,sizeof name,"assets/%s.wav",clip_catalog_id(i));
        if(!clip_load_wav(name,&c,error)) { fprintf(stderr,"asset %s: %s\n",name,error); assert(0); }
        assert(c.frames>0 && c.samples); if(i<3) assert(c.frames==original_frames[i]); clip_free(&c);
    }
}
int main(void) { test_complete_parse(); test_malformed_bounds(); test_format_extension_integrity(); test_utf8_filename(); test_real_assets();
    remove("build/tests/clip-fixture.wav"); puts("clip: 5 groups passed (complete chunks/extensions, malformed bounds, UTF8 paths, 14 full assets)"); }
