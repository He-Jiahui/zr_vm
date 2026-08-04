#include "pooling.h"
#include "pooling_generational_runtime.h"

#include "zr_vm_lib_container/generational_pool.h"

#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"

#include <limits.h>
#include <stdint.h>

static const TZrChar *kPoolAvailableField = "available";
static const TZrChar *kPoolNextGenerationField = "nextGeneration";
static const TZrChar *kPoolReturnCountField = "returnCount";
static const TZrChar *kPoolReuseCountField = "reuseCount";
static const TZrChar *kLeaseOwnerField = "owner";
static const TZrChar *kLeaseBackingField = "backing";
static const TZrChar *kLeaseLengthField = "length";
static const TZrChar *kLeaseGenerationField = "generation";
static const TZrChar *kLeaseReturnedField = "returned";

static SZrObject *pooling_context_self(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }
    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static SZrObject *pooling_object_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        EZrValueType expectedType) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    value = ZrLib_Object_GetFieldCString(state, object, fieldName);
    if (value == ZR_NULL || value->type != expectedType ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

static TZrInt64 pooling_int_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return 0;
    }
    value = ZrLib_Object_GetFieldCString(state, object, fieldName);
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return 0;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (value->value.nativeObject.nativeUInt64 > (TZrUInt64)INT64_MAX) {
        return INT64_MAX;
    }
    return (TZrInt64)value->value.nativeObject.nativeUInt64;
}

static TZrBool pooling_bool_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    value = ZrLib_Object_GetFieldCString(state, object, fieldName);
    return value != ZR_NULL && value->type == ZR_VALUE_TYPE_BOOL &&
           value->value.nativeObject.nativeBool;
}

static TZrBool pooling_has_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName) {
    return (TZrBool)(state != ZR_NULL && object != ZR_NULL &&
                     fieldName != ZR_NULL &&
                     ZrLib_Object_GetFieldCString(
                             state, object, fieldName) != ZR_NULL);
}

