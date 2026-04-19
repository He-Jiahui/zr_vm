//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"
#include "object/object_call_internal.h"
#include "object/object_super_array_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_library/native_binding.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS ((TZrUInt8)1)

enum {
    ZR_OBJECT_CACHED_INDEX_READONLY_INLINE_GET_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
    ZR_OBJECT_CACHED_INDEX_READONLY_INLINE_SET_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL
};

#if defined(ZR_DEBUG)
static TZrBool object_trace_enabled(void);
static void object_trace(const TZrChar *format, ...);
#else
#define object_trace(...) ((void)0)
#endif

static const TZrChar *object_module_name_string(SZrState *state, const SZrObjectModule *module) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return "<module>";
    }

    if (module->moduleName != ZR_NULL) {
        const TZrChar *name = ZrCore_String_GetNativeString(module->moduleName);
        if (name != ZR_NULL) {
            return name;
        }
    }

    if (module->fullPath != ZR_NULL) {
        const TZrChar *name = ZrCore_String_GetNativeString(module->fullPath);
        if (name != ZR_NULL) {
            return name;
        }
    }

    return "<module>";
}

static TZrBool object_module_guard_pending_export(SZrState *state,
                                                  SZrObject *object,
                                                  SZrString *memberName) {
    const SZrModuleExportDescriptor *descriptor;
    SZrObjectModule *module;
    const TZrChar *moduleNameText;
    const TZrChar *memberNameText;
    TZrBool blocked = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || memberName == ZR_NULL ||
        object->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_FALSE;
    }

    module = (SZrObjectModule *)object;
    descriptor = ZrCore_Module_FindExportDescriptor(module, memberName);
    if (descriptor != ZR_NULL && descriptor->accessModifier != ZR_ACCESS_CONSTANT_PUBLIC) {
        descriptor = ZR_NULL;
    }

    moduleNameText = object_module_name_string(state, module);
    memberNameText = ZrCore_String_GetNativeString(memberName);
    if (memberNameText == ZR_NULL) {
        memberNameText = "<export>";
    }

    if (module->initState == ZR_MODULE_INIT_STATE_FAILED) {
        ZrCore_Debug_RunError(state,
                              "module initialization failed: export '%s.%s' is unavailable because module '%s' failed during __entry__",
                              moduleNameText,
                              memberNameText,
                              moduleNameText);
        blocked = ZR_TRUE;
    }

    if (descriptor != ZR_NULL &&
        descriptor->readiness == ZR_MODULE_EXPORT_READY_ENTRY &&
        !descriptor->isReady &&
        (module->initState == ZR_MODULE_INIT_STATE_INITIALIZING ||
         (module->reserved0 & ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS) != 0)) {
        ZrCore_Debug_RunError(state,
                              "circular import initialization: export '%s.%s' is not ready because module '%s' is still executing __entry__",
                              moduleNameText,
                              memberNameText,
                              moduleNameText);
        blocked = ZR_TRUE;
    }

    return blocked;
}

