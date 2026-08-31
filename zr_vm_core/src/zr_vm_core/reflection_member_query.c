#include "zr_vm_core/reflection.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ast_constants.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *kQueryMembersField = "members";
static const TZrChar *kQueryOrderField = "__zr_reflection_order";
static const TZrChar *kQueryPrototypeField = "__zr_reflection_prototype";
static const TZrChar *kQueryCacheFieldPrefix = "__zr_reflection_query_";

static SZrReflectionMemberCacheStats gMemberCacheStats;

typedef enum EZrReflectionMemberFieldKey {
    ZR_REFLECTION_MEMBER_FIELD_KIND = 0,
    ZR_REFLECTION_MEMBER_FIELD_ACCESS,
    ZR_REFLECTION_MEMBER_FIELD_STATIC,
    ZR_REFLECTION_MEMBER_FIELD_META_METHOD,
    ZR_REFLECTION_MEMBER_FIELD_NAME,
    ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT,
} EZrReflectionMemberFieldKey;

typedef struct SZrReflectionMemberFieldKeys {
    SZrTypeValue values[ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT];
    TZrBool pinned[ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT];
} SZrReflectionMemberFieldKeys;

void ZrCore_Reflection_DebugResetMemberCacheStats(void) {
    memset(&gMemberCacheStats, 0, sizeof(gMemberCacheStats));
}

SZrReflectionMemberCacheStats
ZrCore_Reflection_DebugGetMemberCacheStats(void) {
    return gMemberCacheStats;
}

static void query_set_status(EZrReflectionQueryStatus *outStatus,
                             EZrReflectionQueryStatus status) {
    if (outStatus != ZR_NULL) {
        *outStatus = status;
    }
}

static const SZrTypeValue *query_get_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *name) {
    SZrString *keyString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    keyString = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrBool query_set_object_field(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *name,
                                      SZrObject *value) {
    SZrString *keyString;
    SZrTypeValue key;
    SZrTypeValue objectValue;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(
            state, &objectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    objectValue.type = ZR_VALUE_TYPE_ARRAY;
    ZrCore_Object_SetValue(state, object, &key, &objectValue);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static TZrBool query_cache_key(
        EZrReflectionMemberKind kind,
        const SZrReflectionMemberQuery *query,
        TZrChar *buffer,
        TZrSize bufferSize) {
    int length;

    if (query == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }
    length = snprintf(
            buffer,
            bufferSize,
            "%s%u_%u_%u_%u_%u_%u_%u",
            kQueryCacheFieldPrefix,
            (unsigned int)kind,
            (unsigned int)query->scope,
            (unsigned int)query->access,
            (unsigned int)query->storage,
            (unsigned int)query->includeCompilerGenerated,
            (unsigned int)query->includeMetaMethods,
            (unsigned int)query->hasNonPublicAccessCapability);
    return (TZrBool)(length > 0 && (TZrSize)length < bufferSize);
}

static SZrObject *query_get_object_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *name,
                                         EZrValueType expectedType) {
    const SZrTypeValue *value = query_get_field(state, object, name);

    if (value == ZR_NULL || value->type != expectedType ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

static const TZrChar *query_get_string_field(SZrState *state,
                                             SZrObject *object,
                                             const TZrChar *name,
                                             const TZrChar *fallback) {
    const SZrTypeValue *value = query_get_field(state, object, name);

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return fallback;
    }
    return ZrCore_String_GetNativeString(
            ZR_CAST_STRING(state, value->value.object));
}

static TZrInt64 query_get_int_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *name,
                                    TZrInt64 fallback) {
    const SZrTypeValue *value = query_get_field(state, object, name);

    return value != ZR_NULL && ZR_VALUE_IS_TYPE_INT(value->type)
                   ? value->value.nativeObject.nativeInt64
                   : fallback;
}

static TZrBool query_get_bool_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *name,
                                    TZrBool fallback) {
    const SZrTypeValue *value = query_get_field(state, object, name);

    return value != ZR_NULL && value->type == ZR_VALUE_TYPE_BOOL
                   ? (value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE)
                   : fallback;
}

static void query_release_member_field_keys(
        SZrState *state,
        SZrReflectionMemberFieldKeys *keys) {
    if (state == ZR_NULL || keys == ZR_NULL) {
        return;
    }
    for (TZrUInt32 index = 0u;
         index < ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT;
         index++) {
        if (keys->pinned[index] && keys->values[index].value.object != ZR_NULL) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global, keys->values[index].value.object);
            keys->pinned[index] = ZR_FALSE;
        }
    }
}

