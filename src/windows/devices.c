#include "devices.h"
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void device_info_free(DeviceInfo *i) { free(i->id); free(i->name); free(i->description); free(i->adapter); memset(i,0,sizeof(*i)); }
void devices_free(DeviceList *l) { size_t i; for(i=0;i<l->count;++i) device_info_free(&l->items[i]); free(l->items); memset(l,0,sizeof(*l)); }
bool device_read_info(IMMDevice *device,DeviceInfo *out,char error[256]) {
    DeviceInfo result={0}; IMMEndpoint *endpoint=NULL; IPropertyStore *store=NULL;
    LPWSTR id=NULL; PROPVARIANT value; DWORD state=0; EDataFlow flow; HRESULT hr;
    PropVariantInit(&value);
    hr=IMMDevice_GetId(device,&id); if(FAILED(hr)) goto failed;
    result.id=win_wide_to_utf8(id); if(!result.id) { hr=E_OUTOFMEMORY; goto failed; }
    hr=IMMDevice_GetState(device,&state); if(FAILED(hr)) goto failed;
    result.active=(state&DEVICE_STATE_ACTIVE)!=0;
    hr=IMMDevice_QueryInterface(device,&IID_IMMEndpoint,(void **)&endpoint); if(FAILED(hr)) goto failed;
    hr=IMMEndpoint_GetDataFlow(endpoint,&flow); if(FAILED(hr)) goto failed;
    result.direction=flow==eCapture?DEVICE_CAPTURE:DEVICE_RENDER;
    hr=IMMDevice_OpenPropertyStore(device,STGM_READ,&store); if(FAILED(hr)) goto failed;
    hr=IPropertyStore_GetValue(store,&PKEY_Device_FriendlyName,&value); if(FAILED(hr)) goto failed;
    if(value.vt!=VT_LPWSTR || !value.pwszVal) { hr=E_INVALIDARG; goto failed; }
    result.name=win_wide_to_utf8(value.pwszVal); if(!result.name) { hr=E_OUTOFMEMORY; goto failed; }
    PropVariantClear(&value); PropVariantInit(&value);
    hr=IPropertyStore_GetValue(store,&PKEY_Device_DeviceDesc,&value);
    if(SUCCEEDED(hr) && value.vt==VT_LPWSTR && value.pwszVal) result.description=win_wide_to_utf8(value.pwszVal);
    PropVariantClear(&value); PropVariantInit(&value);
    hr=IPropertyStore_GetValue(store,&PKEY_DeviceInterface_FriendlyName,&value);
    if(SUCCEEDED(hr) && value.vt==VT_LPWSTR && value.pwszVal) result.adapter=win_wide_to_utf8(value.pwszVal);
    result.cable=endpoint_classify_metadata(result.description,result.adapter);
    PropVariantClear(&value); IPropertyStore_Release(store); IMMEndpoint_Release(endpoint); CoTaskMemFree(id);
    *out=result; error[0]=0; return true;
failed:
    win_error(error,"Read selected audio endpoint",hr); PropVariantClear(&value);
    if(store) IPropertyStore_Release(store);
    if(endpoint) IMMEndpoint_Release(endpoint);
    CoTaskMemFree(id); device_info_free(&result); return false;
}
bool devices_snapshot(DeviceList *out,char error[256]) {
    IMMDeviceEnumerator *enumerator=NULL; IMMDeviceCollection *collection=NULL;
    IMMDevice *device=NULL; DeviceList result={0}; UINT count=0,i; HRESULT hr;
    hr=CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_INPROC_SERVER,&IID_IMMDeviceEnumerator,(void **)&enumerator);
    if(FAILED(hr)) goto failed;
    hr=IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,eAll,DEVICE_STATE_ACTIVE,&collection); if(FAILED(hr)) goto failed;
    hr=IMMDeviceCollection_GetCount(collection,&count); if(FAILED(hr)) goto failed;
    if(count) { result.items=calloc(count,sizeof(*result.items)); if(!result.items) { hr=E_OUTOFMEMORY; goto failed; } }
    for(i=0;i<count;++i) {
        hr=IMMDeviceCollection_Item(collection,i,&device); if(FAILED(hr)) goto failed;
        if(!device_read_info(device,&result.items[result.count],error)) { hr=E_FAIL; goto failed; }
        ++result.count; IMMDevice_Release(device); device=NULL;
    }
    IMMDeviceCollection_Release(collection); IMMDeviceEnumerator_Release(enumerator);
    *out=result; error[0]=0; return true;
failed:
    if(!error[0]) win_error(error,"Enumerate audio endpoints",hr);
    if(device) IMMDevice_Release(device);
    if(collection) IMMDeviceCollection_Release(collection);
    if(enumerator) IMMDeviceEnumerator_Release(enumerator);
    devices_free(&result); return false;
}
bool device_lookup(IMMDeviceEnumerator *enumerator,const char *id,EndpointRole role,IMMDevice **out,char error[256]) {
    wchar_t *wide=win_utf8_to_wide(id); IMMDevice *device=NULL; DeviceInfo info={0}; HRESULT hr;
    EndpointView view; bool ok;
    *out=NULL;
    if(!wide || !wide[0]) { free(wide); snprintf(error,256,"Select an explicit endpoint first"); return false; }
    hr=IMMDeviceEnumerator_GetDevice(enumerator,wide,&device); free(wide);
    if(FAILED(hr)) { win_error(error,"Selected endpoint unavailable",hr); return false; }
    if(!device_read_info(device,&info,error)) { IMMDevice_Release(device); return false; }
    view=(EndpointView){info.id,info.name,info.direction,info.active,info.cable};
    ok=endpoint_validate(&view,role,error); device_info_free(&info);
    if(!ok) { IMMDevice_Release(device); return false; }
    *out=device; return true;
}
