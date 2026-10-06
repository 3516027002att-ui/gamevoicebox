#ifndef RONGYU_WINDOWS_DEVICES_H
#define RONGYU_WINDOWS_DEVICES_H
#include "device_contract.h"
#include <stddef.h>
typedef struct { char *id,*name,*description,*adapter; DeviceDirection direction; bool active; CableClass cable; } DeviceInfo;
typedef struct { DeviceInfo *items; size_t count; } DeviceList;
bool devices_snapshot(DeviceList *out,char error[256]);
void devices_free(DeviceList *list);
#ifdef _WIN32
#include "common.h"
#include <mmdeviceapi.h>
/* Owner-apartment-only helpers; IMMDevice pointers never go to UI messages. */
bool device_read_info(IMMDevice *device,DeviceInfo *out,char error[256]);
void device_info_free(DeviceInfo *info);
bool device_lookup(IMMDeviceEnumerator *enumerator,const char *id,EndpointRole role,
                   IMMDevice **out,char error[256]);
#endif
#endif