static TZrBool query_prepare_member_field_keys(
        SZrState *state,
        SZrReflectionMemberFieldKeys *keys) {
    static const TZrChar *names[ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT] = {
        "kind", "accessModifier", "isStatic", "isMetaMethod", "name",
    };

    if (state == ZR_NULL || keys == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(keys, 0, sizeof(*keys));
    for (TZrUInt32 index = 0u;
         index < ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT;
         index++) {
        SZrString *keyString = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)names[index]);

        if (keyString == ZR_NULL) {
            query_release_member_field_keys(state, keys);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsRawObject(
                state,
                &keys->values[index],
                ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
        keys->values[index].type = ZR_VALUE_TYPE_STRING;
        if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                    state->global,
                    state,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(keyString),
                    &keys->pinned[index])) {
            query_release_member_field_keys(state, keys);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static const SZrTypeValue *query_get_prepared_field(
        SZrState *state,
        SZrObject *object,
        const SZrReflectionMemberFieldKeys *keys,
        EZrReflectionMemberFieldKey key) {
    if (state == ZR_NULL || object == ZR_NULL || keys == ZR_NULL ||
        key >= ZR_REFLECTION_MEMBER_FIELD_KEY_COUNT) {
        return ZR_NULL;
    }
    return ZrCore_Object_GetValue(state, object, &keys->values[key]);
}

static const TZrChar *query_get_prepared_string_field(
        SZrState *state,
        SZrObject *object,
        const SZrReflectionMemberFieldKeys *keys,
        EZrReflectionMemberFieldKey key,
        const TZrChar *fallback) {
    const SZrTypeValue *value = query_get_prepared_field(
            state, object, keys, key);

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return fallback;
    }
    return ZrCore_String_GetNativeString(
            ZR_CAST_STRING(state, value->value.object));
}

static TZrInt64 query_get_prepared_int_field(
        SZrState *state,
        SZrObject *object,
        const SZrReflectionMemberFieldKeys *keys,
        EZrReflectionMemberFieldKey key,
        TZrInt64 fallback) {
    const SZrTypeValue *value = query_get_prepared_field(
            state, object, keys, key);

    return value != ZR_NULL && ZR_VALUE_IS_TYPE_INT(value->type)
                   ? value->value.nativeObject.nativeInt64
                   : fallback;
}

static TZrBool query_get_prepared_bool_field(
        SZrState *state,
        SZrObject *object,
        const SZrReflectionMemberFieldKeys *keys,
        EZrReflectionMemberFieldKey key,
        TZrBool fallback) {
    const SZrTypeValue *value = query_get_prepared_field(
            state, object, keys, key);

    return value != ZR_NULL && value->type == ZR_VALUE_TYPE_BOOL
                   ? (value->value.nativeObject.nativeBool
                              ? ZR_TRUE
                              : ZR_FALSE)
                   : fallback;
}

static const SZrTypeValue *query_array_get(SZrState *state,
                                           SZrObject *array,
                                           TZrUInt32 index) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

static TZrBool query_array_push_object(SZrState *state,
                                       SZrObject *array,
                                       SZrObject *object) {
    SZrTypeValue key;
    SZrTypeValue value;

    if (state == ZR_NULL || array == ZR_NULL || object == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }
    if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(
            state, &key, (TZrInt64)array->nodeMap.elementCount);
    ZrCore_Value_InitAsRawObject(
            state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, array, &key, &value);
    return ZR_TRUE;
}

static SZrObject *query_new_pinned_array(SZrState *state,
                                         TZrBool *outPinned) {
    SZrObject *array;

    if (outPinned != ZR_NULL) {
        *outPinned = ZR_FALSE;
    }
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Object_Init(state, array);
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                state->global,
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(array),
                outPinned)) {
        return ZR_NULL;
    }
    return array;
}

