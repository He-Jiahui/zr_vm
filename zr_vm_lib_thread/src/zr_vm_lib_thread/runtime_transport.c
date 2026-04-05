#include "runtime_internal.h"

#include <stdio.h>

static const TZrChar *kTaskChannelTransportField = "__zr_task_channel_transport";

static TZrChar *zr_vm_task_duplicate_native_string(const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (text == ZR_NULL) {
        text = "";
    }

    length = strlen(text);
    copy = (TZrChar *)malloc(length + 1);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

void zr_vm_task_transport_clear(ZrVmTaskTransportValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    if (value->kind == ZR_VM_TASK_TRANSPORT_KIND_STRING && value->as.stringValue != ZR_NULL) {
        free(value->as.stringValue);
    }

    memset(value, 0, sizeof(*value));
}

TZrBool zr_vm_task_channel_try_get_transport(SZrState *state,
                                             const SZrTypeValue *value,
                                             ZrVmTaskChannelTransport **outTransport) {
    const SZrTypeValue *transportValue;
    SZrObject *object;

    if (outTransport != ZR_NULL) {
        *outTransport = ZR_NULL;
    }
    if (state == ZR_NULL || value == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    transportValue = zr_vm_task_get_field_value(state, object, kTaskChannelTransportField);
    if (transportValue == ZR_NULL || transportValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        transportValue->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outTransport != ZR_NULL) {
        *outTransport = (ZrVmTaskChannelTransport *)transportValue->value.nativeObject.nativePointer;
    }
    return ZR_TRUE;
}

TZrBool zr_vm_task_channel_make_value(SZrState *state, ZrVmTaskChannelTransport *transport, SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue transportValue;

    if (state == ZR_NULL || transport == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_new_typed_object(state, "Channel");
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetNativePointer(state, &transportValue, transport);
    zr_vm_task_set_value_field(state, handle, kTaskChannelTransportField, &transportValue);
    return zr_vm_task_finish_object(state, result, handle);
}

TZrBool zr_vm_task_transport_encode_value(SZrState *state,
                                          const SZrTypeValue *value,
                                          ZrVmTaskTransportValue *outValue,
                                          const TZrChar *contextMessage) {
    ZrVmTaskChannelTransport *channelTransport = ZR_NULL;
    TZrChar message[256];

    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outValue, 0, sizeof(*outValue));
    if (value == ZR_NULL || value->type == ZR_VALUE_TYPE_NULL) {
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_NULL;
        return ZR_TRUE;
    }

    if (value->type == ZR_VALUE_TYPE_BOOL) {
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_BOOL;
        outValue->as.boolValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_INT;
        outValue->as.intValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_UINT;
        outValue->as.uintValue = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_FLOAT;
        outValue->as.floatValue = value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    if (value->type == ZR_VALUE_TYPE_STRING && value->value.object != ZR_NULL) {
        SZrString *stringObject = ZR_CAST_STRING(state, value->value.object);
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_STRING;
        outValue->as.stringValue = zr_vm_task_duplicate_native_string(ZrCore_String_GetNativeString(stringObject));
        return outValue->as.stringValue != ZR_NULL;
    }
    if (zr_vm_task_channel_try_get_transport(state, value, &channelTransport)) {
        outValue->kind = ZR_VM_TASK_TRANSPORT_KIND_CHANNEL;
        outValue->as.pointerValue = channelTransport;
        return ZR_TRUE;
    }

    if (contextMessage != ZR_NULL && state != ZR_NULL) {
        snprintf(message, sizeof(message), "%s", contextMessage);
        return zr_vm_task_raise_runtime_error(state, message);
    }
    return ZR_FALSE;
}

TZrBool zr_vm_task_transport_decode_value(SZrState *state,
                                          const ZrVmTaskTransportValue *value,
                                          SZrTypeValue *result) {
    if (state == ZR_NULL || value == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    switch ((EZrVmTaskTransportKind)value->kind) {
        case ZR_VM_TASK_TRANSPORT_KIND_NULL:
            ZrLib_Value_SetNull(result);
            return ZR_TRUE;
        case ZR_VM_TASK_TRANSPORT_KIND_BOOL:
            ZrLib_Value_SetBool(state, result, value->as.boolValue);
            return ZR_TRUE;
        case ZR_VM_TASK_TRANSPORT_KIND_INT:
            ZrLib_Value_SetInt(state, result, value->as.intValue);
            return ZR_TRUE;
        case ZR_VM_TASK_TRANSPORT_KIND_UINT:
            ZrCore_Value_InitAsUInt(state, result, value->as.uintValue);
            return ZR_TRUE;
        case ZR_VM_TASK_TRANSPORT_KIND_FLOAT:
            ZrLib_Value_SetFloat(state, result, value->as.floatValue);
            return ZR_TRUE;
        case ZR_VM_TASK_TRANSPORT_KIND_STRING:
            ZrLib_Value_SetString(state, result, value->as.stringValue != ZR_NULL ? value->as.stringValue : "");
            return ZR_TRUE;
        case ZR_VM_TASK_TRANSPORT_KIND_CHANNEL:
            return zr_vm_task_channel_make_value(state, (ZrVmTaskChannelTransport *)value->as.pointerValue, result);
        case ZR_VM_TASK_TRANSPORT_KIND_NONE:
        default:
            ZrLib_Value_SetNull(result);
            return ZR_FALSE;
    }
}

static ZrVmTaskChannelTransport *zr_vm_task_channel_get_transport_internal(SZrState *state, SZrObject *channel) {
    SZrTypeValue value;

    if (state == ZR_NULL || channel == ZR_NULL) {
        return ZR_NULL;
    }

    if (zr_vm_task_get_field_value(state, channel, kTaskChannelTransportField) == ZR_NULL) {
        return ZR_NULL;
    }
    value = *zr_vm_task_get_field_value(state, channel, kTaskChannelTransportField);
    if (value.type != ZR_VALUE_TYPE_NATIVE_POINTER || value.value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }

    return (ZrVmTaskChannelTransport *)value.value.nativeObject.nativePointer;
}

TZrBool zr_vm_task_channel_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    ZrVmTaskChannelTransport *transport;
    SZrTypeValue transportValue;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "Channel");
    }
    transport = (ZrVmTaskChannelTransport *)malloc(sizeof(*transport));
    if (handle == ZR_NULL || transport == ZR_NULL) {
        free(transport);
        return ZR_FALSE;
    }

    memset(transport, 0, sizeof(*transport));
    zr_vm_task_sync_mutex_init(&transport->mutex);
    zr_vm_task_sync_condition_init(&transport->condition);
    transport->notifyRuntime = zr_vm_task_scheduler_get_runtime(context->state, zr_vm_task_main_scheduler(context->state));
    ZrLib_Value_SetNativePointer(context->state, &transportValue, transport);
    zr_vm_task_set_value_field(context->state, handle, kTaskChannelTransportField, &transportValue);
    return zr_vm_task_finish_object(context->state, result, handle);
}

TZrBool zr_vm_task_channel_send(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    SZrTypeValue *value;
    ZrVmTaskChannelTransport *transport;
    ZrVmTaskChannelMessage *message;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_vm_task_self_object(context);
    value = ZrLib_CallContext_Argument(context, 0);
    transport = zr_vm_task_channel_get_transport_internal(context->state, self);
    if (self == ZR_NULL || transport == ZR_NULL) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    message = (ZrVmTaskChannelMessage *)malloc(sizeof(*message));
    if (message == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(message, 0, sizeof(*message));
    if (!zr_vm_task_transport_encode_value(context->state,
                                           value,
                                           &message->value,
                                           "Channel only transports sendable scalar values or Channel handles")) {
        free(message);
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&transport->mutex);
    if (transport->closed) {
        zr_vm_task_sync_mutex_unlock(&transport->mutex);
        zr_vm_task_transport_clear(&message->value);
        free(message);
        return zr_vm_task_raise_runtime_error(context->state, "Channel is closed");
    }

    if (transport->tail != ZR_NULL) {
        transport->tail->next = message;
    } else {
        transport->head = message;
    }
    transport->tail = message;
    transport->length++;
    zr_vm_task_sync_condition_signal(&transport->condition);
    zr_vm_task_sync_mutex_unlock(&transport->mutex);
    zr_vm_task_scheduler_signal_runtime(transport->notifyRuntime);

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_channel_recv(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    ZrVmTaskChannelTransport *transport;
    ZrVmTaskChannelMessage *message;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_vm_task_self_object(context);
    transport = zr_vm_task_channel_get_transport_internal(context->state, self);
    if (self == ZR_NULL || transport == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&transport->mutex);
    message = transport->head;
    if (message != ZR_NULL) {
        transport->head = message->next;
        if (transport->head == ZR_NULL) {
            transport->tail = ZR_NULL;
        }
        if (transport->length > 0) {
            transport->length--;
        }
    }
    zr_vm_task_sync_mutex_unlock(&transport->mutex);

    if (message == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    if (!zr_vm_task_transport_decode_value(context->state, &message->value, result)) {
        zr_vm_task_transport_clear(&message->value);
        free(message);
        return ZR_FALSE;
    }

    zr_vm_task_transport_clear(&message->value);
    free(message);
    return ZR_TRUE;
}

TZrBool zr_vm_task_channel_close(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskChannelTransport *transport;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    transport = zr_vm_task_channel_get_transport_internal(context->state, zr_vm_task_self_object(context));
    if (transport == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&transport->mutex);
    transport->closed = ZR_TRUE;
    zr_vm_task_sync_condition_signal(&transport->condition);
    zr_vm_task_sync_mutex_unlock(&transport->mutex);
    zr_vm_task_scheduler_signal_runtime(transport->notifyRuntime);

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_channel_is_closed(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskChannelTransport *transport;
    TZrBool closed;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    transport = zr_vm_task_channel_get_transport_internal(context->state, zr_vm_task_self_object(context));
    if (transport == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&transport->mutex);
    closed = transport->closed;
    zr_vm_task_sync_mutex_unlock(&transport->mutex);
    ZrLib_Value_SetBool(context->state, result, closed);
    return ZR_TRUE;
}

TZrBool zr_vm_task_channel_length(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskChannelTransport *transport;
    TZrUInt64 length;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    transport = zr_vm_task_channel_get_transport_internal(context->state, zr_vm_task_self_object(context));
    if (transport == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&transport->mutex);
    length = transport->length;
    zr_vm_task_sync_mutex_unlock(&transport->mutex);
    ZrLib_Value_SetInt(context->state, result, (TZrInt64)length);
    return ZR_TRUE;
}