static void pooling_set_object_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        SZrObject *value,
        EZrValueType type) {
    SZrTypeValue fieldValue;

    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetObject(state, &fieldValue, value, type);
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void pooling_set_int_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrInt64 value) {
    SZrTypeValue fieldValue;

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void pooling_set_bool_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrBool value) {
    SZrTypeValue fieldValue;

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void pooling_initialize_pool_state(
        SZrState *state,
        SZrObject *pool) {
    if (!pooling_has_field(state, pool, kPoolNextGenerationField)) {
        pooling_set_int_field(state, pool, kPoolNextGenerationField, 0);
    }
    if (!pooling_has_field(state, pool, kPoolReturnCountField)) {
        pooling_set_int_field(state, pool, kPoolReturnCountField, 0);
    }
    if (!pooling_has_field(state, pool, kPoolReuseCountField)) {
        pooling_set_int_field(state, pool, kPoolReuseCountField, 0);
    }
}

static TZrBool pooling_array_set(
        SZrState *state,
        SZrObject *array,
        TZrSize index,
        const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL ||
        index >= ZrLib_Array_Length(array) || index > (TZrSize)INT64_MAX) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static SZrObject *pooling_ensure_available(
        SZrState *state,
        SZrObject *pool) {
    SZrObject *available = pooling_object_field(
            state, pool, kPoolAvailableField, ZR_VALUE_TYPE_ARRAY);

    if (available == ZR_NULL) {
        ZrLibTempValueRoot root;

        if (!ZrLib_TempValueRoot_Begin(state, &root)) {
            return ZR_NULL;
        }
        available = ZrLib_Array_New(state);
        if (available != ZR_NULL &&
            ZrLib_TempValueRoot_SetObject(
                    &root, available, ZR_VALUE_TYPE_ARRAY)) {
            pooling_set_object_field(
                    state,
                    pool,
                    kPoolAvailableField,
                    available,
                    ZR_VALUE_TYPE_ARRAY);
        }
        ZrLib_TempValueRoot_End(&root);
    }
    return available;
}

static TZrBool pooling_clear_backing(
        SZrState *state,
        SZrObject *backing) {
    SZrTypeValue nullValue;

    if (state == ZR_NULL || backing == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNull(&nullValue);
    for (TZrSize index = 0u; index < ZrLib_Array_Length(backing); index++) {
        if (!pooling_array_set(state, backing, index, &nullValue)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool pooling_store_available(
        SZrState *state,
        SZrObject *pool,
        SZrObject *backing) {
    SZrObject *available;
    SZrTypeValue backingValue;

    available = pooling_ensure_available(state, pool);
    if (available == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(
            state, &backingValue, backing, ZR_VALUE_TYPE_ARRAY);
    for (TZrSize index = 0u; index < ZrLib_Array_Length(available); index++) {
        const SZrTypeValue *candidate = ZrLib_Array_Get(
                state, available, index);
        if (candidate == ZR_NULL || candidate->type == ZR_VALUE_TYPE_NULL) {
            return pooling_array_set(
                    state, available, index, &backingValue);
        }
    }
    return ZrLib_Array_PushValue(state, available, &backingValue);
}

static SZrObject *pooling_acquire_backing(
        SZrState *state,
        SZrObject *pool,
        TZrSize length,
        TZrBool *outReused,
        ZrLibTempValueRoot *backingRoot) {
    SZrObject *available;
    SZrTypeValue nullValue;

    if (backingRoot == ZR_NULL) {
        return ZR_NULL;
    }
    if (outReused != ZR_NULL) {
        *outReused = ZR_FALSE;
    }
    available = pooling_ensure_available(state, pool);
    if (available == ZR_NULL) {
        return ZR_NULL;
    }
    ZrLib_Value_SetNull(&nullValue);
    for (TZrSize index = 0u; index < ZrLib_Array_Length(available); index++) {
        const SZrTypeValue *candidate = ZrLib_Array_Get(
                state, available, index);
        SZrObject *backing;

        if (candidate == ZR_NULL || candidate->type != ZR_VALUE_TYPE_ARRAY ||
            candidate->value.object == ZR_NULL) {
            continue;
        }
        backing = ZR_CAST_OBJECT(state, candidate->value.object);
        if (ZrLib_Array_Length(backing) != length) {
            continue;
        }
        if (!ZrLib_TempValueRoot_SetObject(
                    backingRoot, backing, ZR_VALUE_TYPE_ARRAY) ||
            !pooling_array_set(state, available, index, &nullValue)) {
            return ZR_NULL;
        }
        if (outReused != ZR_NULL) {
            *outReused = ZR_TRUE;
        }
        return backing;
    }

    {
        SZrObject *backing = ZrLib_Array_New(state);
        if (backing == ZR_NULL) {
            return ZR_NULL;
        }
        if (!ZrLib_TempValueRoot_SetObject(
                    backingRoot, backing, ZR_VALUE_TYPE_ARRAY)) {
            return ZR_NULL;
        }
        for (TZrSize index = 0u; index < length; index++) {
            if (!ZrLib_Array_PushValue(state, backing, &nullValue)) {
                return ZR_NULL;
            }
        }
        return backing;
    }
}

static TZrBool pooling_buffer_pool_rent(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *pool = pooling_context_self(context);
    TZrInt64 requestedLength = 0;
    TZrInt64 generation;
    TZrBool reused = ZR_FALSE;
    SZrObject *backing;
    SZrObject *lease;
    ZrLibTempValueRoot backingRoot;
    ZrLibTempValueRoot leaseRoot;
    TZrBool hasBackingRoot = ZR_FALSE;
    TZrBool hasLeaseRoot = ZR_FALSE;

    if (pool == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_ReadInt(context, 0u, &requestedLength)) {
        return ZR_FALSE;
    }
    pooling_initialize_pool_state(context->state, pool);
    if (requestedLength < 0 || (TZrUInt64)requestedLength > (TZrUInt64)SIZE_MAX) {
        ZrCore_Debug_RunError(context->state, "pool rent length is out of range");
    }
    generation = pooling_int_field(
            context->state, pool, kPoolNextGenerationField);
    if (generation == INT64_MAX) {
        ZrCore_Debug_RunError(context->state, "pool generation exhausted");
    }
    generation++;

    if (!ZrLib_CallContext_BeginTempValueRoot(context, &backingRoot)) {
        return ZR_FALSE;
    }
    hasBackingRoot = ZR_TRUE;
    backing = pooling_acquire_backing(
            context->state,
            pool,
            (TZrSize)requestedLength,
            &reused,
            &backingRoot);
    if (backing == ZR_NULL) {
        goto cleanup;
    }

    lease = ZrLib_Type_NewInstance(
            context->state, "zr.pooling.PoolLease");
    if (lease == ZR_NULL) {
        lease = ZrLib_Type_NewInstance(context->state, "PoolLease");
    }
    if (lease == ZR_NULL ||
        !ZrLib_CallContext_BeginTempValueRoot(context, &leaseRoot)) {
        goto cleanup;
    }
    hasLeaseRoot = ZR_TRUE;
    if (!ZrLib_TempValueRoot_SetObject(
                &leaseRoot, lease, ZR_VALUE_TYPE_OBJECT)) {
        goto cleanup;
    }

    pooling_set_object_field(
            context->state, lease, kLeaseOwnerField, pool, ZR_VALUE_TYPE_OBJECT);
    pooling_set_object_field(
            context->state,
            lease,
            kLeaseBackingField,
            backing,
            ZR_VALUE_TYPE_ARRAY);
    pooling_set_int_field(
            context->state, lease, kLeaseLengthField, requestedLength);
    pooling_set_int_field(
            context->state, lease, kLeaseGenerationField, generation);
    pooling_set_bool_field(
            context->state, lease, kLeaseReturnedField, ZR_FALSE);
    pooling_set_int_field(
            context->state, pool, kPoolNextGenerationField, generation);
    if (reused) {
        pooling_set_int_field(
                context->state,
                pool,
                kPoolReuseCountField,
                pooling_int_field(
                        context->state, pool, kPoolReuseCountField) + 1);
    }
    if (context->state->threadStatus != ZR_THREAD_STATUS_FINE) {
        goto cleanup;
    }
    ZrLib_Value_SetObject(
            context->state, result, lease, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&leaseRoot);
    ZrLib_TempValueRoot_End(&backingRoot);
    return ZR_TRUE;

cleanup:
    if (hasLeaseRoot) {
        ZrLib_TempValueRoot_End(&leaseRoot);
    }
    if (hasBackingRoot) {
        ZrLib_TempValueRoot_End(&backingRoot);
    }
    return ZR_FALSE;
}

static TZrBool pooling_pool_lease_close(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *lease = pooling_context_self(context);
    SZrObject *owner;
    SZrObject *backing;

    if (lease == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (pooling_bool_field(
                context->state, lease, kLeaseReturnedField)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }
    owner = pooling_object_field(
            context->state, lease, kLeaseOwnerField, ZR_VALUE_TYPE_OBJECT);
    backing = pooling_object_field(
            context->state, lease, kLeaseBackingField, ZR_VALUE_TYPE_ARRAY);
    if (owner == ZR_NULL || backing == ZR_NULL ||
        !pooling_clear_backing(context->state, backing) ||
        !pooling_store_available(context->state, owner, backing)) {
        return ZR_FALSE;
    }
    pooling_set_bool_field(
            context->state, lease, kLeaseReturnedField, ZR_TRUE);
    pooling_set_object_field(
            context->state,
            lease,
            kLeaseBackingField,
            ZR_NULL,
            ZR_VALUE_TYPE_NULL);
    pooling_set_int_field(
            context->state, lease, kLeaseLengthField, 0);
    pooling_set_int_field(
            context->state,
            owner,
            kPoolReturnCountField,
            pooling_int_field(
                    context->state, owner, kPoolReturnCountField) + 1);
    ZrLib_Value_SetNull(result);
    return context->state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static SZrObject *pooling_require_active_backing(
        ZrLibCallContext *context,
        SZrObject **outLease) {
    SZrObject *lease = pooling_context_self(context);
    SZrObject *backing;

    if (outLease != ZR_NULL) {
        *outLease = lease;
    }
    backing = lease != ZR_NULL
                      ? pooling_object_field(
                                context->state,
                                lease,
                                kLeaseBackingField,
                                ZR_VALUE_TYPE_ARRAY)
                      : ZR_NULL;
    if (lease == ZR_NULL || backing == ZR_NULL ||
        pooling_bool_field(
                context->state, lease, kLeaseReturnedField)) {
        ZrCore_Debug_RunError(
                context != ZR_NULL ? context->state : ZR_NULL,
                "pool lease has already been returned");
    }
    return backing;
}

static TZrSize pooling_read_index(
        ZrLibCallContext *context,
        SZrObject *lease) {
    TZrInt64 index = 0;
    TZrInt64 length = pooling_int_field(
            context->state, lease, kLeaseLengthField);

    if (!ZrLib_CallContext_ReadInt(context, 0u, &index) ||
        index < 0 || index >= length) {
        ZrCore_Debug_RunError(context->state, "pool lease index out of range");
    }
    return (TZrSize)index;
}

static TZrBool pooling_pool_lease_get_item(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *lease;
    SZrObject *backing = pooling_require_active_backing(context, &lease);
    const SZrTypeValue *value;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    value = ZrLib_Array_Get(
            context->state,
            backing,
            pooling_read_index(context, lease));
    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(result);
    } else {
        ZrCore_Value_Copy(context->state, result, value);
    }
    return ZR_TRUE;
}

static TZrBool pooling_pool_lease_set_item(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *lease;
    SZrObject *backing = pooling_require_active_backing(context, &lease);
    const SZrTypeValue *value = ZrLib_CallContext_Argument(context, 1u);

    if (result == ZR_NULL || value == ZR_NULL ||
        !pooling_array_set(
                context->state,
                backing,
                pooling_read_index(context, lease),
                value)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool pooling_pool_lease_span(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *lease;
    SZrObject *backing = pooling_require_active_backing(context, &lease);
    SZrObject *view;
    ZrLibTempValueRoot viewRoot;

    ZR_UNUSED_PARAMETER(backing);
    if (result == ZR_NULL ||
        !ZrLib_CallContext_BeginTempValueRoot(context, &viewRoot)) {
        return ZR_FALSE;
    }
    view = ZrLib_Type_NewInstance(context->state, "zr.container.Span");
    if (view == ZR_NULL) {
        view = ZrLib_Type_NewInstance(context->state, "Span");
    }
    if (view == ZR_NULL ||
        !ZrLib_TempValueRoot_SetObject(
                &viewRoot, view, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&viewRoot);
        return ZR_FALSE;
    }
    pooling_set_object_field(
            context->state,
            view,
            "source",
            lease,
            ZR_VALUE_TYPE_OBJECT);
    pooling_set_int_field(context->state, view, "start", 0);
    pooling_set_int_field(
            context->state,
            view,
            "length",
            pooling_int_field(
                    context->state, lease, kLeaseLengthField));
    ZrLib_Value_SetObject(
            context->state, result, view, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&viewRoot);
    return context->state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static const ZrLibGenericParameterDescriptor kSingleGenericT[] = {
        {"T", ZR_NULL, ZR_NULL, 0u},
};

static const ZrLibParameterDescriptor kRentParameters[] = {
        {"length", "int", "Requested element count."},
};

static const ZrLibParameterDescriptor kIndexParameters[] = {
        {"index", "int", "Zero-based element index."},
};

static const ZrLibParameterDescriptor kIndexValueParameters[] = {
        {"index", "int", "Zero-based element index."},
        {"value", "T", "Replacement element value."},
};

static const ZrLibParameterDescriptor kPoolValueParameters[] = {
        {"value", "T", "Value copied into stable pool storage."},
};

static const ZrLibParameterDescriptor kPoolHandleParameters[] = {
        {"handle", "PoolHandle<T>", "Weak generational entity identity."},
};

static const ZrLibParameterDescriptor kPoolReadParameters[] = {
        {"handle", "PoolHandle<T>", "Weak generational entity identity."},
        {"view", "PoolReadRef<T>", "Receives the scoped readonly guard.", ZR_LIB_PARAMETER_PASSING_MODE_OUT},
};

static const ZrLibParameterDescriptor kPoolBorrowParameters[] = {
        {"handle", "PoolHandle<T>", "Weak generational entity identity."},
        {"view", "PoolRef<T>", "Receives the scoped writable guard.", ZR_LIB_PARAMETER_PASSING_MODE_OUT},
};

static const ZrLibFieldDescriptor kPoolHandleFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "poolId",
                "uint",
                "Identity of the owning pool.",
                ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_POOL_ID),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "slotIndex",
                "uint",
                "Stable slot index inside the owning pool.",
                ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_SLOT),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "generation",
                "uint",
                "Generation that permanently invalidates stale handles.",
                ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_GENERATION),
};

static const ZrLibMethodDescriptor kGenerationalPoolMethods[] = {
        {.name = "deliver",
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = ZrPooling_Generational_Deliver,
         .returnTypeName = "PoolHandle<T>",
         .documentation = "Initialize a stable slot and publish its weak identity.",
         .parameters = kPoolValueParameters,
         .parameterCount = ZR_ARRAY_COUNT(kPoolValueParameters),
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_DELIVER},
        {.name = "isLive",
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = ZrPooling_Generational_IsLive,
         .returnTypeName = "bool",
         .documentation = "Validate the complete pool, slot and generation identity.",
         .parameters = kPoolHandleParameters,
         .parameterCount = ZR_ARRAY_COUNT(kPoolHandleParameters),
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_VALIDATE},
        {.name = "recycle",
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = ZrPooling_Generational_Recycle,
         .returnTypeName = "bool",
         .documentation = "Retire a live identity and reclaim it after active guards end.",
         .parameters = kPoolHandleParameters,
         .parameterCount = ZR_ARRAY_COUNT(kPoolHandleParameters),
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_RECYCLE},
        {.name = "tryRead",
         .minArgumentCount = 2u,
         .maxArgumentCount = 2u,
         .callback = ZrPooling_Generational_TryRead,
         .returnTypeName = "bool",
         .documentation = "Acquire a scoped readonly guard through an out parameter.",
         .parameters = kPoolReadParameters,
         .parameterCount = ZR_ARRAY_COUNT(kPoolReadParameters),
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ},
        {.name = "tryBorrow",
         .minArgumentCount = 2u,
         .maxArgumentCount = 2u,
         .callback = ZrPooling_Generational_TryBorrow,
         .returnTypeName = "bool",
         .documentation = "Acquire an exclusive scoped writable guard through an out parameter.",
         .parameters = kPoolBorrowParameters,
         .parameterCount = ZR_ARRAY_COUNT(kPoolBorrowParameters),
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE},
};

static const ZrLibFieldDescriptor kPoolRefFields[] = {
        {.name = "__zr_pool_guard_value",
         .typeName = "T",
         .documentation = "Runtime-only storage for the guarded projection.",
         .runtimeOnly = ZR_TRUE},
};

static const ZrLibMethodDescriptor kPoolRefMethods[] = {
        {.name = "__get_value",
         .minArgumentCount = 0u,
         .maxArgumentCount = 0u,
         .callback = ZrPooling_Generational_RefValue,
         .returnTypeName = "T",
         .documentation = "Return the writable guarded projection.",
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION,
         .propertyName = "value",
         .propertyReferenceAccess = ZR_LIB_REFERENCE_ACCESS_WRITABLE,
         .propertyExportsWritableRef = ZR_TRUE},
        {.name = "close",
         .minArgumentCount = 0u,
         .maxArgumentCount = 0u,
         .callback = ZrPooling_Generational_RefClose,
         .returnTypeName = "null",
         .documentation = "Release this guard exactly once.",
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE},
};

static const ZrLibFieldDescriptor kPoolReadRefFields[] = {
        {.name = "__zr_pool_guard_value",
         .typeName = "T",
         .documentation = "Runtime-only storage for the guarded projection.",
         .runtimeOnly = ZR_TRUE,
         .isReadonly = ZR_TRUE},
};

static const ZrLibMethodDescriptor kPoolReadRefMethods[] = {
        {.name = "__get_value",
         .minArgumentCount = 0u,
         .maxArgumentCount = 0u,
         .callback = ZrPooling_Generational_RefValue,
         .returnTypeName = "T",
         .documentation = "Return the readonly guarded projection.",
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION,
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
         .propertyName = "value",
         .propertyReferenceAccess = ZR_LIB_REFERENCE_ACCESS_READONLY},
        {.name = "close",
         .minArgumentCount = 0u,
         .maxArgumentCount = 0u,
         .callback = ZrPooling_Generational_RefClose,
         .returnTypeName = "null",
         .documentation = "Release this guard exactly once.",
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE},
};

static const ZrLibMetaMethodDescriptor kPoolRefMetaMethods[] = {
        {.metaType = ZR_META_CLOSE,
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = ZrPooling_Generational_RefClose,
         .returnTypeName = "null",
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_RELEASE},
};

static const ZrLibFieldDescriptor kBufferPoolFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "available", "array", "Pool-owned reusable backing arrays."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "nextGeneration", "int", "Last issued lease generation."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "returnCount", "int", "Number of unique lease returns."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "reuseCount", "int", "Number of reused backing arrays."),
};

static const ZrLibMethodDescriptor kBufferPoolMethods[] = {
        {.name = "rent",
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = pooling_buffer_pool_rent,
         .returnTypeName = "PoolLease<T>",
         .documentation = "Rent an owner-backed buffer lease.",
         .isStatic = ZR_FALSE,
         .parameters = kRentParameters,
         .parameterCount = ZR_ARRAY_COUNT(kRentParameters),
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT)},
};

static const ZrLibFieldDescriptor kPoolLeaseFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "owner", "BufferPool", "Pool that owns the backing allocation."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "backing", "array", "GC-tracked backing array."),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "length",
                "int",
                "Active lease element count.",
                ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH),
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "generation", "int", "Monotonic lease generation."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT(
                "returned", "bool", "Whether this lease has been returned."),
};

