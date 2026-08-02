#include "pooling_generational_runtime.h"

#include "zr_vm_lib_container/generational_pool.h"

#include "zr_vm_core/object.h"
#include "zr_vm_core/property_reference.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ZR_POOLING_RUNTIME_FIELD "__zr_pool_runtime"
#define ZR_POOLING_VALUES_FIELD "__zr_pool_values"
#define ZR_POOLING_GUARD_FIELD "__zr_pool_guard"
#define ZR_POOLING_GUARD_OWNER_FIELD "__zr_pool_guard_owner"
#define ZR_POOLING_GUARD_VALUE_FIELD "__zr_pool_guard_value"

typedef struct SZrPoolingRuntime {
    SZrPool *pool;
    SZrState *state;
    TZrBool finalized;
} SZrPoolingRuntime;

typedef struct SZrPoolingGuardRuntime {
    SZrPoolGuard guard;
    TZrBool finalized;
    TZrBool writable;
} SZrPoolingGuardRuntime;

static const SZrTypeValue *pooling_runtime_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name) {
    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLib_Object_GetFieldCString(state, object, name);
}

static void pooling_runtime_set_native_pointer(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        TZrPtr pointer) {
    SZrTypeValue value;

    ZrLib_Value_SetNativePointer(state, &value, pointer);
    ZrLib_Object_SetFieldCString(state, object, name, &value);
}

static void pooling_runtime_set_object(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        SZrObject *fieldObject,
        EZrValueType type) {
    SZrTypeValue value;

    if (fieldObject == ZR_NULL) {
        ZrLib_Value_SetNull(&value);
    } else {
        ZrLib_Value_SetObject(state, &value, fieldObject, type);
    }
    ZrLib_Object_SetFieldCString(state, object, name, &value);
}

static TZrBool pooling_runtime_set_hidden_value(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    SZrString *fieldName;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }
    fieldName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)name);
    if (fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static SZrObject *pooling_runtime_object_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        EZrValueType expectedType) {
    const SZrTypeValue *value = pooling_runtime_field(state, object, name);

    if (value == ZR_NULL || value->type != expectedType ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrPoolingRuntime *pooling_runtime_from_object(
        SZrState *state,
        SZrObject *object) {
    const SZrTypeValue *value = pooling_runtime_field(
            state, object, ZR_POOLING_RUNTIME_FIELD);

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_NULL;
    }
    return (SZrPoolingRuntime *)value->value.nativeObject.nativePointer;
}

static SZrPoolingGuardRuntime *pooling_guard_from_object(
        SZrState *state,
        SZrObject *object) {
    const SZrTypeValue *value = pooling_runtime_field(
            state, object, ZR_POOLING_GUARD_FIELD);

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_NULL;
    }
    return (SZrPoolingGuardRuntime *)value->value.nativeObject.nativePointer;
}

