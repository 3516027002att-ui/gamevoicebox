#ifndef RONGYU_DEVICE_CONTRACT_H
#define RONGYU_DEVICE_CONTRACT_H
#include <stdbool.h>
typedef enum { DEVICE_CAPTURE, DEVICE_RENDER } DeviceDirection;
typedef enum { CABLE_NONE, CABLE_STANDARD_INPUT, CABLE_OTHER, CABLE_UNVERIFIED } CableClass;
typedef enum { ROLE_MIC=1, ROLE_SEND=2, ROLE_MONITOR=4 } EndpointRole;
typedef struct {
    const char *id;
    const char *name;
    DeviceDirection direction;
    bool active;
    CableClass cable;
} EndpointView;
CableClass endpoint_classify_metadata(const char *description,const char *adapter);
/* Caller frees full label; ID is hashed as opaque bytes, never parsed. */
char *endpoint_display_label(const char *name,const char *id,bool duplicate);
bool endpoint_validate(const EndpointView *endpoint, EndpointRole role, char error[256]);
#endif