static const ZrLibMethodDescriptor kPoolLeaseMethods[] = {
        {.name = "span",
         .minArgumentCount = 0u,
         .maxArgumentCount = 0u,
         .callback = pooling_pool_lease_span,
         .returnTypeName = "Span<T>",
         .documentation = "Borrow the active lease as a mutable Span.",
         .isStatic = ZR_FALSE,
         .contractRole = ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE},
        {.name = "close",
         .minArgumentCount = 0u,
         .maxArgumentCount = 0u,
         .callback = pooling_pool_lease_close,
         .returnTypeName = "null",
         .documentation = "Return the backing array exactly once.",
         .isStatic = ZR_FALSE},
};

static const ZrLibMetaMethodDescriptor kPoolLeaseMetaMethods[] = {
        {.metaType = ZR_META_GET_ITEM,
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = pooling_pool_lease_get_item,
         .returnTypeName = "T",
         .parameters = kIndexParameters,
         .parameterCount = ZR_ARRAY_COUNT(kIndexParameters)},
        {.metaType = ZR_META_SET_ITEM,
         .minArgumentCount = 2u,
         .maxArgumentCount = 2u,
         .callback = pooling_pool_lease_set_item,
         .returnTypeName = "null",
         .parameters = kIndexValueParameters,
         .parameterCount = ZR_ARRAY_COUNT(kIndexValueParameters)},
        {.metaType = ZR_META_CLOSE,
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = pooling_pool_lease_close,
         .returnTypeName = "null"},
};

