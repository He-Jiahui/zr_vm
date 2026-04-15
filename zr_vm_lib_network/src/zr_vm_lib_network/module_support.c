#include "network/network_internal.h"

#include <stdlib.h>
#include <string.h>

static const TZrChar *kNetworkHandleField = "__zr_network_handle";

SZrObject *zr_network_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);

    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT || selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

TZrBool zr_network_raise_runtime_error(SZrState *state, const TZrChar *message) {
    SZrTypeValue errorValue;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(state, &errorValue, message != ZR_NULL ? message : "Network runtime error");
    if (!ZrCore_Exception_NormalizeThrownValue(state,
                                               &errorValue,
                                               state->callInfoList,
                                               ZR_THREAD_STATUS_EXCEPTION_ERROR) &&
        !ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
        ZrCore_Debug_RunError(state, (TZrNativeString) (message != ZR_NULL ? message : "Network runtime error"));
    }

    state->threadStatus = state->currentExceptionStatus != ZR_THREAD_STATUS_FINE
                                  ? state->currentExceptionStatus
                                  : ZR_THREAD_STATUS_EXCEPTION_ERROR;
    return ZR_FALSE;
}

SZrObject *zr_network_new_typed_object(SZrState *state, const TZrChar *moduleName, const TZrChar *typeName) {
    SZrObject *object;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (moduleName != ZR_NULL &&
        ZrLib_Module_GetLoaded(state, moduleName) == ZR_NULL &&
        ZrLib_Module_GetExport(state, moduleName, typeName) == ZR_NULL) {
        return ZrLib_Object_New(state);
    }

    object = ZrLib_Type_NewInstance(state, typeName);
    if (object == ZR_NULL) {
        object = ZrLib_Object_New(state);
    }
    return object;
}

TZrBool zr_network_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

ZrNetworkVmHandle *zr_network_alloc_handle(EZrNetworkVmHandleKind kind) {
    ZrNetworkVmHandle *handle = (ZrNetworkVmHandle *) calloc(1, sizeof(*handle));

    if (handle != ZR_NULL) {
        handle->kind = kind;
    }
    return handle;
}

TZrBool zr_network_store_handle(SZrState *state, SZrObject *object, ZrNetworkVmHandle *handle) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetNativePointer(state, &value, handle);
    ZrLib_Object_SetFieldCString(state, object, kNetworkHandleField, &value);
    return ZR_TRUE;
}

ZrNetworkVmHandle *zr_network_get_handle(SZrState *state, SZrObject *object, EZrNetworkVmHandleKind expectedKind) {
    const SZrTypeValue *value;
    ZrNetworkVmHandle *handle;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    value = ZrLib_Object_GetFieldCString(state, object, kNetworkHandleField);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER || value->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }

    handle = (ZrNetworkVmHandle *) value->value.nativeObject.nativePointer;
    if (handle->kind != expectedKind) {
        return ZR_NULL;
    }

    return handle;
}

TZrBool zr_network_read_endpoint_args(const ZrLibCallContext *context,
                                      TZrSize hostIndex,
                                      TZrSize portIndex,
                                      SZrNetworkEndpoint *outEndpoint) {
    SZrString *hostString = ZR_NULL;
    TZrInt64 portValue = 0;
    const TZrChar *hostText;
    TZrSize hostLength;

    if (outEndpoint == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outEndpoint, 0, sizeof(*outEndpoint));
    if (!ZrLib_CallContext_ReadString(context, hostIndex, &hostString)) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, portIndex, &portValue)) {
        return ZR_FALSE;
    }

    hostText = ZrCore_String_GetNativeString(hostString);
    hostLength = hostText != ZR_NULL ? strlen(hostText) : 0;
    if (hostLength >= sizeof(outEndpoint->host)) {
        return zr_network_raise_runtime_error(context->state, "network host is too long");
    }
    if (portValue < 0 || portValue > 65535) {
        return zr_network_raise_runtime_error(context->state, "network port must be between 0 and 65535");
    }

    if (hostText != ZR_NULL) {
        memcpy(outEndpoint->host, hostText, hostLength + 1);
    } else {
        outEndpoint->host[0] = '\0';
    }
    outEndpoint->port = (TZrUInt16) portValue;
    return ZR_TRUE;
}

TZrBool zr_network_read_timeout_arg(const ZrLibCallContext *context,
                                    TZrSize index,
                                    TZrUInt32 defaultValue,
                                    TZrUInt32 *outTimeoutMs) {
    TZrInt64 value = 0;

    if (outTimeoutMs == ZR_NULL) {
        return ZR_FALSE;
    }

    *outTimeoutMs = defaultValue;
    if (context == ZR_NULL || ZrLib_CallContext_ArgumentCount(context) <= index) {
        return ZR_TRUE;
    }

    if (!ZrLib_CallContext_ReadInt(context, index, &value)) {
        return ZR_FALSE;
    }
    if (value < 0 || value > 0xFFFFFFFFLL) {
        return zr_network_raise_runtime_error(context->state, "timeout must be between 0 and 4294967295");
    }

    *outTimeoutMs = (TZrUInt32) value;
    return ZR_TRUE;
}

TZrBool zr_network_read_byte_count_arg(const ZrLibCallContext *context,
                                       TZrSize index,
                                       TZrSize *outLength) {
    TZrInt64 value = 0;

    if (outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadInt(context, index, &value)) {
        return ZR_FALSE;
    }
    if (value <= 0) {
        return zr_network_raise_runtime_error(context->state, "byte count must be greater than 0");
    }

    *outLength = (TZrSize) value;
    return ZR_TRUE;
}
