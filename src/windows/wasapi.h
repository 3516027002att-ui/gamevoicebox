#ifndef RONGYU_WINDOWS_WASAPI_H
#define RONGYU_WINDOWS_WASAPI_H
#include "../app/controller.h"
typedef void (*AudioStatusCallback)(void *ctx,const ControllerState *state);
typedef struct { void *impl; } AudioWorker;
bool audio_worker_start(AudioWorker *out,AudioStatusCallback cb,void *ctx,char error[256]);
bool audio_worker_submit(AudioWorker *worker,const Command *command,char error[256]);
void audio_worker_shutdown(AudioWorker *worker);
#endif
