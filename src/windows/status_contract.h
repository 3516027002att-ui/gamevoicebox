#ifndef RONGYU_STATUS_CONTRACT_H
#define RONGYU_STATUS_CONTRACT_H
#include "../app/controller.h"
#include "device_contract.h"
/* Durable per-domain POD results survive UI callback/mailbox coalescing. */
typedef struct { ControllerState state; bool posted; } AudioStatusMailbox;
/* Caller holds its mailbox lock around these operations. */
bool audio_mailbox_write(AudioStatusMailbox *,const ControllerState *);
void audio_mailbox_drain(AudioStatusMailbox *,ControllerState *out);
void audio_mailbox_reset(AudioStatusMailbox *);
void audio_status_begin(ControllerState *,EndpointRole,uint64_t generation);
void audio_status_finish(ControllerState *,EndpointRole,const char *error);
void audio_status_error(ControllerState *,const char *fallback);
#endif
