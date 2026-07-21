#include "ffi_runtime_internal.h"

static TZrBool zr_ffi_pointer_require_open(
        ZrLibCallContext *context,
        SZrObject **outObject,
        ZrFfiPointerData **outData) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiPointerData *pointerData =
            (ZrFfiPointerData *)zr_ffi_get_handle_data(
                    context != ZR_NULL ? context->state : ZR_NULL,
                    selfObject);

    if (outObject != ZR_NULL) {
        *outObject = selfObject;
    }
    if (outData != ZR_NULL) {
        *outData = pointerData;
    }
    if (context == ZR_NULL || context->state == ZR_NULL ||
        selfObject == ZR_NULL || pointerData == ZR_NULL ||
        pointerData->base.kind != ZR_FFI_HANDLE_POINTER) {
        return ZR_FALSE;
    }
    if (pointerData->closed ||
        (pointerData->byteLength > 0u && pointerData->address == ZR_NULL)) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_NATIVE_CALL,
                "pointer handle is null or closed");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrFfi_Pointer_Span(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *selfObject;
    ZrFfiPointerData *pointerData;
    SZrObject *spanObject;
    ZrLibTempValueRoot spanRoot;
    SZrTypeValue fieldValue;

    if (result == ZR_NULL ||
        !zr_ffi_pointer_require_open(
                context, &selfObject, &pointerData) ||
        !ZrLib_CallContext_BeginTempValueRoot(context, &spanRoot)) {
        return ZR_FALSE;
    }
    spanObject = ZrLib_Type_NewInstance(context->state, "zr.container.Span");
    if (spanObject == ZR_NULL) {
        spanObject = ZrLib_Type_NewInstance(context->state, "Span");
    }
    if (spanObject == ZR_NULL ||
        !ZrLib_TempValueRoot_SetObject(
                &spanRoot, spanObject, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&spanRoot);
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(
            context->state,
            &fieldValue,
            selfObject,
            ZR_VALUE_TYPE_OBJECT);
    ZrLib_Object_SetFieldCString(
            context->state, spanObject, "source", &fieldValue);
    ZrLib_Value_SetInt(context->state, &fieldValue, 0);
    ZrLib_Object_SetFieldCString(
            context->state, spanObject, "start", &fieldValue);
    ZrLib_Value_SetInt(
            context->state,
            &fieldValue,
            (TZrInt64)pointerData->byteLength);
    ZrLib_Object_SetFieldCString(
            context->state, spanObject, "length", &fieldValue);

    ZrLib_Value_SetObject(
            context->state,
            result,
            spanObject,
            ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&spanRoot);
    return context->state->threadStatus == ZR_THREAD_STATUS_FINE;
}

TZrBool ZrFfi_Pointer_GetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    ZrFfiPointerData *pointerData;
    TZrInt64 index = 0;

    if (result == ZR_NULL ||
        !zr_ffi_pointer_require_open(context, ZR_NULL, &pointerData) ||
        !ZrLib_CallContext_ReadInt(context, 0u, &index)) {
        return ZR_FALSE;
    }
    if (index < 0 || (TZrUInt64)index >= (TZrUInt64)pointerData->byteLength) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_NATIVE_CALL,
                "pinned pointer index is out of range");
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(
            context->state, result, pointerData->address[(TZrSize)index]);
    return ZR_TRUE;
}

TZrBool ZrFfi_Pointer_SetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    ZrFfiPointerData *pointerData;
    TZrInt64 index = 0;
    TZrInt64 value = 0;

    if (result == ZR_NULL ||
        !zr_ffi_pointer_require_open(context, ZR_NULL, &pointerData) ||
        !ZrLib_CallContext_ReadInt(context, 0u, &index) ||
        !ZrLib_CallContext_ReadInt(context, 1u, &value)) {
        return ZR_FALSE;
    }
    if (index < 0 || (TZrUInt64)index >= (TZrUInt64)pointerData->byteLength) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_NATIVE_CALL,
                "pinned pointer index is out of range");
        return ZR_FALSE;
    }
    if (value < 0 || value > 255) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_MARSHAL,
                "pinned pointer byte value must be in the range 0..255");
        return ZR_FALSE;
    }
    pointerData->address[(TZrSize)index] = (unsigned char)value;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}
