#ifndef ZR_VM_LIB_NETWORK_INTERNAL_H
#define ZR_VM_LIB_NETWORK_INTERNAL_H

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_lib_network/network.h"

typedef enum EZrNetworkVmHandleKind {
    ZR_NETWORK_VM_HANDLE_KIND_TCP_LISTENER = 1,
    ZR_NETWORK_VM_HANDLE_KIND_TCP_STREAM = 2,
    ZR_NETWORK_VM_HANDLE_KIND_UDP_SOCKET = 3
} EZrNetworkVmHandleKind;

typedef struct ZrNetworkVmHandle {
    EZrNetworkVmHandleKind kind;
    union {
        SZrNetworkListener listener;
        SZrNetworkStream stream;
        SZrNetworkUdpSocket udpSocket;
    } value;
} ZrNetworkVmHandle;

SZrObject *zr_network_self_object(const ZrLibCallContext *context);
TZrBool zr_network_raise_runtime_error(SZrState *state, const TZrChar *message);
SZrObject *zr_network_new_typed_object(SZrState *state, const TZrChar *moduleName, const TZrChar *typeName);
TZrBool zr_network_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object);

ZrNetworkVmHandle *zr_network_alloc_handle(EZrNetworkVmHandleKind kind);
TZrBool zr_network_store_handle(SZrState *state, SZrObject *object, ZrNetworkVmHandle *handle);
ZrNetworkVmHandle *zr_network_get_handle(SZrState *state, SZrObject *object, EZrNetworkVmHandleKind expectedKind);

TZrBool zr_network_read_endpoint_args(const ZrLibCallContext *context,
                                      TZrSize hostIndex,
                                      TZrSize portIndex,
                                      SZrNetworkEndpoint *outEndpoint);
TZrBool zr_network_read_timeout_arg(const ZrLibCallContext *context,
                                    TZrSize index,
                                    TZrUInt32 defaultValue,
                                    TZrUInt32 *outTimeoutMs);
TZrBool zr_network_read_byte_count_arg(const ZrLibCallContext *context,
                                       TZrSize index,
                                       TZrSize *outLength);

#endif