static TZrBool query_kind_matches(SZrState *state,
                                  SZrObject *member,
                                  EZrReflectionMemberKind kind,
                                  const SZrReflectionMemberFieldKeys *keys) {
    const TZrChar *actualKind;

    if (kind == ZR_REFLECTION_MEMBER_KIND_ANY) {
        return ZR_TRUE;
    }
    actualKind = query_get_prepared_string_field(
            state, member, keys, ZR_REFLECTION_MEMBER_FIELD_KIND, "");

    switch (kind) {
        case ZR_REFLECTION_MEMBER_KIND_FIELD:
            return strcmp(actualKind, "field") == 0;
        case ZR_REFLECTION_MEMBER_KIND_PROPERTY:
            return strcmp(actualKind, "property") == 0;
        case ZR_REFLECTION_MEMBER_KIND_METHOD:
            return strcmp(actualKind, "method") == 0 &&
                   !query_get_prepared_bool_field(
                           state,
                           member,
                           keys,
                           ZR_REFLECTION_MEMBER_FIELD_META_METHOD,
                           ZR_FALSE);
        case ZR_REFLECTION_MEMBER_KIND_META_METHOD:
            return strcmp(actualKind, "method") == 0 &&
                   query_get_prepared_bool_field(
                           state,
                           member,
                           keys,
                           ZR_REFLECTION_MEMBER_FIELD_META_METHOD,
                           ZR_FALSE);
        default:
            return ZR_FALSE;
    }
}