static const ZrLibTypeDescriptor kPoolingTypes[] = {
        {.name = "BufferPool",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = kBufferPoolFields,
         .fieldCount = ZR_ARRAY_COUNT(kBufferPoolFields),
         .methods = kBufferPoolMethods,
         .methodCount = ZR_ARRAY_COUNT(kBufferPoolMethods),
         .documentation = "Owner of reusable GC-tracked buffer allocations.",
         .allowBoxedConstruction = ZR_TRUE,
         .constructorSignature = "BufferPool()"},
        {.name = "PoolLease",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = kPoolLeaseFields,
         .fieldCount = ZR_ARRAY_COUNT(kPoolLeaseFields),
         .methods = kPoolLeaseMethods,
         .methodCount = ZR_ARRAY_COUNT(kPoolLeaseMethods),
         .metaMethods = kPoolLeaseMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kPoolLeaseMetaMethods),
         .documentation = "Single-return owner-backed pooled buffer lease.",
         .allowBoxedConstruction = ZR_FALSE,
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask =
                 ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_OWNER)},
        {.name = "PoolHandle",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kPoolHandleFields,
         .fieldCount = ZR_ARRAY_COUNT(kPoolHandleFields),
         .documentation = "Readonly weak identity containing pool, slot and generation scalars.",
         .allowValueConstruction = ZR_FALSE,
         .allowBoxedConstruction = ZR_FALSE,
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT)},
        {.name = "Pool",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .methods = kGenerationalPoolMethods,
         .methodCount = ZR_ARRAY_COUNT(kGenerationalPoolMethods),
         .documentation = "Stable slab owner with generational weak identities and guarded borrows.",
         .allowBoxedConstruction = ZR_TRUE,
         .constructorSignature = "Pool<T>()",
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask =
                 ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE)},
        {.name = "PoolRef",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kPoolRefFields,
         .fieldCount = ZR_ARRAY_COUNT(kPoolRefFields),
         .methods = kPoolRefMethods,
         .methodCount = ZR_ARRAY_COUNT(kPoolRefMethods),
         .metaMethods = kPoolRefMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kPoolRefMetaMethods),
         .documentation = "Move-only scoped writable stable-slot guard.",
         .allowValueConstruction = ZR_FALSE,
         .allowBoxedConstruction = ZR_FALSE,
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)},
        {.name = "PoolReadRef",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kPoolReadRefFields,
         .fieldCount = ZR_ARRAY_COUNT(kPoolReadRefFields),
         .methods = kPoolReadRefMethods,
         .methodCount = ZR_ARRAY_COUNT(kPoolReadRefMethods),
         .metaMethods = kPoolRefMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kPoolRefMetaMethods),
         .documentation = "Move-only scoped readonly stable-slot guard.",
         .allowValueConstruction = ZR_FALSE,
         .allowBoxedConstruction = ZR_FALSE,
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE)},
};

