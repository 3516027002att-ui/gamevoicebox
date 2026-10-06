/* MinGW's uuid import library does not define all modern audio GUIDs.
   Instantiate the official header declarations once, without new DLLs. */
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
