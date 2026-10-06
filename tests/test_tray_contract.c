/* Production tray lifecycle policy; platform calls are injected, not Windows execution. */
#include "../src/windows/tray_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { bool add_ok; char calls[80]; unsigned n; } Probe;
static void call(Probe *p,char c) { p->calls[p->n++]=c; p->calls[p->n]=0; }
static bool add(void *p) { Probe *s=p; call(s,'A'); return s->add_ok; }
static void remove_icon(void *p) { call(p,'D'); }
static void hide(void *p) { call(p,'H'); }
static void restore(void *p) { call(p,'R'); }
static void reset(Probe *p) { p->n=0; p->calls[0]=0; }
int main(void) {
 Probe p={.add_ok=true}; TrayState s={0}; TrayOps ops={&p,add,remove_icon,hide,restore};
 assert(tray_minimize(&s,&ops)); assert(!strcmp(p.calls,"AH")); assert(s.installed && s.hidden);
 reset(&p); assert(tray_minimize(&s,&ops)); assert(!strcmp(p.calls,"H"));
 reset(&p); tray_restore(&s,&ops); assert(!strcmp(p.calls,"R") && !s.hidden && s.installed);
 reset(&p); assert(tray_recover(&s,&ops)); assert(!strcmp(p.calls,"A") && s.installed && !s.hidden);
 reset(&p); assert(tray_minimize(&s,&ops)); reset(&p); assert(tray_recover(&s,&ops)); assert(!strcmp(p.calls,"A") && s.hidden);
 reset(&p); p.add_ok=false; assert(!tray_recover(&s,&ops)); assert(!strcmp(p.calls,"AR") && !s.installed && !s.hidden);
 reset(&p); assert(!tray_minimize(&s,&ops)); assert(!strcmp(p.calls,"AR") && !s.installed && !s.hidden);
 reset(&p); p.add_ok=true; assert(tray_minimize(&s,&ops)); reset(&p); tray_remove(&s,&ops); assert(!strcmp(p.calls,"D") && !s.installed && !s.hidden);
 reset(&p); tray_remove(&s,&ops); assert(!p.n); assert(!tray_minimize(NULL,&ops)); assert(!tray_recover(NULL,&ops));
 puts("tray lifecycle contract: minimize, restore, failures, Explorer recovery and cleanup passed");
 return 0;
}