static const ZrLibConstantDescriptor kPoolingConstants[] = {
        {.name = "STABLE_SLOT_CONTRACT_HASH",
         .kind = ZR_LIB_CONSTANT_KIND_INT,
         .intValue = (TZrInt64)ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
         .documentation = "Versioned StableSlotSource capability contract hash.",
         .typeName = "uint"},
};

static const ZrLibModuleLinkDescriptor kPoolingModuleLinks[] = {
        {"container", "zr.container", "Span and ReadOnlySpan contracts."},
};

static const ZrLibModuleDescriptor kPoolingModuleDescriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.pooling",
        .constants = kPoolingConstants,
        .constantCount = ZR_ARRAY_COUNT(kPoolingConstants),
        .types = kPoolingTypes,
        .typeCount = ZR_ARRAY_COUNT(kPoolingTypes),
        .documentation = "Buffer leasing and generational stable-slot pooling.",
        .moduleLinks = kPoolingModuleLinks,
        .moduleLinkCount = ZR_ARRAY_COUNT(kPoolingModuleLinks),
        .moduleVersion = "1.1.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
        .publicContractHash = "zr.pooling:v1:stable-slot-generational-pool",
};

const ZrLibModuleDescriptor *ZrVmLibContainer_Pooling_GetModuleDescriptor(void) {
    return &kPoolingModuleDescriptor;
}