static ZR_FORCE_INLINE TZrBool object_key_matches_known_field(SZrState *state,
                                                              const SZrTypeValue *key,
                                                              const TZrChar *literal) {
    SZrString *knownField;
    SZrString *keyString;

    if (state == ZR_NULL || key == ZR_NULL || literal == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING ||
        key->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    knownField = ZrCore_Object_CachedKnownFieldString(state, literal);
    if (knownField == ZR_NULL) {
        return ZR_FALSE;
    }
    if (key->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(knownField)) {
        return ZR_TRUE;
    }

    keyString = ZR_CAST_STRING(state, key->value.object);
    return keyString != ZR_NULL && ZrCore_String_Equal(keyString, knownField);
}

static ZR_FORCE_INLINE TZrBool object_key_is_hidden_items_field(SZrState *state, const SZrTypeValue *key) {
    return object_key_matches_known_field(state, key, ZR_OBJECT_HIDDEN_ITEMS_FIELD);
}

static void object_refresh_hidden_items_object_cache(SZrState *state,
                                                     SZrObject *object,
                                                     const SZrTypeValue *key,
                                                     SZrHashKeyValuePair *pair) {
    SZrObject *itemsObject;

    if (object == ZR_NULL || !object_key_is_hidden_items_field(state, key)) {
        return;
    }

    object->cachedHiddenItemsPair = pair;
    object->cachedHiddenItemsObject = ZR_NULL;
    if (pair == ZR_NULL) {
        return;
    }

    if (pair->value.type != ZR_VALUE_TYPE_OBJECT && pair->value.type != ZR_VALUE_TYPE_ARRAY) {
        return;
    }

    itemsObject = ZR_CAST_OBJECT(state, pair->value.value.object);
    if (itemsObject != ZR_NULL && itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        object->cachedHiddenItemsObject = itemsObject;
    }
}

static ZR_FORCE_INLINE const SZrTypeValue *object_try_get_cached_string_value_unchecked(SZrState *state,
                                                                                         SZrObject *object,
                                                                                         const SZrTypeValue *key) {
    SZrHashKeyValuePair *pair;
    SZrString *keyString;
    SZrString *cachedKeyString;

    if (state == ZR_NULL || object == ZR_NULL || key == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING ||
        key->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    pair = object->cachedStringLookupPair;
    if (pair == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (pair->key.value.object == key->value.object) {
        return &pair->value;
    }

    keyString = ZR_CAST_STRING(state, key->value.object);
    cachedKeyString = ZR_CAST_STRING(state, pair->key.value.object);
    if (keyString == ZR_NULL || cachedKeyString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Equal(keyString, cachedKeyString) ? &pair->value : ZR_NULL;
}

static ZR_FORCE_INLINE SZrProfileRuntime *object_profile_runtime(SZrState *state) {
    return (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;
}

static ZR_FORCE_INLINE void object_record_helper(SZrState *state, EZrProfileHelperKind kind) {
    SZrProfileRuntime *runtime = object_profile_runtime(state);

    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void object_copy_value_profiled(SZrState *state,
                                                       SZrTypeValue *destination,
                                                       const SZrTypeValue *source) {
    object_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
    ZrCore_Value_CopyNoProfile(state, destination, source);
}

static TZrBool object_pin_raw_object(SZrState *state, SZrRawObject *object, TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, object)) {
        return ZR_TRUE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObject(state, object)) {
        return ZR_FALSE;
    }

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_TRUE;
    }
    return ZR_TRUE;
}

static void object_unpin_raw_object(SZrGlobalState *global, SZrRawObject *object, TZrBool addedByCaller) {
    if (!addedByCaller || global == ZR_NULL || object == ZR_NULL) {
        return;
    }

    ZrCore_GarbageCollector_UnignoreObject(global, object);
}

static TZrBool object_pin_value_object(SZrState *state, const SZrTypeValue *value, TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }

    return object_pin_raw_object(state, ZrCore_Value_GetRawObject(value), addedByCaller);
}

static void object_unpin_value_object(SZrGlobalState *global, const SZrTypeValue *value, TZrBool addedByCaller) {
    if (!addedByCaller || global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object_unpin_raw_object(global, ZrCore_Value_GetRawObject(value), ZR_TRUE);
}

static ZR_FORCE_INLINE TZrBool object_value_resides_on_vm_stack(const SZrState *state, const SZrTypeValue *value) {
    TZrStackValuePointer stackValue;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    stackValue = ZR_CAST(TZrStackValuePointer, value);
    return (TZrBool)(stackValue >= state->stackBase.valuePointer && stackValue < state->stackTail.valuePointer);
}

static ZR_FORCE_INLINE SZrObjectPrototype *object_resolve_index_receiver_prototype_unchecked(
        SZrState *state,
        const SZrTypeValue *receiver,
        SZrObject *object) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);

    if (object->prototype != ZR_NULL) {
        return object->prototype;
    }

    if (state->global == ZR_NULL || receiver->type >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_NULL;
    }

    return state->global->basicTypeObjectPrototype[receiver->type];
}

static ZR_FORCE_INLINE void object_refresh_cached_string_lookup_pair(SZrObject *object,
                                                                     const SZrTypeValue *key,
                                                                     SZrHashKeyValuePair *pair) {
    if (object == ZR_NULL || pair == ZR_NULL || key == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING ||
        key->value.object == ZR_NULL) {
        return;
    }

    object->cachedStringLookupPair = pair;
}

static TZrBool ensure_managed_field_capacity(SZrState *state, SZrObjectPrototype *prototype, TZrUInt32 minimumCapacity) {
    SZrManagedFieldInfo *newFields;
    TZrUInt32 newCapacity;
    TZrSize newSize;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (prototype->managedFieldCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = prototype->managedFieldCapacity > 0 ? prototype->managedFieldCapacity : ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY;
    while (newCapacity < minimumCapacity) {
        newCapacity *= ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR;
    }

    newSize = newCapacity * sizeof(SZrManagedFieldInfo);
    newFields = (SZrManagedFieldInfo *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                 newSize,
                                                                 ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (newFields == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newFields, 0, newSize);
    if (prototype->managedFields != ZR_NULL && prototype->managedFieldCount > 0) {
        memcpy(newFields,
               prototype->managedFields,
               prototype->managedFieldCount * sizeof(SZrManagedFieldInfo));
        ZrCore_Memory_RawFreeWithType(state->global,
                                prototype->managedFields,
                                prototype->managedFieldCapacity * sizeof(SZrManagedFieldInfo),
                                ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }

    prototype->managedFields = newFields;
    prototype->managedFieldCapacity = newCapacity;
    return ZR_TRUE;
}

static const SZrManagedFieldInfo *object_prototype_find_managed_field(SZrObjectPrototype *prototype,
                                                                      SZrString *memberName,
                                                                      TZrBool includeInherited) {
    while (prototype != ZR_NULL) {
        for (TZrUInt32 index = 0; index < prototype->managedFieldCount; index++) {
            SZrManagedFieldInfo *fieldInfo = &prototype->managedFields[index];
            if (fieldInfo->name != ZR_NULL && memberName != ZR_NULL &&
                ZrCore_String_Equal(fieldInfo->name, memberName)) {
                return fieldInfo;
            }
        }

        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE void object_make_string_key_unchecked(SZrState *state, SZrString *name, SZrTypeValue *outKey) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(name != ZR_NULL);
    ZR_ASSERT(outKey != ZR_NULL);

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    outKey->type = ZR_VALUE_TYPE_STRING;
}

static TZrBool object_make_string_key(SZrState *state, SZrString *name, SZrTypeValue *outKey) {
    if (state == ZR_NULL || name == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    object_make_string_key_unchecked(state, name, outKey);
    return ZR_TRUE;
}

static TZrBool object_make_cached_string_key(SZrState *state, const TZrChar *name, SZrTypeValue *outKey) {
    return object_make_string_key(state, ZrCore_Object_CachedKnownFieldString(state, name), outKey);
}

static TZrBool object_make_hidden_key(SZrState *state, const TZrChar *name, SZrTypeValue *outKey) {
    return object_make_cached_string_key(state, name, outKey);
}

static TZrBool object_hidden_set(SZrState *state, SZrObject *object, const TZrChar *name, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || value == ZR_NULL || !object_make_hidden_key(state, name, &key)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, &key, value);
    return ZR_TRUE;
}

static TZrBool object_hidden_get(SZrState *state, SZrObject *object, const TZrChar *name, SZrTypeValue *outValue) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || object == ZR_NULL || outValue == ZR_NULL || !object_make_hidden_key(state, name, &key)) {
        return ZR_FALSE;
    }

    value = ZrCore_Object_GetValue(state, object, &key);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(outValue);
    ZrCore_Value_Copy(state, outValue, value);
    return ZR_TRUE;
}

static SZrObjectPrototype *object_value_resolve_prototype(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    if ((value->type != ZR_VALUE_TYPE_OBJECT &&
         value->type != ZR_VALUE_TYPE_ARRAY &&
         value->type != ZR_VALUE_TYPE_STRING) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) {
        object = ZR_CAST_OBJECT(state, value->value.object);
        if (object != ZR_NULL && object->prototype != ZR_NULL) {
            return object->prototype;
        }
    }

    if (state->global == ZR_NULL || value->type >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_NULL;
    }

    return state->global->basicTypeObjectPrototype[value->type];
}

static ZR_FORCE_INLINE SZrObjectPrototype *object_value_resolve_prototype_unchecked(SZrState *state,
                                                                                     const SZrTypeValue *value) {
    SZrObject *object = ZR_NULL;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT((value->type == ZR_VALUE_TYPE_OBJECT ||
               value->type == ZR_VALUE_TYPE_ARRAY ||
               value->type == ZR_VALUE_TYPE_STRING) &&
              value->value.object != ZR_NULL);

    if (value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) {
        object = ZR_CAST_OBJECT(state, value->value.object);
        if (object != ZR_NULL && object->prototype != ZR_NULL) {
            return object->prototype;
        }
    }

    if (state->global == ZR_NULL || value->type >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_NULL;
    }

    return value->type < ZR_VALUE_TYPE_ENUM_MAX ? state->global->basicTypeObjectPrototype[value->type] : ZR_NULL;
}

static const SZrTypeValue *object_get_own_value(SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    SZrHashKeyValuePair *pair;
    const SZrTypeValue *cachedValue;

    if (state == ZR_NULL || object == ZR_NULL || key == ZR_NULL || !object_node_map_is_ready(object)) {
        return ZR_NULL;
    }

    cachedValue = object_try_get_cached_string_value_unchecked(state, object, key);
    if (cachedValue != ZR_NULL) {
        return cachedValue;
    }

    pair = ZrCore_HashSet_Find(state, &object->nodeMap, key);
    object_refresh_cached_string_lookup_pair(object, key, pair);
    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

static ZR_FORCE_INLINE const SZrTypeValue *object_get_own_value_unchecked(SZrState *state,
                                                                          SZrObject *object,
                                                                          const SZrTypeValue *key) {
    const SZrTypeValue *cachedValue;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);

    if (!object_node_map_is_ready(object)) {
        return ZR_NULL;
    }

    cachedValue = object_try_get_cached_string_value_unchecked(state, object, key);
    if (cachedValue != ZR_NULL) {
        return cachedValue;
    }

    {
        SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &object->nodeMap, key);
        object_refresh_cached_string_lookup_pair(object, key, pair);
        return pair != ZR_NULL ? &pair->value : ZR_NULL;
    }
}

static ZR_FORCE_INLINE const SZrTypeValue *object_get_prototype_value_unchecked(SZrState *state,
                                                                                SZrObjectPrototype *prototype,
                                                                                const SZrTypeValue *key,
                                                                                TZrBool includeInherited) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);

    while (prototype != ZR_NULL) {
        const SZrTypeValue *value = object_get_own_value_unchecked(state, &prototype->super, key);
        if (value != ZR_NULL) {
            return value;
        }

        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

static TZrBool object_allows_dynamic_member_write(const SZrObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->prototype == ZR_NULL || object->prototype->dynamicMemberCapable;
}

static TZrBool object_can_use_direct_index_fallback(const SZrObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->prototype == ZR_NULL || object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY;
}

static ZR_FORCE_INLINE TZrBool object_can_use_direct_index_fallback_unchecked(const SZrObject *object) {
    ZR_ASSERT(object != ZR_NULL);
    return object->prototype == ZR_NULL || object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY;
}

static ZR_FORCE_INLINE TZrBool object_value_is_struct_instance_local(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT;
}

static ZR_FORCE_INLINE TZrBool object_cached_readonly_inline_get_fast_shape_matches(
        const SZrObjectKnownNativeDirectDispatch *directDispatch) {
    return (TZrBool)(directDispatch != ZR_NULL &&
                     directDispatch->readonlyInlineGetFastCallback != ZR_NULL &&
                     directDispatch->usesReceiver &&
                     directDispatch->rawArgumentCount == 2u &&
                     (directDispatch->reserved0 & ZR_OBJECT_CACHED_INDEX_READONLY_INLINE_GET_FLAGS) ==
                             ZR_OBJECT_CACHED_INDEX_READONLY_INLINE_GET_FLAGS);
}

static ZR_FORCE_INLINE TZrBool object_cached_readonly_inline_set_fast_shape_matches(
        const SZrObjectKnownNativeDirectDispatch *directDispatch) {
    return (TZrBool)(directDispatch != ZR_NULL &&
                     directDispatch->readonlyInlineSetNoResultFastCallback != ZR_NULL &&
                     directDispatch->usesReceiver &&
                     directDispatch->rawArgumentCount == 3u &&
                     (directDispatch->reserved0 & ZR_OBJECT_CACHED_INDEX_READONLY_INLINE_SET_FLAGS) ==
                             ZR_OBJECT_CACHED_INDEX_READONLY_INLINE_SET_FLAGS);
}

static ZR_FORCE_INLINE TZrBool object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(
        SZrState *state,
        const SZrObject *object,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        SZrTypeValue *result) {
    const SZrObjectKnownNativeDirectDispatch *directDispatch;
    FZrObjectKnownNativeReadonlyInlineGetFastCallback fastCallback;
    TZrBool success = ZR_FALSE;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    if (state->debugHookSignal != 0u ||
        object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT ||
        object->prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    directDispatch = &object->prototype->indexContract.getByIndexKnownNativeDirectDispatch;
    fastCallback = directDispatch->readonlyInlineGetFastCallback;
    if (fastCallback == ZR_NULL ||
        ((directDispatch->reserved1 &
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_GET_FAST_READY) == 0u &&
         !object_cached_readonly_inline_get_fast_shape_matches(directDispatch))) {
        return ZR_FALSE;
    }

    success = fastCallback(state, receiver, key, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_try_call_hot_cached_known_native_set_by_index_readonly_inline_stack_operands(
        SZrState *state,
        const SZrObject *object,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        const SZrTypeValue *value) {
    const SZrObjectKnownNativeDirectDispatch *directDispatch;
    FZrObjectKnownNativeReadonlyInlineSetNoResultFastCallback fastCallback;
    TZrBool success = ZR_FALSE;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);

    if (state->debugHookSignal != 0u ||
        object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT ||
        object->prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    directDispatch = &object->prototype->indexContract.setByIndexKnownNativeDirectDispatch;
    fastCallback = directDispatch->readonlyInlineSetNoResultFastCallback;
    if (fastCallback == ZR_NULL ||
        ((directDispatch->reserved1 &
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_SET_NO_RESULT_FAST_READY) == 0u &&
         !object_cached_readonly_inline_set_fast_shape_matches(directDispatch))) {
        return ZR_FALSE;
    }

    success = fastCallback(state, receiver, key, value);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_try_get_by_index_readonly_inline_fast_stack_operands(
        SZrState *state,
        SZrTypeValue *receiver,
        const SZrTypeValue *key,
        SZrTypeValue *result) {
    SZrObject *object;

    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || result == ZR_NULL ||
        (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object == ZR_NULL ||
        ZR_VALUE_IS_TYPE_INT(key->type)) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(
            state, object, receiver, key, result);
}

static ZR_FORCE_INLINE TZrBool object_try_set_by_index_readonly_inline_fast_stack_operands(
        SZrState *state,
        SZrTypeValue *receiver,
        const SZrTypeValue *key,
        const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || value == ZR_NULL ||
        (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object == ZR_NULL ||
        ZR_VALUE_IS_TYPE_INT(key->type)) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object_try_call_hot_cached_known_native_set_by_index_readonly_inline_stack_operands(
            state, object, receiver, key, value);
}

static ZR_FORCE_INLINE TZrBool object_try_get_cached_known_native_index_contract_callable(
        SZrFunction *function,
        SZrRawObject *cachedCallable,
        FZrNativeFunction cachedNativeFunction,
        SZrRawObject **outCallableObject,
        FZrNativeFunction *outNativeFunction) {
    SZrRawObject *rawCallable;

    if (outCallableObject != ZR_NULL) {
        *outCallableObject = ZR_NULL;
    }
    if (outNativeFunction != ZR_NULL) {
        *outNativeFunction = ZR_NULL;
    }

    if (function == ZR_NULL || outCallableObject == ZR_NULL || outNativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    rawCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    if (rawCallable == cachedCallable && cachedNativeFunction != ZR_NULL) {
        *outCallableObject = cachedCallable;
        *outNativeFunction = cachedNativeFunction;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_get_by_index_readonly_inline_mode(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        SZrTypeValue *result,
        TZrBool stackOperandsGuaranteed) {
    FZrObjectKnownNativeReadonlyInlineGetFastCallback fastCallback;
    TZrBool success = ZR_FALSE;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(directDispatch != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    if (state->debugHookSignal != 0u) {
        return ZR_FALSE;
    }

    fastCallback = directDispatch->readonlyInlineGetFastCallback;
    if (fastCallback == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((directDispatch->reserved1 & ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_GET_FAST_READY) ==
            0u &&
        !object_cached_readonly_inline_get_fast_shape_matches(directDispatch)) {
        return ZR_FALSE;
    }
    if (!stackOperandsGuaranteed &&
        (!object_value_resides_on_vm_stack(state, receiver) || !object_value_resides_on_vm_stack(state, key))) {
        return ZR_FALSE;
    }
    success = fastCallback(state, receiver, key, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_get_by_index_readonly_inline(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        SZrTypeValue *result) {
    return object_try_call_cached_known_native_get_by_index_readonly_inline_mode(
            state, directDispatch, receiver, key, result, ZR_FALSE);
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_get_by_index_readonly_inline_stack_operands(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        SZrTypeValue *result) {
    return object_try_call_cached_known_native_get_by_index_readonly_inline_mode(
            state, directDispatch, receiver, key, result, ZR_TRUE);
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_set_by_index_readonly_inline_mode(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        const SZrTypeValue *value,
        TZrBool stackOperandsGuaranteed) {
    FZrObjectKnownNativeReadonlyInlineSetNoResultFastCallback fastCallback;
    TZrBool success = ZR_FALSE;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(directDispatch != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);

    if (state->debugHookSignal != 0u) {
        return ZR_FALSE;
    }

    fastCallback = directDispatch->readonlyInlineSetNoResultFastCallback;
    if (fastCallback == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((directDispatch->reserved1 &
         ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_SET_NO_RESULT_FAST_READY) == 0u &&
        !object_cached_readonly_inline_set_fast_shape_matches(directDispatch)) {
        return ZR_FALSE;
    }
    if (!stackOperandsGuaranteed &&
        (!object_value_resides_on_vm_stack(state, receiver) || !object_value_resides_on_vm_stack(state, key) ||
         !object_value_resides_on_vm_stack(state, value))) {
        return ZR_FALSE;
    }
    success = fastCallback(state, receiver, key, value);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_set_by_index_readonly_inline(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        const SZrTypeValue *value) {
    return object_try_call_cached_known_native_set_by_index_readonly_inline_mode(
            state, directDispatch, receiver, key, value, ZR_FALSE);
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_set_by_index_readonly_inline_stack_operands(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        const SZrTypeValue *value) {
    return object_try_call_cached_known_native_set_by_index_readonly_inline_mode(
            state, directDispatch, receiver, key, value, ZR_TRUE);
}

static ZR_FORCE_INLINE TZrBool object_try_cache_known_native_index_contract_callable(
        SZrState *state,
        SZrFunction *function,
        TZrSize expectedArgumentCount,
        SZrRawObject **cachedCallableSlot,
        FZrNativeFunction *cachedNativeFunctionSlot,
        SZrObjectKnownNativeDirectDispatch *cachedDirectDispatchSlot,
        SZrRawObject **outCallableObject,
        FZrNativeFunction *outNativeFunction) {
    SZrRawObject *rawCallable;
    SZrClosureNative *nativeClosure;
    FZrNativeFunction nativeFunction;

    if (outCallableObject != ZR_NULL) {
        *outCallableObject = ZR_NULL;
    }
    if (outNativeFunction != ZR_NULL) {
        *outNativeFunction = ZR_NULL;
    }

    if (state == ZR_NULL || function == ZR_NULL || cachedCallableSlot == ZR_NULL || cachedNativeFunctionSlot == ZR_NULL ||
        cachedDirectDispatchSlot == ZR_NULL || outCallableObject == ZR_NULL || outNativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    rawCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    if (*cachedCallableSlot == rawCallable && *cachedNativeFunctionSlot != ZR_NULL) {
        *outCallableObject = rawCallable;
        *outNativeFunction = *cachedNativeFunctionSlot;
        return ZR_TRUE;
    }

    *cachedCallableSlot = ZR_NULL;
    *cachedNativeFunctionSlot = ZR_NULL;
    memset(cachedDirectDispatchSlot, 0, sizeof(*cachedDirectDispatchSlot));
    if (rawCallable == ZR_NULL ||
        !rawCallable->isNative ||
        rawCallable->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        !ZrCore_RawObject_IsPermanent(state, rawCallable)) {
        return ZR_FALSE;
    }

    nativeClosure = ZR_CAST(SZrClosureNative *, rawCallable);
    nativeFunction = nativeClosure != ZR_NULL ? nativeClosure->nativeFunction : ZR_NULL;
    if (nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    *cachedCallableSlot = rawCallable;
    *cachedNativeFunctionSlot = nativeFunction;
    (void)ZrCore_Object_TryResolveKnownNativeDirectDispatch(
            state,
            rawCallable,
            expectedArgumentCount,
            cachedDirectDispatchSlot);
    *outCallableObject = rawCallable;
    *outNativeFunction = nativeFunction;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_resolve_known_native_index_contract_callable(
        SZrState *state,
        SZrFunction *function,
        TZrSize expectedArgumentCount,
        SZrRawObject **cachedCallableSlot,
        FZrNativeFunction *cachedNativeFunctionSlot,
        SZrObjectKnownNativeDirectDispatch *cachedDirectDispatchSlot,
        SZrRawObject **outCallableObject,
        FZrNativeFunction *outNativeFunction) {
    if (object_try_get_cached_known_native_index_contract_callable(
                function,
                cachedCallableSlot != ZR_NULL ? *cachedCallableSlot : ZR_NULL,
                cachedNativeFunctionSlot != ZR_NULL ? *cachedNativeFunctionSlot : ZR_NULL,
                outCallableObject,
                outNativeFunction)) {
        return ZR_TRUE;
    }

    return object_try_cache_known_native_index_contract_callable(
            state,
            function,
            expectedArgumentCount,
            cachedCallableSlot,
            cachedNativeFunctionSlot,
            cachedDirectDispatchSlot,
            outCallableObject,
            outNativeFunction);
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_get_by_index_dispatch(
        SZrState *state,
        SZrFunction *function,
        SZrIndexContract *indexContract,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        SZrTypeValue *result) {
    SZrRawObject *nativeCallableObject = ZR_NULL;
    FZrNativeFunction nativeFunction = ZR_NULL;

    if (state == ZR_NULL || function == ZR_NULL || indexContract == ZR_NULL || directDispatch == ZR_NULL ||
        receiver == ZR_NULL || key == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_Object_CallDirectBindingFastOneArgument(state, directDispatch, receiver, key, result)) {
        return ZR_TRUE;
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (!object_try_get_cached_known_native_index_contract_callable(function,
                                                                    indexContract->getByIndexKnownNativeCallable,
                                                                    indexContract->getByIndexKnownNativeFunction,
                                                                    &nativeCallableObject,
                                                                    &nativeFunction)) {
        return ZR_FALSE;
    }

    return ZrCore_Object_CallKnownNativeFastOneArgument(state,
                                                        nativeCallableObject,
                                                        nativeFunction,
                                                        directDispatch,
                                                        receiver,
                                                        key,
                                                        result);
}

static ZR_FORCE_INLINE TZrBool object_try_call_cached_known_native_set_by_index_dispatch(
        SZrState *state,
        SZrFunction *function,
        SZrIndexContract *indexContract,
        const SZrObjectKnownNativeDirectDispatch *directDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *key,
        const SZrTypeValue *value) {
    SZrRawObject *nativeCallableObject = ZR_NULL;
    FZrNativeFunction nativeFunction = ZR_NULL;
    SZrTypeValue ignoredResult;

    if (state == ZR_NULL || function == ZR_NULL || indexContract == ZR_NULL || directDispatch == ZR_NULL ||
        receiver == ZR_NULL || key == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_Object_CallDirectBindingFastTwoArgumentsNoResult(state, directDispatch, receiver, key, value)) {
        return ZR_TRUE;
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&ignoredResult);
    if (ZrCore_Object_CallDirectBindingFastTwoArguments(state, directDispatch, receiver, key, value, &ignoredResult)) {
        return ZR_TRUE;
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (!object_try_get_cached_known_native_index_contract_callable(function,
                                                                    indexContract->setByIndexKnownNativeCallable,
                                                                    indexContract->setByIndexKnownNativeFunction,
                                                                    &nativeCallableObject,
                                                                    &nativeFunction)) {
        return ZR_FALSE;
    }

    return ZrCore_Object_CallKnownNativeFastTwoArguments(state,
                                                         nativeCallableObject,
                                                         nativeFunction,
                                                         directDispatch,
                                                         receiver,
                                                         key,
                                                         value,
                                                         &ignoredResult);
}

static TZrBool object_get_index_length(SZrState *state,
                                       SZrTypeValue *receiver,
                                       SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, receiver);
    if (prototype != ZR_NULL && prototype->indexContract.getLengthFunction != ZR_NULL) {
        return ZrCore_Object_CallFunctionWithReceiver(state,
                                                      prototype->indexContract.getLengthFunction,
                                                      receiver,
                                                      ZR_NULL,
                                                      0,
                                                      result);
    }

    if (!object_can_use_direct_index_fallback(object)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, result, (TZrInt64)object->nodeMap.elementCount);
    return ZR_TRUE;
}


SZrObject *ZrCore_Object_New(SZrState *state, SZrObjectPrototype *prototype) {
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrObject), ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = prototype;
    object->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    object->memberVersion = 0;
    ZrCore_HashSet_Construct(&object->nodeMap);
    object_reset_hot_field_pair_cache(object);
    return object;
}

SZrObject *ZrCore_Object_NewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType) {
    // 根据 internalType 选择正确的值类型
    EZrValueType valueType = ZR_VALUE_TYPE_OBJECT;
    if (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        valueType = ZR_VALUE_TYPE_ARRAY;
    }
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, valueType, size, ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY && state != ZR_NULL && state->global != ZR_NULL)
                                ? state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY]
                                : ZR_NULL;
    object->internalType = internalType;
    object->memberVersion = 0;
    ZrCore_HashSet_Construct(&object->nodeMap);
    object_reset_hot_field_pair_cache(object);
    return object;
}

void ZrCore_Object_Init(struct SZrState *state, SZrObject *object) {
    object_trace("object init enter object=%p state=%p global=%p internalType=%d nodeMap{valid=%d buckets=%p capacity=%llu}",
                 (void *)object,
                 (void *)state,
                 state != ZR_NULL ? (void *)state->global : ZR_NULL,
                 object != ZR_NULL ? (int)object->internalType : -1,
                 object != ZR_NULL ? (int)object->nodeMap.isValid : -1,
                 object != ZR_NULL ? (void *)object->nodeMap.buckets : ZR_NULL,
                 object != ZR_NULL ? (unsigned long long)object->nodeMap.capacity : 0ull);
    object_reset_hot_field_pair_cache(object);
    ZrCore_HashSet_Init(state, &object->nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    object_trace("object init exit object=%p nodeMap{valid=%d buckets=%p capacity=%llu threshold=%llu elementCount=%llu}",
                 (void *)object,
                 object != ZR_NULL ? (int)object->nodeMap.isValid : -1,
                 object != ZR_NULL ? (void *)object->nodeMap.buckets : ZR_NULL,
                 object != ZR_NULL ? (unsigned long long)object->nodeMap.capacity : 0ull,
                 object != ZR_NULL ? (unsigned long long)object->nodeMap.resizeThreshold : 0ull,
                 object != ZR_NULL ? (unsigned long long)object->nodeMap.elementCount : 0ull);
}

SZrObject *ZrCore_Object_CloneStruct(struct SZrState *state, const SZrObject *source) {
    SZrObject *clone = ZR_NULL;
    TZrBool failed = ZR_FALSE;
    TZrBool cloneIgnored = ZR_FALSE;

    if (state == ZR_NULL ||
        source == ZR_NULL ||
        source->internalType != ZR_OBJECT_INTERNAL_TYPE_STRUCT ||
        source->prototype == ZR_NULL) {
        return ZR_NULL;
    }

    clone = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    if (clone == ZR_NULL) {
        return ZR_NULL;
    }
    clone->prototype = source->prototype;
    ZrCore_Object_Init(state, clone);
    clone->memberVersion = source->memberVersion;

    if (state->global != ZR_NULL &&
        !ZrCore_GarbageCollector_IsObjectIgnored(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(clone))) {
        if (!ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(clone))) {
            return ZR_NULL;
        }
        cloneIgnored = ZR_TRUE;
    }

    if (object_node_map_is_ready(source)) {
        for (TZrSize bucketIndex = 0; bucketIndex < source->nodeMap.capacity; bucketIndex++) {
            for (SZrHashKeyValuePair *pair = source->nodeMap.buckets[bucketIndex]; pair != ZR_NULL; pair = pair->next) {
                ZrCore_Object_SetValue(state, clone, &pair->key, &pair->value);
                if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                    failed = ZR_TRUE;
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (cloneIgnored && state->global != ZR_NULL && clone != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(clone));
    }
    return failed ? ZR_NULL : clone;
}

TZrBool ZrCore_Object_CompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2) {
    ZR_UNUSED_PARAMETER(state);
    return object1 == object2;
}

static const SZrTypeValue *object_resolve_storage_key(SZrState *state,
                                                      const SZrTypeValue *key,
                                                      SZrTypeValue *normalizedKey) {
    SZrString *keyString;
    TZrNativeString keyText;
    TZrSize keyLength;
    SZrString *canonicalString;

    if (key == ZR_NULL || normalizedKey == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING || key->value.object == ZR_NULL ||
        state == ZR_NULL) {
        return key;
    }

    keyString = ZR_CAST_STRING(state, key->value.object);
    if (keyString == ZR_NULL) {
        return key;
    }

    keyText = ZrCore_String_GetNativeString(keyString);
    keyLength = ZrCore_String_GetByteLength(keyString);
    canonicalString = ZrCore_String_Create(state, keyText != ZR_NULL ? keyText : "", keyLength);
    if (canonicalString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, normalizedKey, ZR_CAST_RAW_OBJECT_AS_SUPER(canonicalString));
    normalizedKey->type = ZR_VALUE_TYPE_STRING;
    return normalizedKey;
}


void ZrCore_Object_SetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key, const SZrTypeValue *value) {
    SZrTypeValue normalizedKey;
    const SZrTypeValue *storageKey = key;
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrBool objectPinned = ZR_FALSE;
    TZrBool keyPinned = ZR_FALSE;
    TZrBool valuePinned = ZR_FALSE;

    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrCore_Log_Error(state, "attempt to set value with null key");
        return;
    }
    if (!object_node_map_is_ready(object)) {
        object_trace("set value nodeMap not ready object=%p state=%p keyType=%d keyObject=%p nodeMap{valid=%d buckets=%p capacity=%llu}",
                     (void *)object,
                     (void *)state,
                     key != ZR_NULL ? (int)key->type : -1,
                     key != ZR_NULL ? (void *)key->value.object : ZR_NULL,
                     (int)object->nodeMap.isValid,
                     (void *)object->nodeMap.buckets,
                     (unsigned long long)object->nodeMap.capacity);
        if (state == ZR_NULL) {
            return;
        }
        ZrCore_Object_Init(state, object);
        if (!object_node_map_is_ready(object)) {
            object_trace("set value init failed object=%p state=%p threadStatus=%d nodeMap{valid=%d buckets=%p capacity=%llu threshold=%llu}",
                         (void *)object,
                         (void *)state,
                         state != ZR_NULL ? (int)state->threadStatus : -1,
                         (int)object->nodeMap.isValid,
                         (void *)object->nodeMap.buckets,
                         (unsigned long long)object->nodeMap.capacity,
                         (unsigned long long)object->nodeMap.resizeThreshold);
            ZrCore_Log_Error(state, "failed to initialize object storage");
            return;
        }
    }

    storageKey = object_resolve_storage_key(state, key, &normalizedKey);
    if (storageKey == ZR_NULL) {
        ZrCore_Log_Error(state, "failed to normalize object storage key");
        return;
    }

    nodeMap = &object->nodeMap;
    pair = ZrCore_HashSet_Find(state, nodeMap, storageKey);
    if (pair != ZR_NULL) {
        ZrCore_Object_SetExistingPairValueUnchecked(state, object, pair, value);
        object->memberVersion++;
        return;
    }

    if (!object_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinned) ||
        !object_pin_value_object(state, storageKey, &keyPinned) ||
        !object_pin_value_object(state, value, &valuePinned)) {
        object_unpin_value_object(state->global, value, valuePinned);
        object_unpin_value_object(state->global, storageKey, keyPinned);
        object_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
        ZrCore_Log_Error(state, "failed to pin object storage write operands");
        return;
    }

    pair = ZrCore_HashSet_Add(state, nodeMap, storageKey);
    if (pair == ZR_NULL) {
        object_unpin_value_object(state->global, value, valuePinned);
        object_unpin_value_object(state->global, storageKey, keyPinned);
        object_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
        ZrCore_Log_Error(state, "failed to allocate object storage entry");
        return;
    }
    ZrCore_Value_Copy(state, &pair->value, value);
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->key);
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->value);
    object->memberVersion++;
    object_refresh_cached_string_lookup_pair(object, storageKey, pair);
    object_refresh_hidden_items_object_cache(state, object, storageKey, pair);
    object_unpin_value_object(state->global, value, valuePinned);
    object_unpin_value_object(state->global, storageKey, keyPinned);
    object_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
}

void ZrCore_Object_SetExistingPairValueUnchecked(SZrState *state,
                                                 SZrObject *object,
                                                 SZrHashKeyValuePair *pair,
                                                 const SZrTypeValue *value) {
    TZrBool shouldRefreshHiddenItemsCache;

    if (state == ZR_NULL || object == ZR_NULL || pair == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if (object_try_set_existing_pair_plain_value_fast_unchecked(state, object, pair, value)) {
        object_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
        return;
    }

    shouldRefreshHiddenItemsCache =
            object->cachedHiddenItemsPair == pair || object->cachedHiddenItemsObject != ZR_NULL;

    if (pair->value.ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
        pair->value.ownershipControl == ZR_NULL &&
        pair->value.ownershipWeakRef == ZR_NULL &&
        value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
        value->ownershipControl == ZR_NULL &&
        value->ownershipWeakRef == ZR_NULL) {
        object_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
        if (&pair->value != value) {
            pair->value = *value;
        }
    } else {
        ZrCore_Value_Copy(state, &pair->value, value);
    }

    if (ZrCore_Value_IsGarbageCollectable(&pair->value)) {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->value);
    }
    if (object->cachedStringLookupPair != pair) {
        object_refresh_cached_string_lookup_pair(object, &pair->key, pair);
    }
    if (shouldRefreshHiddenItemsCache) {
        object_refresh_hidden_items_object_cache(state, object, &pair->key, pair);
    }
}

#if defined(ZR_DEBUG)
static TZrBool object_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_CORE_BOOTSTRAP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void object_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!object_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-object] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
#endif

const SZrTypeValue *ZrCore_Object_GetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    const SZrTypeValue *resolvedValue;

    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrCore_Log_Error(state, "attempt to get value with null key");
        return ZR_NULL;
    }
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    resolvedValue = object_get_own_value(state, object, key);
    if (resolvedValue != ZR_NULL) {
        return resolvedValue;
    }

    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return object_get_prototype_value_unchecked(state, ((SZrObjectPrototype *)object)->superPrototype, key, ZR_TRUE);
    }

    return object_get_prototype_value_unchecked(state, object->prototype, key, ZR_TRUE);
}

// 创建基础 ObjectPrototype
SZrObjectPrototype *ZrCore_ObjectPrototype_New(SZrState *state, SZrString *name, EZrObjectPrototypeType type) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 ObjectPrototype 对象
    SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrObjectPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)objectBase;
    
    // 初始化哈希集
    ZrCore_HashSet_Init(state, &prototype->super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->name = name;
    prototype->type = type;
    prototype->superPrototype = ZR_NULL;
    prototype->memberDescriptors = ZR_NULL;
    prototype->memberDescriptorCount = 0;
    prototype->memberDescriptorCapacity = 0;
    memset(&prototype->indexContract, 0, sizeof(prototype->indexContract));
    memset(&prototype->iterableContract, 0, sizeof(prototype->iterableContract));
    memset(&prototype->iteratorContract, 0, sizeof(prototype->iteratorContract));
    prototype->protocolMask = 0;
    prototype->dynamicMemberCapable = ZR_FALSE;
    prototype->reserved0 = 0;
    prototype->reserved1 = 0;
    prototype->modifierFlags = 0;
    prototype->nextVirtualSlotIndex = 0;
    prototype->nextPropertyIdentity = 0;
    prototype->managedFields = ZR_NULL;
    prototype->managedFieldCount = 0;
    prototype->managedFieldCapacity = 0;
    
    // 初始化 metaTable
    ZrCore_MetaTable_Construct(&prototype->metaTable);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 创建 StructPrototype
SZrStructPrototype *ZrCore_StructPrototype_New(SZrState *state, SZrString *name) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 StructPrototype 对象
    SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrStructPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrStructPrototype *prototype = (SZrStructPrototype *)objectBase;
    
    // 初始化哈希集
    ZrCore_HashSet_Init(state, &prototype->super.super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->super.name = name;
    prototype->super.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype->super.superPrototype = ZR_NULL;
    prototype->super.memberDescriptors = ZR_NULL;
    prototype->super.memberDescriptorCount = 0;
    prototype->super.memberDescriptorCapacity = 0;
    memset(&prototype->super.indexContract, 0, sizeof(prototype->super.indexContract));
    memset(&prototype->super.iterableContract, 0, sizeof(prototype->super.iterableContract));
    memset(&prototype->super.iteratorContract, 0, sizeof(prototype->super.iteratorContract));
    prototype->super.protocolMask = 0;
    prototype->super.dynamicMemberCapable = ZR_FALSE;
    prototype->super.reserved0 = 0;
    prototype->super.reserved1 = 0;
    prototype->super.modifierFlags = 0;
    prototype->super.nextVirtualSlotIndex = 0;
    prototype->super.nextPropertyIdentity = 0;
    prototype->super.managedFields = ZR_NULL;
    prototype->super.managedFieldCount = 0;
    prototype->super.managedFieldCapacity = 0;
    
    // 初始化 metaTable
    ZrCore_MetaTable_Construct(&prototype->super.metaTable);
    
    // 初始化 keyOffsetMap
    ZrCore_HashSet_Construct(&prototype->keyOffsetMap);
    ZrCore_HashSet_Init(state, &prototype->keyOffsetMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 设置继承关系
void ZrCore_ObjectPrototype_SetSuper(SZrState *state, SZrObjectPrototype *prototype, SZrObjectPrototype *superPrototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    prototype->superPrototype = superPrototype;
    prototype->super.memberVersion++;
}

// 初始化元表
void ZrCore_ObjectPrototype_InitMetaTable(SZrState *state, SZrObjectPrototype *prototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    ZrCore_MetaTable_Construct(&prototype->metaTable);
}

// 向 StructPrototype 添加字段
void ZrCore_StructPrototype_AddField(SZrState *state, SZrStructPrototype *prototype, SZrString *fieldName, TZrSize offset) {
    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    
    // 创建键值（字段名）
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 创建值（偏移量）
    SZrTypeValue value;
    ZrCore_Value_InitAsUInt(state, &value, (TZrUInt64)offset);
    
    // 添加到 keyOffsetMap
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &prototype->keyOffsetMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &prototype->keyOffsetMap, &key);
        if (pair == ZR_NULL) {
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, &value);
}

// 向 Prototype 添加元函数
void ZrCore_ObjectPrototype_AddMeta(SZrState *state, SZrObjectPrototype *prototype, EZrMetaType metaType, SZrFunction *function) {
    if (state == ZR_NULL || prototype == ZR_NULL || function == ZR_NULL) {
        return;
    }
    
    if (metaType >= ZR_META_ENUM_MAX) {
        return;
    }
    
    // 创建 Meta 对象
    SZrGlobalState *global = state->global;
    SZrMeta *meta = (SZrMeta *)ZrCore_Memory_RawMallocWithType(global, sizeof(SZrMeta), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (meta == ZR_NULL) {
        return;
    }
    
    meta->metaType = metaType;
    meta->function = function;
    
    // 添加到 metaTable
    prototype->metaTable.metas[metaType] = meta;
}

void ZrCore_ObjectPrototype_AddManagedField(SZrState *state,
                                      SZrObjectPrototype *prototype,
                                      SZrString *fieldName,
                                      TZrUInt32 fieldOffset,
                                      TZrUInt32 fieldSize,
                                      TZrUInt32 ownershipQualifier,
                                      TZrBool callsClose,
                                      TZrBool callsDestructor,
                                      TZrUInt32 declarationOrder) {
    SZrManagedFieldInfo *fieldInfo;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    if (!ensure_managed_field_capacity(state, prototype, prototype->managedFieldCount + 1)) {
        return;
    }

    fieldInfo = &prototype->managedFields[prototype->managedFieldCount++];
    fieldInfo->name = fieldName;
    fieldInfo->fieldOffset = fieldOffset;
    fieldInfo->fieldSize = fieldSize;
    fieldInfo->ownershipQualifier = ownershipQualifier;
    fieldInfo->callsClose = callsClose;
    fieldInfo->callsDestructor = callsDestructor;
    fieldInfo->declarationOrder = declarationOrder;
}

const SZrMemberDescriptor *ZrCore_ObjectPrototype_FindMemberDescriptor(SZrObjectPrototype *prototype,
                                                                       SZrString *memberName,
                                                                       TZrBool includeInherited) {
    while (prototype != ZR_NULL) {
        for (TZrUInt32 index = 0; index < prototype->memberDescriptorCount; index++) {
            SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[index];
            if (descriptor->name != ZR_NULL && memberName != ZR_NULL &&
                ZrCore_String_Equal(descriptor->name, memberName)) {
                return descriptor;
            }
        }

        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

TZrBool ZrCore_ObjectPrototype_AddMemberDescriptor(struct SZrState *state,
                                                   SZrObjectPrototype *prototype,
                                                   const SZrMemberDescriptor *descriptor) {
    SZrMemberDescriptor *newEntries;
    TZrUInt32 newCapacity;
    TZrSize bytes;

    if (state == ZR_NULL || prototype == ZR_NULL || descriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    if (prototype->memberDescriptorCount >= prototype->memberDescriptorCapacity) {
        newCapacity = prototype->memberDescriptorCapacity > 0
                              ? prototype->memberDescriptorCapacity * ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR
                              : ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY;
        bytes = sizeof(SZrMemberDescriptor) * newCapacity;
        newEntries = (SZrMemberDescriptor *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                             bytes,
                                                                             ZR_MEMORY_NATIVE_TYPE_OBJECT);
        if (newEntries == ZR_NULL) {
            return ZR_FALSE;
        }

        memset(newEntries, 0, bytes);
        if (prototype->memberDescriptors != ZR_NULL && prototype->memberDescriptorCount > 0) {
            memcpy(newEntries,
                   prototype->memberDescriptors,
                   sizeof(SZrMemberDescriptor) * prototype->memberDescriptorCount);
            ZrCore_Memory_RawFreeWithType(state->global,
                                          prototype->memberDescriptors,
                                          sizeof(SZrMemberDescriptor) * prototype->memberDescriptorCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }

        prototype->memberDescriptors = newEntries;
        prototype->memberDescriptorCapacity = newCapacity;
    }

    prototype->memberDescriptors[prototype->memberDescriptorCount++] = *descriptor;
    prototype->super.memberVersion++;
    return ZR_TRUE;
}

TZrBool ZrCore_Object_GetMemberWithKeyUnchecked(struct SZrState *state,
                                                SZrTypeValue *receiver,
                                                struct SZrString *memberName,
                                                const SZrTypeValue *memberKey,
                                                SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *resolvedValue;
    const SZrMemberDescriptor *descriptor;
    TZrBool isPrototypeReceiver;

    object_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);
    ZR_ASSERT(memberKey != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT ||
              receiver->type == ZR_VALUE_TYPE_ARRAY ||
              receiver->type == ZR_VALUE_TYPE_STRING);
    ZR_ASSERT(memberKey->type == ZR_VALUE_TYPE_STRING);

    object = (receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY)
                     ? ZR_CAST_OBJECT(state, receiver->value.object)
                     : ZR_NULL;

    resolvedValue = object != ZR_NULL ? object_get_own_value_unchecked(state, object, memberKey) : ZR_NULL;
    if (resolvedValue != ZR_NULL) {
        if (object != ZR_NULL && object_module_guard_pending_export(state, object, memberName)) {
            ZrCore_Value_ResetAsNull(result);
            return state->threadStatus != ZR_THREAD_STATUS_FINE ? ZR_TRUE : ZR_FALSE;
        }
        object_copy_value_profiled(state, result, resolvedValue);
        return ZR_TRUE;
    }

    if (object != ZR_NULL) {
        (void)object_module_guard_pending_export(state, object, memberName);
    }

    prototype = (object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE)
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype_unchecked(state, receiver);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;
    isPrototypeReceiver =
            (TZrBool)(object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);

    if (descriptor != ZR_NULL) {
        if (descriptor->isStatic && !isPrototypeReceiver) {
            return ZR_FALSE;
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            descriptor->getterFunction != ZR_NULL) {
            return ZrCore_Object_CallFunctionWithReceiver(state,
                                                          descriptor->getterFunction,
                                                          receiver,
                                                          ZR_NULL,
                                                          0,
                                                          result);
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            descriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH) {
            return object_get_index_length(state, receiver, result);
        }
    }

    resolvedValue = prototype != ZR_NULL ? object_get_prototype_value_unchecked(state, prototype, memberKey, ZR_TRUE) : ZR_NULL;
    if (resolvedValue != ZR_NULL) {
        if (descriptor == ZR_NULL || !descriptor->isStatic || isPrototypeReceiver) {
            ZrCore_Value_Copy(state, result, resolvedValue);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    return ZR_FALSE;
}

TZrBool ZrCore_Object_TryGetMemberWithKeyFastUnchecked(struct SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       struct SZrString *memberName,
                                                       const SZrTypeValue *memberKey,
                                                       SZrTypeValue *result,
                                                       TZrBool *outHandled) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *resolvedValue;
    const SZrMemberDescriptor *descriptor;
    TZrBool isPrototypeReceiver;
    SZrTypeValue localMemberKey;
    const SZrTypeValue *effectiveMemberKey = memberKey;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(outHandled != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);
    ZR_ASSERT(receiver->value.object != ZR_NULL);
    ZR_ASSERT(memberKey == ZR_NULL || memberKey->type == ZR_VALUE_TYPE_STRING);

    *outHandled = ZR_FALSE;
    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL || object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_FALSE;
    }

    resolvedValue = effectiveMemberKey == ZR_NULL
                            ? object_try_get_cached_string_value_by_name_pointer_unchecked(object, memberName)
                            : ZR_NULL;
    if (resolvedValue == ZR_NULL) {
        if (effectiveMemberKey == ZR_NULL) {
            object_make_string_key_cached_unchecked(state, memberName, &localMemberKey);
            effectiveMemberKey = &localMemberKey;
        }
        resolvedValue = object_get_own_value_unchecked(state, object, effectiveMemberKey);
    }
    if (resolvedValue != ZR_NULL) {
        ZrCore_Value_Copy(state, result, resolvedValue);
        *outHandled = ZR_TRUE;
        return ZR_TRUE;
    }

    prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype_unchecked(state, receiver);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;
    isPrototypeReceiver = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ? ZR_TRUE : ZR_FALSE;

    if (descriptor != ZR_NULL) {
        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            (descriptor->getterFunction != ZR_NULL ||
             descriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH)) {
            return ZR_FALSE;
        }

        if (descriptor->isStatic && !isPrototypeReceiver) {
            *outHandled = ZR_TRUE;
            return ZR_FALSE;
        }
    }

    resolvedValue = prototype != ZR_NULL
                            ? object_get_prototype_value_unchecked(state, prototype, effectiveMemberKey, ZR_TRUE)
                            : ZR_NULL;
    if (resolvedValue != ZR_NULL) {
        if (descriptor == ZR_NULL || !descriptor->isStatic || isPrototypeReceiver) {
            ZrCore_Value_Copy(state, result, resolvedValue);
            *outHandled = ZR_TRUE;
            return ZR_TRUE;
        }
        *outHandled = ZR_TRUE;
        return ZR_FALSE;
    }

    *outHandled = ZR_TRUE;
    return ZR_FALSE;
}

TZrBool ZrCore_Object_SetMember(struct SZrState *state,
                                SZrTypeValue *receiver,
                                struct SZrString *memberName,
                                const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object_make_string_key_unchecked(state, memberName, &key);
    return ZrCore_Object_SetMemberWithKeyUnchecked(state, receiver, memberName, &key, value);
}

TZrBool ZrCore_Object_GetMember(struct SZrState *state,
                                SZrTypeValue *receiver,
                                struct SZrString *memberName,
                                SZrTypeValue *result) {
    SZrTypeValue key;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((receiver->type != ZR_VALUE_TYPE_OBJECT &&
         receiver->type != ZR_VALUE_TYPE_ARRAY &&
         receiver->type != ZR_VALUE_TYPE_STRING) ||
        receiver->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object_make_string_key_unchecked(state, memberName, &key);
    return ZrCore_Object_GetMemberWithKeyUnchecked(state, receiver, memberName, &key, result);
}

SZrFunction *ZrCore_Object_GetMemberCachedCallableTargetUnchecked(struct SZrState *state,
                                                                  SZrObjectPrototype *ownerPrototype,
                                                                  TZrUInt32 descriptorIndex) {
    const SZrMemberDescriptor *descriptor;
    const SZrTypeValue *resolvedValue;
    SZrTypeValue memberKey;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(ownerPrototype != ZR_NULL);

    if (ownerPrototype == ZR_NULL || ownerPrototype->memberDescriptors == ZR_NULL ||
        descriptorIndex >= ownerPrototype->memberDescriptorCount) {
        return ZR_NULL;
    }

    descriptor = &ownerPrototype->memberDescriptors[descriptorIndex];
    if (descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }
    if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
        (descriptor->getterFunction != ZR_NULL ||
         descriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH)) {
        return ZR_NULL;
    }

    resolvedValue = object_try_get_cached_string_value_by_name_pointer_unchecked(&ownerPrototype->super, descriptor->name);
    if (resolvedValue == ZR_NULL) {
        object_make_string_key_unchecked(state, descriptor->name, &memberKey);
        resolvedValue = object_get_own_value_unchecked(state, &ownerPrototype->super, &memberKey);
    }
    if (resolvedValue == ZR_NULL ||
        (resolvedValue->type != ZR_VALUE_TYPE_FUNCTION && resolvedValue->type != ZR_VALUE_TYPE_CLOSURE) ||
        resolvedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST(SZrFunction *, resolvedValue->value.object);
}

TZrBool ZrCore_Object_GetMemberCachedCallableUnchecked(struct SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       SZrObjectPrototype *ownerPrototype,
                                                       TZrUInt32 descriptorIndex,
                                                       SZrFunction *cachedCallable,
                                                       SZrTypeValue *result) {
    SZrObject *object;
    const SZrMemberDescriptor *descriptor;
    const SZrTypeValue *resolvedValue;
    TZrBool isPrototypeReceiver;
    SZrTypeValue memberKey;

    object_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(ownerPrototype != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT ||
              receiver->type == ZR_VALUE_TYPE_ARRAY ||
              receiver->type == ZR_VALUE_TYPE_STRING);

    if (ownerPrototype == ZR_NULL || ownerPrototype->memberDescriptors == ZR_NULL || cachedCallable == ZR_NULL ||
        descriptorIndex >= ownerPrototype->memberDescriptorCount) {
        return ZR_FALSE;
    }

    descriptor = &ownerPrototype->memberDescriptors[descriptorIndex];
    if (descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    object = (receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY)
                     ? ZR_CAST_OBJECT(state, receiver->value.object)
                     : ZR_NULL;
    isPrototypeReceiver =
            (TZrBool)(object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (descriptor->isStatic && !isPrototypeReceiver) {
        return ZR_FALSE;
    }

    resolvedValue = object != ZR_NULL
                            ? object_try_get_cached_string_value_by_name_pointer_unchecked(object, descriptor->name)
                            : ZR_NULL;
    if (resolvedValue == ZR_NULL) {
        object_make_string_key_unchecked(state, descriptor->name, &memberKey);
        resolvedValue = object != ZR_NULL ? object_get_own_value_unchecked(state, object, &memberKey) : ZR_NULL;
    }
    if (resolvedValue != ZR_NULL) {
        if (object != ZR_NULL && object_module_guard_pending_export(state, object, descriptor->name)) {
            ZrCore_Value_ResetAsNull(result);
            return state->threadStatus != ZR_THREAD_STATUS_FINE ? ZR_TRUE : ZR_FALSE;
        }
        object_copy_value_profiled(state, result, resolvedValue);
        return ZR_TRUE;
    }

    if (object != ZR_NULL) {
        (void)object_module_guard_pending_export(state, object, descriptor->name);
    }
    if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
        (descriptor->getterFunction != ZR_NULL ||
         descriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(cachedCallable));
    return ZR_TRUE;
}

TZrBool ZrCore_Object_GetMemberCachedDescriptorUnchecked(struct SZrState *state,
                                                         SZrTypeValue *receiver,
                                                         SZrObjectPrototype *ownerPrototype,
                                                         TZrUInt32 descriptorIndex,
                                                         SZrTypeValue *result) {
    SZrObject *object;
    const SZrMemberDescriptor *descriptor;
    const SZrTypeValue *resolvedValue;
    TZrBool isPrototypeReceiver;
    SZrTypeValue memberKey;

    object_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(ownerPrototype != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT ||
              receiver->type == ZR_VALUE_TYPE_ARRAY ||
              receiver->type == ZR_VALUE_TYPE_STRING);

    object = (receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY)
                     ? ZR_CAST_OBJECT(state, receiver->value.object)
                     : ZR_NULL;

    if (ownerPrototype == ZR_NULL || ownerPrototype->memberDescriptors == ZR_NULL ||
        descriptorIndex >= ownerPrototype->memberDescriptorCount) {
        return ZR_FALSE;
    }

    descriptor = &ownerPrototype->memberDescriptors[descriptorIndex];
    if (descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    isPrototypeReceiver =
            (TZrBool)(object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (descriptor->isStatic && !isPrototypeReceiver) {
        return ZR_FALSE;
    }

    resolvedValue = object != ZR_NULL
                            ? object_try_get_cached_string_value_by_name_pointer_unchecked(object, descriptor->name)
                            : ZR_NULL;
    if (resolvedValue == ZR_NULL) {
        object_make_string_key_unchecked(state, descriptor->name, &memberKey);
        resolvedValue = object != ZR_NULL ? object_get_own_value_unchecked(state, object, &memberKey) : ZR_NULL;
    }
    if (resolvedValue != ZR_NULL) {
        if (object != ZR_NULL && object_module_guard_pending_export(state, object, descriptor->name)) {
            ZrCore_Value_ResetAsNull(result);
            return state->threadStatus != ZR_THREAD_STATUS_FINE ? ZR_TRUE : ZR_FALSE;
        }
        object_copy_value_profiled(state, result, resolvedValue);
        return ZR_TRUE;
    }

    if (object != ZR_NULL) {
        (void)object_module_guard_pending_export(state, object, descriptor->name);
    }

    if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY && descriptor->getterFunction != ZR_NULL) {
        return ZrCore_Object_CallFunctionWithReceiver(state,
                                                      descriptor->getterFunction,
                                                      receiver,
                                                      ZR_NULL,
                                                      0,
                                                      result);
    }

    if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
        descriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH) {
        return object_get_index_length(state, receiver, result);
    }

    object_make_string_key_unchecked(state, descriptor->name, &memberKey);
    resolvedValue = object_get_prototype_value_unchecked(state, ownerPrototype, &memberKey, ZR_TRUE);
    if (resolvedValue != ZR_NULL) {
        if (!descriptor->isStatic || isPrototypeReceiver) {
            object_copy_value_profiled(state, result, resolvedValue);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    return ZR_FALSE;
}

TZrBool ZrCore_Object_SetMemberWithKeyUnchecked(struct SZrState *state,
                                                SZrTypeValue *receiver,
                                                struct SZrString *memberName,
                                                const SZrTypeValue *memberKey,
                                                const SZrTypeValue *value) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *existingValue;
    const SZrMemberDescriptor *descriptor;
    const SZrManagedFieldInfo *managedField;
    const SZrTypeValue *prototypeValue;
    TZrBool isPrototypeReceiver;

    object_record_helper(state, ZR_PROFILE_HELPER_SET_MEMBER);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);
    ZR_ASSERT(memberKey != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);
    ZR_ASSERT(memberKey->type == ZR_VALUE_TYPE_STRING);

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype_unchecked(state, receiver);
    existingValue = object_get_own_value_unchecked(state, object, memberKey);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;
    managedField = prototype != ZR_NULL
                           ? object_prototype_find_managed_field(prototype, memberName, ZR_TRUE)
                           : ZR_NULL;
    prototypeValue = prototype != ZR_NULL ? object_get_prototype_value_unchecked(state, prototype, memberKey, ZR_TRUE) : ZR_NULL;
    isPrototypeReceiver = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ? ZR_TRUE : ZR_FALSE;

    if (descriptor != ZR_NULL) {
        if (descriptor->isStatic && !isPrototypeReceiver) {
            return ZR_FALSE;
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            descriptor->setterFunction != ZR_NULL) {
            SZrTypeValue ignoredResult;
            ZrCore_Value_ResetAsNull(&ignoredResult);
            return ZrCore_Object_CallFunctionWithReceiver(state,
                                                          descriptor->setterFunction,
                                                          receiver,
                                                          value,
                                                          1,
                                                          &ignoredResult);
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD ||
            descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER) {
            if (!descriptor->isWritable) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(state,
                                   descriptor->isStatic && prototype != ZR_NULL ? &prototype->super : object,
                                   memberKey,
                                   value);
            return ZR_TRUE;
        }
    }

    if (managedField != ZR_NULL || existingValue != ZR_NULL) {
        ZrCore_Object_SetValue(state, object, memberKey, value);
        return ZR_TRUE;
    }

    if (prototypeValue != ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_allows_dynamic_member_write(object)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, memberKey, value);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_SetMemberCachedDescriptorUnchecked(struct SZrState *state,
                                                         SZrTypeValue *receiver,
                                                         SZrObjectPrototype *ownerPrototype,
                                                         TZrUInt32 descriptorIndex,
                                                         const SZrTypeValue *value) {
    SZrObject *object;
    const SZrMemberDescriptor *descriptor;
    const SZrManagedFieldInfo *managedField;
    const SZrTypeValue *existingValue;
    const SZrTypeValue *prototypeValue;
    TZrBool isPrototypeReceiver;
    SZrTypeValue memberKey;

    object_record_helper(state, ZR_PROFILE_HELPER_SET_MEMBER);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(ownerPrototype != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);

    object = ZR_CAST_OBJECT(state, receiver->value.object);

    if (ownerPrototype == ZR_NULL || ownerPrototype->memberDescriptors == ZR_NULL ||
        descriptorIndex >= ownerPrototype->memberDescriptorCount) {
        return ZR_FALSE;
    }

    descriptor = &ownerPrototype->memberDescriptors[descriptorIndex];
    if (descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    object_make_string_key_unchecked(state, descriptor->name, &memberKey);
    isPrototypeReceiver =
            (TZrBool)(object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (descriptor->isStatic && !isPrototypeReceiver) {
        return ZR_FALSE;
    }

    if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY && descriptor->setterFunction != ZR_NULL) {
        SZrTypeValue ignoredResult;
        ZrCore_Value_ResetAsNull(&ignoredResult);
        return ZrCore_Object_CallFunctionWithReceiver(state,
                                                      descriptor->setterFunction,
                                                      receiver,
                                                      value,
                                                      1,
                                                      &ignoredResult);
    }

    if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD ||
        descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER) {
        if (!descriptor->isWritable) {
            return ZR_FALSE;
        }

        ZrCore_Object_SetValue(state,
                               descriptor->isStatic ? &ownerPrototype->super : object,
                               &memberKey,
                               value);
        return ZR_TRUE;
    }

    existingValue = object_get_own_value_unchecked(state, object, &memberKey);
    managedField = object_prototype_find_managed_field(ownerPrototype, descriptor->name, ZR_TRUE);
    if (managedField != ZR_NULL || existingValue != ZR_NULL) {
        ZrCore_Object_SetValue(state, object, &memberKey, value);
        return ZR_TRUE;
    }

    prototypeValue = object_get_prototype_value_unchecked(state, ownerPrototype, &memberKey, ZR_TRUE);
    if (prototypeValue != ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_allows_dynamic_member_write(object)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, &memberKey, value);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_TrySetMemberWithKeyFastUnchecked(struct SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       struct SZrString *memberName,
                                                       const SZrTypeValue *memberKey,
                                                       const SZrTypeValue *value,
                                                       TZrBool *outHandled) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *existingValue;
    const SZrMemberDescriptor *descriptor;
    const SZrManagedFieldInfo *managedField;
    const SZrTypeValue *prototypeValue;
    TZrBool isPrototypeReceiver;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);
    ZR_ASSERT(memberKey != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(outHandled != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);
    ZR_ASSERT(receiver->value.object != ZR_NULL);
    ZR_ASSERT(memberKey->type == ZR_VALUE_TYPE_STRING);

    *outHandled = ZR_FALSE;
    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL || object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_FALSE;
    }

    prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype_unchecked(state, receiver);
    existingValue = object_get_own_value_unchecked(state, object, memberKey);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;
    managedField = prototype != ZR_NULL
                           ? object_prototype_find_managed_field(prototype, memberName, ZR_TRUE)
                           : ZR_NULL;
    prototypeValue = prototype != ZR_NULL ? object_get_prototype_value_unchecked(state, prototype, memberKey, ZR_TRUE) : ZR_NULL;
    isPrototypeReceiver = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ? ZR_TRUE : ZR_FALSE;

    if (descriptor != ZR_NULL) {
        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY && descriptor->setterFunction != ZR_NULL) {
            return ZR_FALSE;
        }

        if (descriptor->isStatic && !isPrototypeReceiver) {
            *outHandled = ZR_TRUE;
            return ZR_FALSE;
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD ||
            descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER) {
            if (!descriptor->isWritable) {
                *outHandled = ZR_TRUE;
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(state,
                                   descriptor->isStatic && prototype != ZR_NULL ? &prototype->super : object,
                                   memberKey,
                                   value);
            *outHandled = ZR_TRUE;
            return ZR_TRUE;
        }
    }

    if (managedField != ZR_NULL || existingValue != ZR_NULL) {
        ZrCore_Object_SetValue(state, object, memberKey, value);
        *outHandled = ZR_TRUE;
        return ZR_TRUE;
    }

    if (prototypeValue != ZR_NULL) {
        *outHandled = ZR_TRUE;
        return ZR_FALSE;
    }

    if (!object_allows_dynamic_member_write(object)) {
        *outHandled = ZR_TRUE;
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, memberKey, value);
    *outHandled = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_Object_InvokeMember(struct SZrState *state,
                                   SZrTypeValue *receiver,
                                   struct SZrString *memberName,
                                   const SZrTypeValue *arguments,
                                   TZrSize argumentCount,
                                   SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrMemberDescriptor *descriptor;
    SZrTypeValue callableValue;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT &&
        receiver->type != ZR_VALUE_TYPE_ARRAY &&
        receiver->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }

    object = (receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY)
                     ? ZR_CAST_OBJECT(state, receiver->value.object)
                     : ZR_NULL;

    prototype = (object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE)
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype(state, receiver);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;

    ZrCore_Value_ResetAsNull(&callableValue);
    if (!ZrCore_Object_GetMember(state, receiver, memberName, &callableValue)) {
        return ZR_FALSE;
    }

    return ZrCore_Object_CallValue(state,
                                   &callableValue,
                                   (descriptor != ZR_NULL && descriptor->isStatic) ? ZR_NULL : receiver,
                                   arguments,
                                   argumentCount,
                                   result);
}

TZrBool ZrCore_Object_ResolveMemberCallable(struct SZrState *state,
                                            SZrTypeValue *receiver,
                                            struct SZrString *memberName,
                                            struct SZrObjectPrototype **outOwnerPrototype,
                                            struct SZrFunction **outFunction,
                                            TZrBool *outIsStatic) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    SZrTypeValue key;
    TZrBool isPrototypeReceiver;

    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ZR_NULL;
    }
    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (outIsStatic != ZR_NULL) {
        *outIsStatic = ZR_FALSE;
    }

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT &&
        receiver->type != ZR_VALUE_TYPE_ARRAY &&
        receiver->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }

    object = (receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY)
                     ? ZR_CAST_OBJECT(state, receiver->value.object)
                     : ZR_NULL;
    if (!object_make_string_key(state, memberName, &key)) {
        return ZR_FALSE;
    }

    isPrototypeReceiver =
            (TZrBool)(object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    prototype = isPrototypeReceiver ? (SZrObjectPrototype *)object : object_value_resolve_prototype(state, receiver);
    while (prototype != ZR_NULL) {
        const SZrMemberDescriptor *descriptor =
                ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_FALSE);
        const SZrTypeValue *resolvedValue = object_get_own_value(state, &prototype->super, &key);

        if (descriptor != ZR_NULL && descriptor->isStatic && !isPrototypeReceiver) {
            return ZR_FALSE;
        }

        if (resolvedValue != ZR_NULL &&
            (resolvedValue->type == ZR_VALUE_TYPE_FUNCTION || resolvedValue->type == ZR_VALUE_TYPE_CLOSURE) &&
            resolvedValue->value.object != ZR_NULL) {
            if (outOwnerPrototype != ZR_NULL) {
                *outOwnerPrototype = prototype;
            }
            if (outFunction != ZR_NULL) {
                *outFunction = ZR_CAST(SZrFunction *, resolvedValue->value.object);
            }
            if (outIsStatic != ZR_NULL) {
                *outIsStatic = (descriptor != ZR_NULL && descriptor->isStatic) ? ZR_TRUE : ZR_FALSE;
            }
            return ZR_TRUE;
        }

        prototype = prototype->superPrototype;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_Object_InvokeResolvedFunction(struct SZrState *state,
                                             struct SZrFunction *function,
                                             TZrBool isStatic,
                                             SZrTypeValue *receiver,
                                             const SZrTypeValue *arguments,
                                             TZrSize argumentCount,
                                             SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_Object_CallFunctionWithReceiver(state,
                                                  function,
                                                  isStatic ? ZR_NULL : receiver,
                                                  arguments,
                                                  argumentCount,
                                                  result);
}

static ZR_FORCE_INLINE TZrBool object_get_by_index_unchecked_core(SZrState *state,
                                                                  SZrTypeValue *receiver,
                                                                  const SZrTypeValue *key,
                                                                  SZrTypeValue *result,
                                                                  TZrBool stackOperandsGuaranteed) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    SZrIndexContract *indexContract;
    const SZrObjectKnownNativeDirectDispatch *cachedDirectDispatch;
    SZrFunction *indexFunction;
    TZrBool canTryKnownNativeIndexFastPath = ZR_FALSE;
    const SZrTypeValue *resolvedValue;
    TZrBool superArrayApplicable = ZR_FALSE;

    object_record_helper(state, ZR_PROFILE_HELPER_GET_BY_INDEX);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (stackOperandsGuaranteed && !ZR_VALUE_IS_TYPE_INT(key->type)) {
        if (object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(
                    state,
                    object,
                    receiver,
                    key,
                    result)) {
            return ZR_TRUE;
        }
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(key->type)) {
        if (!ZrCore_Object_SuperArrayTryGetIntFast(state, receiver, key, result, &superArrayApplicable)) {
            return ZR_FALSE;
        }
        if (superArrayApplicable) {
            return ZR_TRUE;
        }
    }

    prototype = object_resolve_index_receiver_prototype_unchecked(state, receiver, object);
    if (prototype != ZR_NULL && prototype->indexContract.getByIndexFunction != ZR_NULL) {
        SZrRawObject *nativeCallableObject = ZR_NULL;
        FZrNativeFunction nativeFunction = ZR_NULL;

        indexContract = &prototype->indexContract;
        cachedDirectDispatch = &indexContract->getByIndexKnownNativeDirectDispatch;
        indexFunction = indexContract->getByIndexFunction;
        canTryKnownNativeIndexFastPath = (TZrBool)(object->internalType != ZR_OBJECT_INTERNAL_TYPE_STRUCT);

        if (canTryKnownNativeIndexFastPath) {
            if ((stackOperandsGuaranteed
                          ? object_try_call_cached_known_native_get_by_index_readonly_inline_stack_operands(
                                     state, cachedDirectDispatch, receiver, key, result)
                          : object_try_call_cached_known_native_get_by_index_readonly_inline(
                                     state, cachedDirectDispatch, receiver, key, result))) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            if (object_try_call_cached_known_native_get_by_index_dispatch(state,
                                                                          indexFunction,
                                                                          indexContract,
                                                                          cachedDirectDispatch,
                                                                          receiver,
                                                                          key,
                                                                          result)) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
        }

        if (canTryKnownNativeIndexFastPath &&
            object_resolve_known_native_index_contract_callable(state,
                                                                indexFunction,
                                                                1u,
                                                                &indexContract->getByIndexKnownNativeCallable,
                                                                &indexContract->getByIndexKnownNativeFunction,
                                                                &indexContract->getByIndexKnownNativeDirectDispatch,
                                                                 &nativeCallableObject,
                                                                &nativeFunction)) {
            if ((stackOperandsGuaranteed
                         ? object_try_call_cached_known_native_get_by_index_readonly_inline_stack_operands(
                                    state, &indexContract->getByIndexKnownNativeDirectDispatch, receiver, key, result)
                         : object_try_call_cached_known_native_get_by_index_readonly_inline(
                                    state, &indexContract->getByIndexKnownNativeDirectDispatch, receiver, key, result))) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            if (ZrCore_Object_CallDirectBindingFastOneArgument(
                        state,
                        &indexContract->getByIndexKnownNativeDirectDispatch,
                        receiver,
                        key,
                        result)) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            return ZrCore_Object_CallKnownNativeFastOneArgument(state,
                                                                nativeCallableObject,
                                                                nativeFunction,
                                                                &indexContract->getByIndexKnownNativeDirectDispatch,
                                                                receiver,
                                                                key,
                                                                result);
        }

        return ZrCore_Object_CallFunctionWithReceiver(state,
                                                      indexFunction,
                                                      receiver,
                                                      key,
                                                      1,
                                                      result);
    }

    if (!object_can_use_direct_index_fallback_unchecked(object)) {
        return ZR_FALSE;
    }

    resolvedValue = object_get_own_value_unchecked(state, object, key);
    if (resolvedValue == ZR_NULL) {
        if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
            if (object_module_guard_pending_export(state, object, ZR_CAST_STRING(state, key->value.object))) {
                return state->threadStatus != ZR_THREAD_STATUS_FINE ? ZR_TRUE : ZR_FALSE;
            }
        }
        ZrCore_Value_ResetAsNull(result);
    } else {
        if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL &&
            object_module_guard_pending_export(state, object, ZR_CAST_STRING(state, key->value.object))) {
            return state->threadStatus != ZR_THREAD_STATUS_FINE ? ZR_TRUE : ZR_FALSE;
        }
        ZrCore_Value_Copy(state, result, resolvedValue);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Object_GetByIndexUnchecked(struct SZrState *state,
                                          SZrTypeValue *receiver,
                                          const SZrTypeValue *key,
                                          SZrTypeValue *result) {
    return object_get_by_index_unchecked_core(state, receiver, key, result, ZR_FALSE);
}

TZrBool ZrCore_Object_GetByIndexUncheckedStackOperands(struct SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       const SZrTypeValue *key,
                                                       SZrTypeValue *result) {
    return object_get_by_index_unchecked_core(state, receiver, key, result, ZR_TRUE);
}

TZrBool ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands(struct SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   const SZrTypeValue *key,
                                                                   SZrTypeValue *result) {
    return object_try_get_by_index_readonly_inline_fast_stack_operands(state, receiver, key, result);
}

TZrBool ZrCore_Object_SetByIndex(struct SZrState *state,
                                 SZrTypeValue *receiver,
                                 const SZrTypeValue *key,
                                 const SZrTypeValue *value) {
    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_Object_SetByIndexUnchecked(state, receiver, key, value);
}

TZrBool ZrCore_Object_GetByIndex(struct SZrState *state,
                                 SZrTypeValue *receiver,
                                 const SZrTypeValue *key,
                                 SZrTypeValue *result) {
    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_Object_GetByIndexUnchecked(state, receiver, key, result);
}

static ZR_FORCE_INLINE TZrBool object_set_by_index_unchecked_core(SZrState *state,
                                                                  SZrTypeValue *receiver,
                                                                  const SZrTypeValue *key,
                                                                  const SZrTypeValue *value,
                                                                  TZrBool stackOperandsGuaranteed) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    SZrIndexContract *indexContract;
    TZrBool superArrayApplicable = ZR_FALSE;

    object_record_helper(state, ZR_PROFILE_HELPER_SET_BY_INDEX);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (stackOperandsGuaranteed && !ZR_VALUE_IS_TYPE_INT(key->type)) {
        if (object_try_call_hot_cached_known_native_set_by_index_readonly_inline_stack_operands(
                    state,
                    object,
                    receiver,
                    key,
                    value)) {
            return ZR_TRUE;
        }
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(key->type)) {
        if (!ZrCore_Object_SuperArrayTrySetIntFast(state, receiver, key, value, &superArrayApplicable)) {
            return ZR_FALSE;
        }
        if (superArrayApplicable) {
            return ZR_TRUE;
        }
    }

    prototype = object_resolve_index_receiver_prototype_unchecked(state, receiver, object);
    if (prototype != ZR_NULL && prototype->indexContract.setByIndexFunction != ZR_NULL) {
        SZrTypeValue ignoredResult;
        const SZrObjectKnownNativeDirectDispatch *cachedDirectDispatch;
        SZrFunction *indexFunction;
        SZrRawObject *nativeCallableObject = ZR_NULL;
        FZrNativeFunction nativeFunction = ZR_NULL;
        indexContract = &prototype->indexContract;
        cachedDirectDispatch = &indexContract->setByIndexKnownNativeDirectDispatch;
        indexFunction = indexContract->setByIndexFunction;

        if (object->internalType != ZR_OBJECT_INTERNAL_TYPE_STRUCT) {
            if ((stackOperandsGuaranteed
                          ? object_try_call_cached_known_native_set_by_index_readonly_inline_stack_operands(
                                     state, cachedDirectDispatch, receiver, key, value)
                          : object_try_call_cached_known_native_set_by_index_readonly_inline(
                                     state, cachedDirectDispatch, receiver, key, value))) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            if (object_try_call_cached_known_native_set_by_index_dispatch(state,
                                                                          indexFunction,
                                                                          indexContract,
                                                                          cachedDirectDispatch,
                                                                          receiver,
                                                                          key,
                                                                          value)) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
        }

        if (object->internalType != ZR_OBJECT_INTERNAL_TYPE_STRUCT &&
            object_resolve_known_native_index_contract_callable(state,
                                                                indexFunction,
                                                                2u,
                                                                &indexContract->setByIndexKnownNativeCallable,
                                                                &indexContract->setByIndexKnownNativeFunction,
                                                                &indexContract->setByIndexKnownNativeDirectDispatch,
                                                                 &nativeCallableObject,
                                                                 &nativeFunction)) {
            if ((stackOperandsGuaranteed
                         ? object_try_call_cached_known_native_set_by_index_readonly_inline_stack_operands(
                                    state, &indexContract->setByIndexKnownNativeDirectDispatch, receiver, key, value)
                         : object_try_call_cached_known_native_set_by_index_readonly_inline(
                                    state, &indexContract->setByIndexKnownNativeDirectDispatch, receiver, key, value))) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            if (ZrCore_Object_CallDirectBindingFastTwoArgumentsNoResult(state,
                                                                        &indexContract->setByIndexKnownNativeDirectDispatch,
                                                                        receiver,
                                                                        key,
                                                                        value)) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            ZrCore_Value_ResetAsNull(&ignoredResult);
            if (ZrCore_Object_CallDirectBindingFastTwoArguments(
                        state,
                        &indexContract->setByIndexKnownNativeDirectDispatch,
                        receiver,
                        key,
                        value,
                        &ignoredResult)) {
                return ZR_TRUE;
            }
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }
            return ZrCore_Object_CallKnownNativeFastTwoArguments(state,
                                                                 nativeCallableObject,
                                                                 nativeFunction,
                                                                 &indexContract->setByIndexKnownNativeDirectDispatch,
                                                                 receiver,
                                                                 key,
                                                                 value,
                                                                 &ignoredResult);
        }

        return ZrCore_Object_CallFunctionWithReceiverTwoArguments(state,
                                                                 indexContract->setByIndexFunction,
                                                                 receiver,
                                                                 key,
                                                                 value,
                                                                 &ignoredResult);
    }

    if (!object_can_use_direct_index_fallback_unchecked(object)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, key, value);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_SetByIndexUnchecked(struct SZrState *state,
                                          SZrTypeValue *receiver,
                                          const SZrTypeValue *key,
                                          const SZrTypeValue *value) {
    return object_set_by_index_unchecked_core(state, receiver, key, value, ZR_FALSE);
}

TZrBool ZrCore_Object_SetByIndexUncheckedStackOperands(struct SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       const SZrTypeValue *key,
                                                       const SZrTypeValue *value) {
    return object_set_by_index_unchecked_core(state, receiver, key, value, ZR_TRUE);
}

TZrBool ZrCore_Object_TrySetByIndexReadonlyInlineFastStackOperands(struct SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   const SZrTypeValue *key,
                                                                   const SZrTypeValue *value) {
    return object_try_set_by_index_readonly_inline_fast_stack_operands(state, receiver, key, value);
}

TZrBool ZrCore_Object_IterInit(struct SZrState *state,
                               SZrTypeValue *iterableValue,
                               SZrTypeValue *result) {
    SZrObject *iteratorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue indexValue;
    SZrTypeValue currentValue;
    SZrTypeValue currentValidValue;

    if (state == ZR_NULL || iterableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (iterableValue->type != ZR_VALUE_TYPE_ARRAY && iterableValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, iterableValue);
    if (iterableValue->type != ZR_VALUE_TYPE_ARRAY &&
        prototype != ZR_NULL &&
        prototype->iterableContract.iterInitFunction != ZR_NULL) {
        if (ZrCore_Object_CallFunctionWithReceiver(state,
                                                   prototype->iterableContract.iterInitFunction,
                                                   iterableValue,
                                                   ZR_NULL,
                                                   0,
                                                   result)) {
            return ZR_TRUE;
        }
    }

    if (iterableValue->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    iteratorObject = ZrCore_Object_New(state, ZR_NULL);
    if (iteratorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, iteratorObject);

    ZrCore_Value_InitAsInt(state, &indexValue, -1);
    ZrCore_Value_ResetAsNull(&currentValue);
    ZrCore_Value_InitAsBool(state, &currentValidValue, ZR_FALSE);

    if (!object_hidden_set(state, iteratorObject, "__zr_iter_source", iterableValue) ||
        !object_hidden_set(state, iteratorObject, "__zr_iter_index", &indexValue) ||
        !object_hidden_set(state, iteratorObject, "__zr_iter_current", &currentValue) ||
        !object_hidden_set(state, iteratorObject, "__zr_iter_has_current", &currentValidValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

TZrBool ZrCore_Object_IterMoveNext(struct SZrState *state,
                                   SZrTypeValue *iteratorValue,
                                   SZrTypeValue *result) {
    SZrObject *iteratorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue sourceValue;
    SZrTypeValue indexValue;
    SZrTypeValue nextIndexValue;
    SZrTypeValue currentValue;
    SZrTypeValue hasCurrentValue;
    const SZrTypeValue *resolvedValue;
    SZrObject *sourceObject;

    if (state == ZR_NULL || iteratorValue == ZR_NULL || result == ZR_NULL ||
        iteratorValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    iteratorObject = ZR_CAST_OBJECT(state, iteratorValue->value.object);
    if (iteratorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    prototype = object_value_resolve_prototype(state, iteratorValue);
    if (prototype != ZR_NULL && prototype->iteratorContract.moveNextFunction != ZR_NULL) {
        if (ZrCore_Object_CallFunctionWithReceiver(state,
                                                   prototype->iteratorContract.moveNextFunction,
                                                   iteratorValue,
                                                   ZR_NULL,
                                                   0,
                                                   result)) {
            return ZR_TRUE;
        }
    }

    if (!object_hidden_get(state, iteratorObject, "__zr_iter_source", &sourceValue) ||
        !object_hidden_get(state, iteratorObject, "__zr_iter_index", &indexValue)) {
        return ZR_FALSE;
    }

    if (sourceValue.type != ZR_VALUE_TYPE_ARRAY && sourceValue.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &nextIndexValue, indexValue.value.nativeObject.nativeInt64 + 1);
    sourceObject = ZR_CAST_OBJECT(state, sourceValue.value.object);
    resolvedValue = ZrCore_Object_GetValue(state, sourceObject, &nextIndexValue);
    if (resolvedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&currentValue);
        ZrCore_Value_InitAsBool(state, &hasCurrentValue, ZR_FALSE);
        object_hidden_set(state, iteratorObject, "__zr_iter_current", &currentValue);
        object_hidden_set(state, iteratorObject, "__zr_iter_has_current", &hasCurrentValue);
        ZrCore_Value_InitAsBool(state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    // Value copy is ownership-aware, so the destination must be initialized first.
    ZrCore_Value_ResetAsNull(&currentValue);
    ZrCore_Value_Copy(state, &currentValue, resolvedValue);
    ZrCore_Value_InitAsBool(state, &hasCurrentValue, ZR_TRUE);
    object_hidden_set(state, iteratorObject, "__zr_iter_index", &nextIndexValue);
    object_hidden_set(state, iteratorObject, "__zr_iter_current", &currentValue);
    object_hidden_set(state, iteratorObject, "__zr_iter_has_current", &hasCurrentValue);
    ZrCore_Value_InitAsBool(state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_IterCurrent(struct SZrState *state,
                                  SZrTypeValue *iteratorValue,
                                  SZrTypeValue *result) {
    SZrObject *iteratorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue currentValue;

    if (state == ZR_NULL || iteratorValue == ZR_NULL || result == ZR_NULL ||
        iteratorValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    iteratorObject = ZR_CAST_OBJECT(state, iteratorValue->value.object);
    if (iteratorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, iteratorValue);
    if (prototype != ZR_NULL) {
        if (prototype->iteratorContract.currentFunction != ZR_NULL) {
            if (ZrCore_Object_CallFunctionWithReceiver(state,
                                                       prototype->iteratorContract.currentFunction,
                                                       iteratorValue,
                                                       ZR_NULL,
                                                       0,
                                                       result)) {
                return ZR_TRUE;
            }
        }

        if (prototype->iteratorContract.currentMemberName != ZR_NULL) {
            if (ZrCore_Object_GetMember(state,
                                        iteratorValue,
                                        prototype->iteratorContract.currentMemberName,
                                        result)) {
                return ZR_TRUE;
            }
        }
    }

    if (!object_hidden_get(state, iteratorObject, "__zr_iter_current", &currentValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, &currentValue);
    return ZR_TRUE;
}

void ZrCore_ObjectPrototype_SetIndexContract(SZrObjectPrototype *prototype,
                                             const SZrIndexContract *contract) {
    if (prototype == ZR_NULL) {
        return;
    }

    if (contract == ZR_NULL) {
        memset(&prototype->indexContract, 0, sizeof(prototype->indexContract));
        return;
    }

    prototype->indexContract = *contract;
    prototype->indexContract.getByIndexKnownNativeCallable = ZR_NULL;
    prototype->indexContract.setByIndexKnownNativeCallable = ZR_NULL;
    prototype->indexContract.getByIndexKnownNativeFunction = ZR_NULL;
    prototype->indexContract.setByIndexKnownNativeFunction = ZR_NULL;
    memset(&prototype->indexContract.getByIndexKnownNativeDirectDispatch,
           0,
           sizeof(prototype->indexContract.getByIndexKnownNativeDirectDispatch));
    memset(&prototype->indexContract.setByIndexKnownNativeDirectDispatch,
           0,
           sizeof(prototype->indexContract.setByIndexKnownNativeDirectDispatch));
}

void ZrCore_ObjectPrototype_SetIterableContract(SZrObjectPrototype *prototype,
                                                const SZrIterableContract *contract) {
    if (prototype == ZR_NULL) {
        return;
    }

    if (contract == ZR_NULL) {
        memset(&prototype->iterableContract, 0, sizeof(prototype->iterableContract));
        return;
    }

    prototype->iterableContract = *contract;
}

void ZrCore_ObjectPrototype_SetIteratorContract(SZrObjectPrototype *prototype,
                                                const SZrIteratorContract *contract) {
    if (prototype == ZR_NULL) {
        return;
    }

    if (contract == ZR_NULL) {
        memset(&prototype->iteratorContract, 0, sizeof(prototype->iteratorContract));
        return;
    }

    prototype->iteratorContract = *contract;
}

void ZrCore_ObjectPrototype_AddProtocol(SZrObjectPrototype *prototype, EZrProtocolId protocolId) {
    if (prototype == ZR_NULL || protocolId <= ZR_PROTOCOL_ID_NONE) {
        return;
    }

    prototype->protocolMask |= ZR_PROTOCOL_BIT(protocolId);
}

TZrBool ZrCore_ObjectPrototype_ImplementsProtocol(SZrObjectPrototype *prototype, EZrProtocolId protocolId) {
    if (prototype == ZR_NULL || protocolId <= ZR_PROTOCOL_ID_NONE) {
        return ZR_FALSE;
    }

    return (prototype->protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0;
}

TZrBool ZrCore_Object_IsInstanceOfPrototype(SZrObject *object, SZrObjectPrototype *prototype) {
    SZrObjectPrototype *currentPrototype;

    if (object == ZR_NULL || prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    currentPrototype = object->prototype;
    while (currentPrototype != ZR_NULL) {
        if (currentPrototype == prototype) {
            return ZR_TRUE;
        }
        currentPrototype = currentPrototype->superPrototype;
    }

    return ZR_FALSE;
}
