#include "internal.h"
#include "native_call_context_internal.h"

#include "zr_vm_core/state.h"

#include <stdint.h>
#include <string.h>

struct ZrRustBindingNativeArgumentView {
    const ZrRustBindingNativeCallContext *context;
    TZrSize index;
    ZrLibTempValueRoot root;
    SZrGcNativeCallPin pin;
    TZrBool hasRoot;
    TZrBool isPinned;
};

static const SZrTypeValue *zr_rust_binding_native_argument_view_value(
        const ZrRustBindingNativeArgumentView *argument) {
    if (argument == ZR_NULL || argument->context == ZR_NULL || argument->context->context == ZR_NULL) {
        return ZR_NULL;
    }
    if (argument->hasRoot) {
        return ZrLib_TempValueRoot_Value((ZrLibTempValueRoot *)&argument->root);
    }
    return ZrLib_CallContext_Argument(argument->context->context, argument->index);
}

static void zr_rust_binding_native_argument_view_cleanup(
        ZrRustBindingNativeArgumentView *argument) {
    if (argument == ZR_NULL) {
        return;
    }
    if (argument->isPinned && argument->root.state != ZR_NULL) {
        ZrCore_Gc_NativeCallUnpin(argument->root.state->global, &argument->pin);
        argument->isPinned = ZR_FALSE;
    }
    if (argument->hasRoot) {
        ZrLib_TempValueRoot_End(&argument->root);
        argument->hasRoot = ZR_FALSE;
    }
}