static TZrBool query_access_matches(SZrState *state,
                                    SZrObject *member,
                                    EZrReflectionMemberAccess access,
                                    const SZrReflectionMemberFieldKeys *keys) {
    TZrInt64 actualAccess;

    if (access == ZR_REFLECTION_MEMBER_ACCESS_ALL) {
        return ZR_TRUE;
    }
    actualAccess = query_get_prepared_int_field(
            state,
            member,
            keys,
            ZR_REFLECTION_MEMBER_FIELD_ACCESS,
            ZR_ACCESS_CONSTANT_PUBLIC);

    switch (access) {
        case ZR_REFLECTION_MEMBER_ACCESS_PUBLIC:
            return actualAccess == ZR_ACCESS_CONSTANT_PUBLIC;
        case ZR_REFLECTION_MEMBER_ACCESS_PROTECTED:
            return actualAccess == ZR_ACCESS_CONSTANT_PROTECTED;
        case ZR_REFLECTION_MEMBER_ACCESS_PRIVATE:
            return actualAccess == ZR_ACCESS_CONSTANT_PRIVATE;
        case ZR_REFLECTION_MEMBER_ACCESS_ALL:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool query_storage_matches(SZrState *state,
                                     SZrObject *member,
                                     EZrReflectionMemberStorage storage,
                                     const SZrReflectionMemberFieldKeys *keys) {
    TZrBool isStatic;

    if (storage == ZR_REFLECTION_MEMBER_STORAGE_ALL) {
        return ZR_TRUE;
    }
    isStatic = query_get_prepared_bool_field(
            state,
            member,
            keys,
            ZR_REFLECTION_MEMBER_FIELD_STATIC,
            ZR_FALSE);

    return (storage == ZR_REFLECTION_MEMBER_STORAGE_STATIC && isStatic) ||
           (storage == ZR_REFLECTION_MEMBER_STORAGE_INSTANCE && !isStatic);
}

static TZrBool query_member_matches(SZrState *state,
                                    SZrObject *member,
                                    EZrReflectionMemberKind kind,
                                    const SZrReflectionMemberQuery *query,
                                    const SZrReflectionMemberFieldKeys *keys) {
    const TZrChar *name;

    if (!query_kind_matches(state, member, kind, keys) ||
        !query_access_matches(state, member, query->access, keys) ||
        !query_storage_matches(state, member, query->storage, keys)) {
        return ZR_FALSE;
    }
    if (!query->includeMetaMethods &&
        kind != ZR_REFLECTION_MEMBER_KIND_META_METHOD &&
        query_get_prepared_bool_field(
                state,
                member,
                keys,
                ZR_REFLECTION_MEMBER_FIELD_META_METHOD,
                ZR_FALSE)) {
            return ZR_FALSE;
    }
    if (query->includeCompilerGenerated) {
        return ZR_TRUE;
    }
    name = query_get_prepared_string_field(
            state,
            member,
            keys,
            ZR_REFLECTION_MEMBER_FIELD_NAME,
            "");
    return strncmp(name, "__", 2u) != 0;
}

static TZrBool query_append_declared_members(
        SZrState *state,
        SZrObject *descriptor,
        EZrReflectionMemberKind kind,
        const SZrReflectionMemberQuery *query,
        const SZrReflectionMemberFieldKeys *keys,
        SZrObject *result) {
    SZrObject *members = query_get_object_field(
            state, descriptor, kQueryMembersField, ZR_VALUE_TYPE_OBJECT);
    SZrObject *order;

    if (members == ZR_NULL) {
        return ZR_FALSE;
    }
    order = query_get_object_field(
            state, members, kQueryOrderField, ZR_VALUE_TYPE_ARRAY);
    if (order == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, order)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 nameIndex = 0u;
         nameIndex < (TZrUInt32)order->nodeMap.elementCount;
         nameIndex++) {
        const SZrTypeValue *nameValue = query_array_get(state, order, nameIndex);
        const TZrChar *name;
        SZrObject *bucket;

        if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
            nameValue->value.object == ZR_NULL) {
            continue;
        }
        name = ZrCore_String_GetNativeString(
                ZR_CAST_STRING(state, nameValue->value.object));
        bucket = query_get_object_field(
                state, members, name, ZR_VALUE_TYPE_ARRAY);
        if (bucket == ZR_NULL) {
            continue;
        }
        if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, bucket)) {
            return ZR_FALSE;
        }
        {
            TZrSize requiredElementCount =
                    result->nodeMap.elementCount + bucket->nodeMap.elementCount;
            TZrSize requiredCapacity =
                    ZrCore_HashSet_MinDenseSequentialIntKeyCapacity(
                            requiredElementCount);

            if (requiredCapacity > 0u &&
                !ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacityAndPairPoolExact(
                        state, &result->nodeMap, requiredCapacity)) {
                return ZR_FALSE;
            }
        }

        for (TZrUInt32 memberIndex = 0u;
             memberIndex < (TZrUInt32)bucket->nodeMap.elementCount;
             memberIndex++) {
            const SZrTypeValue *memberValue = query_array_get(
                    state, bucket, memberIndex);
            SZrObject *member;

            if (memberValue == ZR_NULL ||
                memberValue->type != ZR_VALUE_TYPE_OBJECT ||
                memberValue->value.object == ZR_NULL) {
                continue;
            }
            member = ZR_CAST_OBJECT(state, memberValue->value.object);
            if (query_member_matches(state, member, kind, query, keys) &&
                !query_array_push_object(state, result, member)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static SZrObjectPrototype *query_descriptor_prototype(
        SZrState *state,
        SZrObject *descriptor) {
    SZrObject *prototypeObject = query_get_object_field(
            state, descriptor, kQueryPrototypeField, ZR_VALUE_TYPE_OBJECT);

    return prototypeObject != ZR_NULL &&
                   prototypeObject->internalType ==
                           ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                   ? (SZrObjectPrototype *)prototypeObject
                   : ZR_NULL;
}

static TZrBool query_append_inherited_members(
        SZrState *state,
        SZrObject *descriptor,
        EZrReflectionMemberKind kind,
        const SZrReflectionMemberQuery *query,
        const SZrReflectionMemberFieldKeys *keys,
        SZrObject *result) {
    SZrObjectPrototype *prototype = query_descriptor_prototype(
            state, descriptor);

    for (prototype = prototype != ZR_NULL ? prototype->superPrototype : ZR_NULL;
         prototype != ZR_NULL;
         prototype = prototype->superPrototype) {
        SZrTypeValue prototypeValue;
        SZrTypeValue descriptorValue;

        ZrCore_Value_InitAsRawObject(
                state,
                &prototypeValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
        prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_ResetAsNull(&descriptorValue);
        if (!ZrCore_Reflection_TypeOfValue(
                    state, &prototypeValue, &descriptorValue) ||
            descriptorValue.type != ZR_VALUE_TYPE_OBJECT ||
            descriptorValue.value.object == ZR_NULL ||
            !query_append_declared_members(
                    state,
                    ZR_CAST_OBJECT(state, descriptorValue.value.object),
                    kind,
                    query,
                    keys,
                    result)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void ZrCore_Reflection_MemberQueryInitDefault(
        SZrReflectionMemberQuery *query) {
    if (query == ZR_NULL) {
        return;
    }
    query->scope = ZR_REFLECTION_MEMBER_SCOPE_ALL;
    query->access = ZR_REFLECTION_MEMBER_ACCESS_PUBLIC;
    query->storage = ZR_REFLECTION_MEMBER_STORAGE_ALL;
    query->includeCompilerGenerated = ZR_FALSE;
    query->includeMetaMethods = ZR_FALSE;
    query->hasNonPublicAccessCapability = ZR_FALSE;
}

TZrBool ZrCore_Reflection_QueryMembers(
        SZrState *state,
        SZrObject *typeDescriptor,
        EZrReflectionMemberKind kind,
        const SZrReflectionMemberQuery *query,
        SZrObject **outMembers,
        EZrReflectionQueryStatus *outStatus) {
    SZrReflectionMemberQuery defaultQuery;
    const SZrReflectionMemberQuery *effectiveQuery = query;
    SZrObject *result;
    SZrObject *cached;
    TZrChar cacheKey[128];
    TZrBool resultPinned = ZR_FALSE;
    TZrBool success = ZR_TRUE;
    SZrReflectionMemberFieldKeys memberFieldKeys;

    if (outMembers != ZR_NULL) {
        *outMembers = ZR_NULL;
    }
    query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_INVALID_ARGUMENT);
    if (state == ZR_NULL || typeDescriptor == ZR_NULL ||
        outMembers == ZR_NULL ||
        kind > ZR_REFLECTION_MEMBER_KIND_ANY ||
        !ZrCore_Reflection_IsReflectionObject(state, typeDescriptor)) {
        return ZR_FALSE;
    }
    if (effectiveQuery == ZR_NULL) {
        ZrCore_Reflection_MemberQueryInitDefault(&defaultQuery);
        effectiveQuery = &defaultQuery;
    }
    if ((effectiveQuery->access == ZR_REFLECTION_MEMBER_ACCESS_PRIVATE ||
         effectiveQuery->access == ZR_REFLECTION_MEMBER_ACCESS_PROTECTED ||
         effectiveQuery->access == ZR_REFLECTION_MEMBER_ACCESS_ALL) &&
        !effectiveQuery->hasNonPublicAccessCapability) {
        query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_ACCESS_DENIED);
        return ZR_FALSE;
    }
    if (query_cache_key(
                kind, effectiveQuery, cacheKey, sizeof(cacheKey))) {
        cached = query_get_object_field(
                state, typeDescriptor, cacheKey, ZR_VALUE_TYPE_ARRAY);
        if (cached != ZR_NULL) {
            gMemberCacheStats.hitCount++;
            *outMembers = cached;
            query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_OK);
            return ZR_TRUE;
        }
    } else {
        cacheKey[0] = '\0';
    }
    gMemberCacheStats.missCount++;

    result = query_new_pinned_array(state, &resultPinned);
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!query_prepare_member_field_keys(state, &memberFieldKeys)) {
        if (resultPinned) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
        }
        return ZR_FALSE;
    }
    if (effectiveQuery->scope == ZR_REFLECTION_MEMBER_SCOPE_DECLARED ||
        effectiveQuery->scope == ZR_REFLECTION_MEMBER_SCOPE_ALL) {
        success = query_append_declared_members(
                state,
                typeDescriptor,
                kind,
                effectiveQuery,
                &memberFieldKeys,
                result);
    }
    if (success &&
        (effectiveQuery->scope == ZR_REFLECTION_MEMBER_SCOPE_INHERITED ||
         effectiveQuery->scope == ZR_REFLECTION_MEMBER_SCOPE_ALL)) {
        success = query_append_inherited_members(
                state,
                typeDescriptor,
                kind,
                effectiveQuery,
                &memberFieldKeys,
                result);
    }
    query_release_member_field_keys(state, &memberFieldKeys);
    if (success && cacheKey[0] != '\0' &&
        !query_set_object_field(
                state, typeDescriptor, cacheKey, result)) {
        success = ZR_FALSE;
    }
    if (resultPinned) {
        ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    }
    if (!success) {
        query_set_status(
                outStatus, ZR_REFLECTION_QUERY_STATUS_METADATA_NOT_PRESERVED);
        return ZR_FALSE;
    }
    *outMembers = result;
    query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_OK);
    return ZR_TRUE;
}