static TZrBool pooling_runtime_array_set(
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

static TZrBool pooling_runtime_store_value(
        SZrState *state,
        SZrObject *owner,
        TZrSize slotIndex,
        const SZrTypeValue *value) {
    SZrObject *values = pooling_runtime_object_field(
            state, owner, ZR_POOLING_VALUES_FIELD, ZR_VALUE_TYPE_ARRAY);
    SZrTypeValue nullValue;

    if (values == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNull(&nullValue);
    while (ZrLib_Array_Length(values) <= slotIndex) {
        if (!ZrLib_Array_PushValue(state, values, &nullValue)) {
            return ZR_FALSE;
        }
    }
    return pooling_runtime_array_set(state, values, slotIndex, value);
}

static void pooling_runtime_clear_value(
        SZrState *state,
        SZrObject *owner,
        TZrSize slotIndex) {
    SZrObject *values = pooling_runtime_object_field(
            state, owner, ZR_POOLING_VALUES_FIELD, ZR_VALUE_TYPE_ARRAY);
    SZrTypeValue nullValue;

    if (values == ZR_NULL || slotIndex >= ZrLib_Array_Length(values)) {
        return;
    }
    ZrLib_Value_SetNull(&nullValue);
    pooling_runtime_array_set(state, values, slotIndex, &nullValue);
}

static void pooling_value_scan(
        SZrState *state,
        SZrTypeValue *value,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(value);
    ZR_UNUSED_PARAMETER(userData);
}

static void pooling_pool_finalize(SZrState *state, SZrRawObject *rawObject) {
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    SZrPoolingRuntime *runtime;

    if (object == ZR_NULL) {
        return;
    }
    runtime = pooling_runtime_from_object(state, object);
    if (runtime == ZR_NULL || runtime->finalized) {
        return;
    }
    runtime->finalized = ZR_TRUE;
    if (runtime->pool != ZR_NULL &&
        ZrPool_Destroy(&runtime->pool) != ZR_POOL_STATUS_OK) {
        runtime->finalized = ZR_FALSE;
        return;
    }
    free(runtime);
    pooling_runtime_set_native_pointer(
            state, object, ZR_POOLING_RUNTIME_FIELD, ZR_NULL);
}

static SZrPoolingRuntime *pooling_runtime_require(
        ZrLibCallContext *context,
        SZrObject **outOwner) {
    SZrTypeValue *selfValue;
    SZrObject *owner;
    SZrPoolingRuntime *runtime;
    SZrObject *values;
    ZrLibTempValueRoot valuesRoot;
    SZrTypeLayout layout;
    SZrPoolConfig config;

    if (outOwner != ZR_NULL) {
        *outOwner = ZR_NULL;
    }
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }
    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    owner = ZR_CAST_OBJECT(context->state, selfValue->value.object);
    runtime = pooling_runtime_from_object(context->state, owner);
    if (runtime != ZR_NULL) {
        if (outOwner != ZR_NULL) {
            *outOwner = owner;
        }
        return runtime;
    }

    if (!ZrLib_CallContext_BeginTempValueRoot(context, &valuesRoot)) {
        return ZR_NULL;
    }
    values = ZrLib_Array_New(context->state);
    if (values == ZR_NULL ||
        !ZrLib_TempValueRoot_SetObject(
                &valuesRoot, values, ZR_VALUE_TYPE_ARRAY)) {
        ZrLib_TempValueRoot_End(&valuesRoot);
        return ZR_NULL;
    }
    runtime = (SZrPoolingRuntime *)calloc(1u, sizeof(*runtime));
    if (runtime == ZR_NULL) {
        ZrLib_TempValueRoot_End(&valuesRoot);
        return ZR_NULL;
    }
    runtime->state = context->state;
    ZrCore_TypeLayout_InitValue(&layout);
    memset(&config, 0, sizeof(config));
    config.slabCapacity = 64u;
    config.generationLimit = UINT64_MAX;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_THREAD_LOCAL;
    if (ZrPool_CreateFromTypeLayout(
                context->state,
                &layout,
                ZR_NULL,
                pooling_value_scan,
                runtime,
                &config,
                &runtime->pool) !=
        ZR_POOL_STATUS_OK) {
        free(runtime);
        ZrLib_TempValueRoot_End(&valuesRoot);
        return ZR_NULL;
    }
    pooling_runtime_set_object(
            context->state,
            owner,
            ZR_POOLING_VALUES_FIELD,
            values,
            ZR_VALUE_TYPE_ARRAY);
    pooling_runtime_set_native_pointer(
            context->state, owner, ZR_POOLING_RUNTIME_FIELD, runtime);
    owner->super.scanMarkGcFunction = pooling_pool_finalize;
    ZrLib_TempValueRoot_End(&valuesRoot);
    if (context->state->threadStatus != ZR_THREAD_STATUS_FINE) {
        pooling_pool_finalize(
                context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(owner));
        return ZR_NULL;
    }
    if (outOwner != ZR_NULL) {
        *outOwner = owner;
    }
    return runtime;
}

static TZrBool pooling_read_uint_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        TZrUInt64 *outValue) {
    const SZrTypeValue *value = pooling_runtime_field(state, object, name);

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (value == ZR_NULL || outValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }
    *outValue = (TZrUInt64)value->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool pooling_read_handle(
        ZrLibCallContext *context,
        TZrSize argumentIndex,
        SZrPoolHandle *outHandle) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(
            context, argumentIndex);
    SZrObject *object;
    TZrUInt64 slotIndex;

    if (outHandle == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outHandle, 0, sizeof(*outHandle));
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    object = ZR_CAST_OBJECT(context->state, value->value.object);
    if (!pooling_read_uint_field(
                context->state, object, "poolId", &outHandle->poolId) ||
        !pooling_read_uint_field(
                context->state, object, "slotIndex", &slotIndex) ||
        !pooling_read_uint_field(
                context->state,
                object,
                "generation",
                &outHandle->generation) ||
        slotIndex > (TZrUInt64)SIZE_MAX) {
        memset(outHandle, 0, sizeof(*outHandle));
        return ZR_FALSE;
    }
    outHandle->slotIndex = (TZrSize)slotIndex;
    return ZR_TRUE;
}

