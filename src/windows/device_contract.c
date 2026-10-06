#include "device_contract.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
static bool contains_ci(const char *text,const char *word) {
    size_t n=strlen(word), i,j;
    for(i=0;text[i];++i) {
        for(j=0;j<n && text[i+j] && tolower((unsigned char)text[i+j])==tolower((unsigned char)word[j]);++j) {}
        if(j==n) return true;
    }
    return false;
}
static bool equals_ci(const char *a,const char *b) {
    size_t i=0;
    if(!a || !b) return false;
    while(a[i] && b[i] && tolower((unsigned char)a[i])==tolower((unsigned char)b[i])) ++i;
    return !a[i] && !b[i];
}
CableClass endpoint_classify_metadata(const char *description,const char *adapter) {
    /* Endpoint FriendlyName is editable and deliberately not an argument.
       Required MMDevice description + audio adapter metadata fail closed. */
    if(!description || !description[0] || !adapter || !adapter[0]) return CABLE_UNVERIFIED;
    if(equals_ci(adapter,"VB-Audio Virtual Cable")) {
        return equals_ci(description,"CABLE Input")?CABLE_STANDARD_INPUT:CABLE_OTHER;
    }
    if(contains_ci(adapter,"VB-Audio") && contains_ci(adapter,"Cable")) return CABLE_OTHER;
    return CABLE_NONE;
}
char *endpoint_display_label(const char *name,const char *id,bool duplicate) {
    char *label; size_t len,i; uint64_t hash=UINT64_C(14695981039346656037);
    if(!name || !id) return NULL;
    len=strlen(name);
    if(len>SIZE_MAX-40) return NULL;
    label=malloc(len+40); if(!label) return NULL;
    memcpy(label,name,len+1);
    if(duplicate) {
        for(i=0;id[i];++i) { hash^=(unsigned char)id[i]; hash*=UINT64_C(1099511628211); }
        snprintf(label+len,40," [device %016" PRIx64 "]",hash);
    }
    return label;
}
bool endpoint_validate(const EndpointView *e,EndpointRole role,char error[256]) {
    const char *why=NULL;
    if(!e || !e->id || !e->id[0] || !e->name) why="Selected endpoint is missing";
    else if(e->cable==CABLE_UNVERIFIED) why="Endpoint driver identification is unavailable; selection rejected for safety";
    else if(!e->active) why="Selected endpoint is inactive";
    else if(role==ROLE_MIC && e->direction!=DEVICE_CAPTURE) why="Microphone must be a capture endpoint";
    else if(role!=ROLE_MIC && e->direction!=DEVICE_RENDER) why="Output must be a render endpoint";
    else if(role==ROLE_SEND && e->cable!=CABLE_STANDARD_INPUT) why="Send output must be standard CABLE Input (VB-Audio Virtual Cable)";
    else if(role!=ROLE_SEND && e->cable!=CABLE_NONE) why="Microphone and preview must use real non-cable devices";
    else if(role!=ROLE_MIC && role!=ROLE_SEND && role!=ROLE_MONITOR) why="Invalid endpoint role";
    if(why) { snprintf(error,256,"%s",why); return false; }
    error[0]=0; return true;
}