static ZrRustBindingStatus zr_rust_binding_native_argument_view_init(
        ZrRustBindingNativeArgumentView *argument,
        const ZrRustBindingNativeCallContext *context,
        TZrSize index) {
    const SZrTypeValue *value;
    ZrRustBindingValueKind kind;

    if (argument == ZR_NULL || context == ZR_NULL || context->context == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view context is invalid");
    }

    memset(argument, 0, sizeof(*argument));
    value = ZrLib_CallContext_Argument(context->context, index);
    if (value == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "native argument index is out of range");
    }

    argument->context = context;
    argument->index = index;
    kind = zr_rust_binding_map_value_kind(value->type);
    if (kind != ZR_RUST_BINDING_VALUE_KIND_STRING && kind != ZR_RUST_BINDING_VALUE_KIND_ARRAY) {
        return ZR_RUST_BINDING_STATUS_OK;
    }

    if (!ZrLib_CallContext_BeginTempValueRoot(context->context, &argument->root)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to root native argument view");
    }
    argument->hasRoot = ZR_TRUE;
    value = ZrLib_CallContext_Argument(context->context, index);
    if (value == ZR_NULL) {
        zr_rust_binding_native_argument_view_cleanup(argument);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "native argument index moved out of range");
    }
    if (!ZrLib_TempValueRoot_SetValue(&argument->root, value)) {
        zr_rust_binding_native_argument_view_cleanup(argument);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to preserve native argument view");
    }
    value = ZrLib_TempValueRoot_Value(&argument->root);
    if (value == ZR_NULL || !ZrCore_Gc_NativeCallPinValue(context->context->state, value, &argument->pin)) {
        zr_rust_binding_native_argument_view_cleanup(argument);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to pin native argument view");
    }
    argument->isPinned = ZR_TRUE;
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_WithArgument(
        const ZrRustBindingNativeCallContext *context,
        TZrSize index,
        FZrRustBindingNativeArgumentVisitor visitor,
        TZrPtr userData) {
    ZrRustBindingNativeArgumentView argument = {0};
    ZrRustBindingStatus status;

    if (visitor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument visitor is null");
    }

    status = zr_rust_binding_native_argument_view_init(&argument, context, index);
    if (status == ZR_RUST_BINDING_STATUS_OK) {
        status = visitor(&argument, userData);
    }
    zr_rust_binding_native_argument_view_cleanup(&argument);
    if (status == ZR_RUST_BINDING_STATUS_OK) {
        zr_rust_binding_clear_error();
    }
    return status;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_GetKind(
        const ZrRustBindingNativeArgumentView *argument,
        ZrRustBindingValueKind *outKind) {
    const SZrTypeValue *value = zr_rust_binding_native_argument_view_value(argument);

    if (value == ZR_NULL || outKind == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view or kind output is invalid");
    }
    *outKind = zr_rust_binding_map_value_kind(value->type);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ReadBool(
        const ZrRustBindingNativeArgumentView *argument,
        TZrBool *outBoolValue) {
    const SZrTypeValue *value = zr_rust_binding_native_argument_view_value(argument);

    if (value == ZR_NULL || outBoolValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view or bool output is invalid");
    }
    if (zr_rust_binding_map_value_kind(value->type) != ZR_RUST_BINDING_VALUE_KIND_BOOL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view is not bool");
    }
    *outBoolValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ReadInt(
        const ZrRustBindingNativeArgumentView *argument,
        TZrInt64 *outIntValue) {
    const SZrTypeValue *value = zr_rust_binding_native_argument_view_value(argument);

    if (value == ZR_NULL || outIntValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view or int output is invalid");
    }
    if (zr_rust_binding_map_value_kind(value->type) != ZR_RUST_BINDING_VALUE_KIND_INT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view is not int");
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outIntValue = value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outIntValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    } else {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view is not an integer value");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ReadFloat(
        const ZrRustBindingNativeArgumentView *argument,
        TZrFloat64 *outFloatValue) {
    const SZrTypeValue *value = zr_rust_binding_native_argument_view_value(argument);

    if (value == ZR_NULL || outFloatValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view or float output is invalid");
    }
    if (zr_rust_binding_map_value_kind(value->type) != ZR_RUST_BINDING_VALUE_KIND_FLOAT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view is not float");
    }
    *outFloatValue = value->value.nativeObject.nativeDouble;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

static ZrRustBindingStatus zr_rust_binding_native_argument_view_array(
        const ZrRustBindingNativeArgumentView *argument,
        SZrObject **outArray) {
    const SZrTypeValue *value = zr_rust_binding_native_argument_view_value(argument);
    SZrObject *array;

    if (value == ZR_NULL || outArray == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view or array output is invalid");
    }
    if (zr_rust_binding_map_value_kind(value->type) != ZR_RUST_BINDING_VALUE_KIND_ARRAY) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view is not array");
    }
    array = ZR_CAST_OBJECT(argument->context->context->state, value->value.object);
    if (array == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "native argument array is unavailable");
    }
    *outArray = array;
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ByteArrayLength(
        const ZrRustBindingNativeArgumentView *argument,
        TZrSize *outLength) {
    SZrObject *array;
    ZrRustBindingStatus status = zr_rust_binding_native_argument_view_array(argument, &array);

    if (status != ZR_RUST_BINDING_STATUS_OK || outLength == ZR_NULL) {
        return outLength == ZR_NULL
                       ? zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                                   "native byte-array length output is null")
                       : status;
    }
    *outLength = ZrLib_Array_Length(array);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_ByteArrayGet(
        const ZrRustBindingNativeArgumentView *argument,
        TZrSize index,
        TZrUInt8 *outByteValue) {
    SZrObject *array;
    const SZrTypeValue *element;
    ZrRustBindingStatus status = zr_rust_binding_native_argument_view_array(argument, &array);

    if (status != ZR_RUST_BINDING_STATUS_OK || outByteValue == ZR_NULL) {
        return outByteValue == ZR_NULL
                       ? zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                                   "native byte-array output is null")
                       : status;
    }
    if (index >= ZrLib_Array_Length(array)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "native byte-array index is out of range");
    }
    element = ZrLib_Array_Get(argument->context->context->state, array, index);
    if (element == ZR_NULL || zr_rust_binding_map_value_kind(element->type) != ZR_RUST_BINDING_VALUE_KIND_INT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native byte-array element is not an integer");
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(element->type)) {
        if (element->value.nativeObject.nativeInt64 < 0 ||
            element->value.nativeObject.nativeInt64 > (TZrInt64)UINT8_MAX) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                             "native byte-array element is outside 0..=255");
        }
        *outByteValue = (TZrUInt8)element->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(element->type)) {
        if (element->value.nativeObject.nativeUInt64 > (TZrUInt64)UINT8_MAX) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                             "native byte-array element is outside 0..=255");
        }
        *outByteValue = (TZrUInt8)element->value.nativeObject.nativeUInt64;
    } else {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native byte-array element is not an integer");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeArgumentView_WithString(
        const ZrRustBindingNativeArgumentView *argument,
        FZrRustBindingNativeStringVisitor visitor,
        TZrPtr userData) {
    const SZrTypeValue *value = zr_rust_binding_native_argument_view_value(argument);
    SZrString *stringValue;

    if (value == ZR_NULL || visitor == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view or string visitor is invalid");
    }
    if (zr_rust_binding_map_value_kind(value->type) != ZR_RUST_BINDING_VALUE_KIND_STRING) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "native argument view is not string");
    }
    stringValue = ZR_CAST_STRING(argument->context->context->state, value->value.object);
    if (stringValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "native argument string is unavailable");
    }
    return visitor(ZrCore_String_GetNativeString(stringValue),
                   ZrCore_String_GetByteLength(stringValue),
                   userData);
}