static void pooling_set_uint_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        TZrUInt64 number) {
    SZrTypeValue value;

    ZrCore_Value_InitAsUInt(state, &value, number);
    ZrLib_Object_SetFieldCString(state, object, name, &value);
}

static SZrObject *pooling_new_handle(
        ZrLibCallContext *context,
        SZrPoolHandle handle,
        ZrLibTempValueRoot *root) {
    SZrObject *object;

    if (!ZrLib_CallContext_BeginTempValueRoot(context, root)) {
        return ZR_NULL;
    }
    object = ZrLib_Type_NewInstance(
            context->state, "zr.pooling.PoolHandle");
    if (object == ZR_NULL) {
        object = ZrLib_Type_NewInstance(context->state, "PoolHandle");
    }
    if (object == ZR_NULL ||
        !ZrLib_TempValueRoot_SetObject(root, object, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(root);
        return ZR_NULL;
    }
    pooling_set_uint_field(context->state, object, "poolId", handle.poolId);
    pooling_set_uint_field(
            context->state,
            object,
            "slotIndex",
            (TZrUInt64)handle.slotIndex);
    pooling_set_uint_field(
            context->state, object, "generation", handle.generation);
    if (context->state->threadStatus != ZR_THREAD_STATUS_FINE) {
        ZrLib_TempValueRoot_End(root);
        return ZR_NULL;
    }
    return object;
}

static void pooling_clear_reclaimed_value(
        SZrState *state,
        SZrObject *owner,
        SZrPoolingRuntime *runtime,
        SZrPoolHandle handle) {
    EZrPoolStatus status;

    if (runtime == ZR_NULL || runtime->pool == ZR_NULL) {
        return;
    }
    status = ZrPool_Validate(runtime->pool, handle);
    if (status == ZR_POOL_STATUS_HANDLE_STALE ||
        status == ZR_POOL_STATUS_GENERATION_EXHAUSTED) {
        pooling_runtime_clear_value(state, owner, handle.slotIndex);
    }
}

TZrBool ZrPooling_Generational_Deliver(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *owner;
    SZrPoolingRuntime *runtime = pooling_runtime_require(context, &owner);
    SZrTypeValue *source = ZrLib_CallContext_Argument(context, 0u);
    SZrPoolHandle handle;
    ZrLibTempValueRoot handleRoot;
    SZrObject *handleObject;

    if (runtime == ZR_NULL || source == ZR_NULL || result == ZR_NULL ||
        ZrPool_Deliver(runtime->pool, source, &handle) != ZR_POOL_STATUS_OK) {
        return ZR_FALSE;
    }
    if (!pooling_runtime_store_value(
                context->state, owner, handle.slotIndex, source)) {
        ZrPool_Recycle(runtime->pool, handle);
        return ZR_FALSE;
    }
    handleObject = pooling_new_handle(context, handle, &handleRoot);
    if (handleObject == ZR_NULL) {
        ZrPool_Recycle(runtime->pool, handle);
        pooling_runtime_clear_value(
                context->state, owner, handle.slotIndex);
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(
            context->state, result, handleObject, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&handleRoot);
    return context->state->threadStatus == ZR_THREAD_STATUS_FINE;
}

TZrBool ZrPooling_Generational_IsLive(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrPoolingRuntime *runtime = pooling_runtime_require(context, ZR_NULL);
    SZrPoolHandle handle;
    TZrBool isLive;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    isLive = (TZrBool)(runtime != ZR_NULL &&
                       pooling_read_handle(context, 0u, &handle) &&
                       ZrPool_Validate(runtime->pool, handle) ==
                               ZR_POOL_STATUS_OK);
    ZrLib_Value_SetBool(context->state, result, isLive);
    return ZR_TRUE;
}

TZrBool ZrPooling_Generational_Recycle(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *owner;
    SZrPoolingRuntime *runtime = pooling_runtime_require(context, &owner);
    SZrPoolHandle handle;
    TZrBool recycled = ZR_FALSE;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (runtime != ZR_NULL && pooling_read_handle(context, 0u, &handle) &&
        ZrPool_Recycle(runtime->pool, handle) == ZR_POOL_STATUS_OK) {
        recycled = ZR_TRUE;
        pooling_clear_reclaimed_value(
                context->state, owner, runtime, handle);
    }
    ZrLib_Value_SetBool(context->state, result, recycled);
    return ZR_TRUE;
}

static void pooling_guard_finalize(SZrState *state, SZrRawObject *rawObject) {
    SZrObject *view = ZR_CAST_OBJECT(state, rawObject);
    SZrPoolingGuardRuntime *guardRuntime;
    SZrObject *owner;
    SZrPoolingRuntime *poolRuntime;
    SZrPoolHandle handle;

    if (view == ZR_NULL) {
        return;
    }
    guardRuntime = pooling_guard_from_object(state, view);
    if (guardRuntime == ZR_NULL || guardRuntime->finalized) {
        return;
    }
    guardRuntime->finalized = ZR_TRUE;
    owner = pooling_runtime_object_field(
            state,
            view,
            ZR_POOLING_GUARD_OWNER_FIELD,
            ZR_VALUE_TYPE_OBJECT);
    poolRuntime = pooling_runtime_from_object(state, owner);
    memset(&handle, 0, sizeof(handle));
    handle.slotIndex = guardRuntime->guard.slotIndex;
    handle.generation = guardRuntime->guard.generation;
    if (guardRuntime->guard.pool != ZR_NULL && poolRuntime != ZR_NULL &&
        poolRuntime->pool == guardRuntime->guard.pool) {
        SZrPoolStats stats;
        if (ZrPool_GetStats(poolRuntime->pool, &stats) == ZR_POOL_STATUS_OK) {
            handle.poolId = stats.poolId;
        }
    }
    if (guardRuntime->guard.active && guardRuntime->writable) {
        const SZrTypeValue *storedValue = pooling_runtime_field(
                state, view, ZR_POOLING_GUARD_VALUE_FIELD);
        SZrTypeValue *guardedValue =
                (SZrTypeValue *)ZrPoolGuard_Value(&guardRuntime->guard);

        if (storedValue != ZR_NULL && guardedValue != ZR_NULL) {
            ZrCore_Value_Copy(state, guardedValue, storedValue);
            if (owner != ZR_NULL) {
                (void)pooling_runtime_store_value(
                        state,
                        owner,
                        guardRuntime->guard.slotIndex,
                        storedValue);
            }
        }
    }
    if (guardRuntime->guard.active) {
        ZrPoolGuard_Release(&guardRuntime->guard);
    }
    if (poolRuntime != ZR_NULL) {
        pooling_clear_reclaimed_value(state, owner, poolRuntime, handle);
    }
    free(guardRuntime);
    pooling_runtime_set_native_pointer(
            state, view, ZR_POOLING_GUARD_FIELD, ZR_NULL);
}

static SZrObject *pooling_new_guard_view(
        ZrLibCallContext *context,
        SZrObject *owner,
        SZrPoolGuard *guard,
        const TZrChar *qualifiedTypeName,
        const TZrChar *shortTypeName,
        TZrBool writable,
        ZrLibTempValueRoot *root) {
    SZrObject *view;
    SZrPoolingGuardRuntime *guardRuntime;
    const SZrTypeValue *guardedValue;

    if (!ZrLib_CallContext_BeginTempValueRoot(context, root)) {
        return ZR_NULL;
    }
    view = ZrLib_Type_NewInstance(context->state, qualifiedTypeName);
    if (view == ZR_NULL) {
        view = ZrLib_Type_NewInstance(context->state, shortTypeName);
    }
    guardRuntime = (SZrPoolingGuardRuntime *)calloc(
            1u, sizeof(*guardRuntime));
    if (view == ZR_NULL || guardRuntime == ZR_NULL ||
        !ZrLib_TempValueRoot_SetObject(root, view, ZR_VALUE_TYPE_OBJECT)) {
        free(guardRuntime);
        ZrLib_TempValueRoot_End(root);
        return ZR_NULL;
    }
    guardRuntime->guard = *guard;
    guardRuntime->writable = writable;
    memset(guard, 0, sizeof(*guard));
    pooling_runtime_set_native_pointer(
            context->state, view, ZR_POOLING_GUARD_FIELD, guardRuntime);
    pooling_runtime_set_object(
            context->state,
            view,
            ZR_POOLING_GUARD_OWNER_FIELD,
            owner,
            ZR_VALUE_TYPE_OBJECT);
    guardedValue = (const SZrTypeValue *)ZrPoolGuard_ReadOnlyValue(
            &guardRuntime->guard);
    if (guardedValue != ZR_NULL) {
        (void)pooling_runtime_set_hidden_value(
                context->state,
                view,
                ZR_POOLING_GUARD_VALUE_FIELD,
                guardedValue);
    }
    view->super.scanMarkGcFunction = pooling_guard_finalize;
    if (context->state->threadStatus != ZR_THREAD_STATUS_FINE) {
        pooling_guard_finalize(
                context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(view));
        ZrLib_TempValueRoot_End(root);
        return ZR_NULL;
    }
    return view;
}

static TZrBool pooling_try_guard(
        ZrLibCallContext *context,
        SZrTypeValue *result,
        EZrPoolBorrowMode mode) {
    SZrObject *owner;
    SZrPoolingRuntime *runtime = pooling_runtime_require(context, &owner);
    SZrPoolHandle handle;
    SZrPoolGuard guard = {0};
    EZrPoolStatus status = ZR_POOL_STATUS_INVALID_ARGUMENT;
    TZrBool readHandle = ZR_FALSE;
    SZrObject *view = ZR_NULL;
    ZrLibTempValueRoot viewRoot;
    SZrTypeValue viewValue;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNull(&viewValue);
    readHandle = pooling_read_handle(context, 0u, &handle);
    if (runtime != ZR_NULL && readHandle) {
        status = mode == ZR_POOL_BORROW_WRITE
                         ? ZrPool_TryBorrow(runtime->pool, handle, &guard)
                         : ZrPool_TryRead(runtime->pool, handle, &guard);
    }
    if (status == ZR_POOL_STATUS_OK) {
        view = pooling_new_guard_view(
                context,
                owner,
                &guard,
                mode == ZR_POOL_BORROW_WRITE
                        ? "zr.pooling.PoolRef"
                        : "zr.pooling.PoolReadRef",
                mode == ZR_POOL_BORROW_WRITE ? "PoolRef" : "PoolReadRef",
                mode == ZR_POOL_BORROW_WRITE,
                &viewRoot);
        if (view == ZR_NULL) {
            if (guard.active) {
                ZrPoolGuard_Release(&guard);
            }
            return ZR_FALSE;
        }
        ZrLib_Value_SetObject(
                context->state, &viewValue, view, ZR_VALUE_TYPE_OBJECT);
    }
    if (!ZrLib_CallContext_WriteBackArgument(context, 1u, &viewValue)) {
        if (view != ZR_NULL) {
            pooling_guard_finalize(
                    context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(view));
            ZrLib_TempValueRoot_End(&viewRoot);
        }
        return ZR_FALSE;
    }
    if (view != ZR_NULL) {
        ZrLib_TempValueRoot_End(&viewRoot);
    }
    ZrLib_Value_SetBool(
            context->state,
            result,
            (TZrBool)(status == ZR_POOL_STATUS_OK));
    return ZR_TRUE;
}

TZrBool ZrPooling_Generational_TryRead(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    return pooling_try_guard(context, result, ZR_POOL_BORROW_READ);
}

TZrBool ZrPooling_Generational_TryBorrow(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    return pooling_try_guard(context, result, ZR_POOL_BORROW_WRITE);
}

TZrBool ZrPooling_Generational_RefClose(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrTypeValue *selfValue;
    SZrObject *view;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_FALSE;
    }
    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL && ZrLib_CallContext_ArgumentCount(context) > 0u) {
        selfValue = ZrLib_CallContext_Argument(context, 0u);
    }
    if (selfValue != ZR_NULL && selfValue->type == ZR_VALUE_TYPE_OBJECT &&
        selfValue->value.object != ZR_NULL) {
        view = ZR_CAST_OBJECT(context->state, selfValue->value.object);
        pooling_guard_finalize(
                context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(view));
    }
    if (result != ZR_NULL) {
        ZrLib_Value_SetNull(result);
    }
    return ZR_TRUE;
}

TZrBool ZrPooling_Generational_RefValue(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrTypeValue *selfValue;
    SZrObject *view;
    SZrPoolingGuardRuntime *guardRuntime;

    if (context == ZR_NULL || context->state == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT ||
        selfValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    view = ZR_CAST_OBJECT(context->state, selfValue->value.object);
    guardRuntime = pooling_guard_from_object(context->state, view);
    if (guardRuntime == ZR_NULL || guardRuntime->finalized ||
        !guardRuntime->guard.active) {
        return ZR_FALSE;
    }
    return ZrCore_PropertyReference_CreateMemberByName(
            context->state,
            selfValue,
            ZR_POOLING_GUARD_VALUE_FIELD,
            result);
}