static TZrBool query_parameter_types_match(
        SZrState *state,
        SZrObject *member,
        const SZrObject *const *parameterTypeIds,
        TZrUInt32 parameterTypeCount) {
    SZrObject *parameters;

    if ((TZrUInt32)query_get_int_field(
                state, member, "parameterCount", 0) != parameterTypeCount) {
        return ZR_FALSE;
    }
    if (parameterTypeCount == 0u) {
        return ZR_TRUE;
    }
    parameters = query_get_object_field(
            state, member, "parameters", ZR_VALUE_TYPE_ARRAY);
    if (parameters == ZR_NULL ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, parameters) ||
        parameters->nodeMap.elementCount != parameterTypeCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < parameterTypeCount; index++) {
        const SZrTypeValue *parameterValue = query_array_get(
                state, parameters, index);
        SZrObject *parameter;
        SZrReflectionTypeIdentity identity;
        SZrString *identityName = ZR_NULL;
        const TZrChar *parameterTypeName;

        if (parameterTypeIds == ZR_NULL || parameterTypeIds[index] == ZR_NULL ||
            parameterValue == ZR_NULL ||
            parameterValue->type != ZR_VALUE_TYPE_OBJECT ||
            parameterValue->value.object == ZR_NULL ||
            !ZrCore_Reflection_ReadTypeIdObject(
                    state,
                    (SZrObject *)parameterTypeIds[index],
                    &identity,
                    &identityName)) {
            return ZR_FALSE;
        }
        parameter = ZR_CAST_OBJECT(state, parameterValue->value.object);
        parameterTypeName = query_get_string_field(
                state, parameter, "typeName", ZR_NULL);
        if (parameterTypeName == ZR_NULL || identityName == ZR_NULL ||
            strcmp(parameterTypeName,
                   ZrCore_String_GetNativeString(identityName)) != 0) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_GetMember(
        SZrState *state,
        SZrObject *typeDescriptor,
        const SZrString *name,
        EZrReflectionMemberKind kind,
        const SZrObject *const *parameterTypeIds,
        TZrUInt32 parameterTypeCount,
        const SZrReflectionMemberQuery *query,
        SZrObject **outMember,
        EZrReflectionQueryStatus *outStatus) {
    SZrObject *members = ZR_NULL;
    const TZrChar *nameText;
    SZrObject *match = ZR_NULL;
    TZrUInt32 matchCount = 0u;
    TZrBool membersPinned = ZR_FALSE;

    if (outMember != ZR_NULL) {
        *outMember = ZR_NULL;
    }
    if (state == ZR_NULL || name == ZR_NULL || outMember == ZR_NULL ||
        !ZrCore_Reflection_QueryMembers(
                state,
                typeDescriptor,
                kind,
                query,
                &members,
                outStatus)) {
        return ZR_FALSE;
    }
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                state->global,
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(members),
                &membersPinned)) {
        query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, members)) {
        if (membersPinned) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(members));
        }
        query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    nameText = ZrCore_String_GetNativeString((SZrString *)name);
    for (TZrUInt32 index = 0u;
         index < (TZrUInt32)members->nodeMap.elementCount;
         index++) {
        const SZrTypeValue *memberValue = query_array_get(
                state, members, index);
        SZrObject *member;

        if (memberValue == ZR_NULL ||
            memberValue->type != ZR_VALUE_TYPE_OBJECT ||
            memberValue->value.object == ZR_NULL) {
            continue;
        }
        member = ZR_CAST_OBJECT(state, memberValue->value.object);
        if (strcmp(query_get_string_field(state, member, "name", ""),
                   nameText) != 0) {
            continue;
        }
        if ((kind == ZR_REFLECTION_MEMBER_KIND_METHOD ||
             kind == ZR_REFLECTION_MEMBER_KIND_META_METHOD) &&
            !query_parameter_types_match(
                    state, member, parameterTypeIds, parameterTypeCount)) {
            continue;
        }
        match = member;
        matchCount++;
    }

    if (matchCount == 0u) {
        if (membersPinned) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(members));
        }
        query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_NOT_FOUND);
        return ZR_FALSE;
    }
    if (matchCount > 1u) {
        if (membersPinned) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(members));
        }
        query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_AMBIGUOUS);
        return ZR_FALSE;
    }
    if (membersPinned) {
        ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(members));
    }
    *outMember = match;
    query_set_status(outStatus, ZR_REFLECTION_QUERY_STATUS_OK);
    return ZR_TRUE;
}
