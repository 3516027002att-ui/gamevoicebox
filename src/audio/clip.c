#include "clip.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static bool fail(char error[256],const char *why) { if(error) snprintf(error,256,"%s",why); return false; }
static bool valid_utf8(const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    while(*p) {
        uint32_t value; unsigned more; uint32_t minimum;
        if(*p<0x80) { p++; continue; }
        if(*p>=0xc2&&*p<=0xdf) { value=*p&0x1f; more=1; minimum=0x80; }
        else if(*p>=0xe0&&*p<=0xef) { value=*p&0x0f; more=2; minimum=0x800; }
        else if(*p>=0xf0&&*p<=0xf4) { value=*p&7; more=3; minimum=0x10000; }
        else return false;
        p++;
        for(unsigned i=0;i<more;i++) { if(*p<0x80||*p>0xbf) return false; value=(value<<6)|(*p&0x3f); p++; }
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff)) return false;
    }
    return true;
}
static FILE *open_utf8(const char *path) {
#ifdef _WIN32
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    if(count<=0||(size_t)count>SIZE_MAX/sizeof(wchar_t)) return NULL;
    wchar_t *wide=malloc((size_t)count*sizeof *wide); if(!wide) return NULL;
    FILE *file=NULL;
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,count)==count) file=_wfopen(wide,L"rb");
    free(wide); return file;
#else
    return fopen(path,"rb");
#endif
}
void clip_free(Clip *clip) { if(clip) { free(clip->samples); clip->samples=NULL; clip->frames=0; } }
bool clip_load_wav(const char *path,Clip *out,char error[256]) {
    if(error) error[0]='\0';
    if(!path||!out) return fail(error,"Missing clip path or output");
    if(!valid_utf8(path)) return fail(error,"WAV path is not valid UTF-8");
    FILE *file=open_utf8(path); if(!file) return fail(error,"Could not open WAV file");
    const char *why="Invalid WAV file"; Clip loaded={0}; bool ok=false;
    if(fseek(file,0,SEEK_END)!=0) { why="Could not measure WAV file"; goto done; }
    long length=ftell(file);
    if(length<12||fseek(file,0,SEEK_SET)!=0) { why="Truncated WAV header"; goto done; }
    unsigned char header[16];
    if(fread(header,1,12,file)!=12||memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4)) { why="Not a RIFF/WAVE file"; goto done; }
    uint64_t end=(uint64_t)le32(header+4)+8u;
    if(end!=(uint64_t)length) { why="RIFF length does not match complete file"; goto done; }
    uint64_t pos=12,data_offset=0; uint32_t data_bytes=0; bool have_fmt=false,have_data=false;
    while(pos<end) {
        if(end-pos<8u||pos>(uint64_t)LONG_MAX||fseek(file,(long)pos,SEEK_SET)!=0||fread(header,1,8,file)!=8) {
            why="Truncated WAV chunk header"; goto done;
        }
        uint32_t bytes=le32(header+4); uint64_t payload=pos+8u;
        uint64_t padded=(uint64_t)bytes+(bytes&1u);
        if(padded>end-payload) { why="Truncated or overflowing WAV chunk (including padding)"; goto done; }
        if(!memcmp(header,"fmt ",4)) {
            if(have_fmt||bytes<16u||fread(header,1,16,file)!=16) { why="Missing or duplicate WAV format"; goto done; }
            if(le16(header)!=1||le16(header+2)!=2||le32(header+4)!=48000||le32(header+8)!=192000||le16(header+12)!=4||le16(header+14)!=16) {
                why="Clip must be PCM16 / 48000 Hz / stereo with valid byte rate and block alignment"; goto done;
            }
            if(bytes!=16u) {
                unsigned char extension[2];
                if(bytes<18u||fread(extension,1,2,file)!=2||(uint32_t)le16(extension)!=bytes-18u) {
                    why="Malformed PCM format extension length"; goto done;
                }
            }
            have_fmt=true;
        } else if(!memcmp(header,"data",4)) {
            if(have_data||!bytes||bytes%4u) { why="Duplicate, empty or misaligned WAV data"; goto done; }
            have_data=true; data_offset=payload; data_bytes=bytes;
        }
        pos=payload+padded;
    }
    if(!have_fmt||!have_data) { why="WAV requires format and data chunks"; goto done; }
    loaded.frames=data_bytes/4u;
    if(loaded.frames>SIZE_MAX/(2u*sizeof(float))) { why="WAV sample allocation overflows size_t"; goto done; }
    loaded.samples=malloc(loaded.frames*2u*sizeof(float));
    if(!loaded.samples) { why="Could not allocate WAV samples"; goto done; }
    if(data_offset>(uint64_t)LONG_MAX||fseek(file,(long)data_offset,SEEK_SET)!=0) { why="Could not seek to WAV samples"; goto done; }
    unsigned char block[4096]; size_t sample=0; uint32_t remaining=data_bytes;
    while(remaining) {
        size_t bytes=remaining<sizeof block?remaining:sizeof block;
        if(fread(block,1,bytes,file)!=bytes) { why="WAV sample data changed or was truncated"; goto done; }
        for(size_t i=0;i<bytes;i+=2) {
            uint16_t raw=le16(block+i); int32_t value=raw>=32768u?(int32_t)raw-65536:(int32_t)raw;
            loaded.samples[sample++]=(float)value/32768.0f;
        }
        remaining-=(uint32_t)bytes;
    }
    ok=true;
done:
    if(fclose(file)!=0&&ok) { why="Could not close WAV file"; ok=false; }
    if(!ok) { clip_free(&loaded); return fail(error,why); }
    *out=loaded; return true;
}
