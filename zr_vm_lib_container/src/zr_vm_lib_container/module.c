//
// Built-in zr.container module and runtime callbacks.
//

#include "zr_vm_lib_container/module.h"

#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const TZrChar *kContainerItemsField = "__zr_items";
static const TZrChar *kContainerEntriesField = "__zr_entries";
static const TZrChar *kContainerSourceField = "__zr_source";
static const TZrChar *kContainerIndexField = "__zr_index";
static const TZrChar *kContainerNextNodeField = "__zr_nextNode";

static TZrBool zr_container_type_name_matches_base(const TZrChar *actualName, const TZrChar *baseName) {
    TZrSize baseLength;

    if (actualName == ZR_NULL || baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(actualName, baseName) == 0) {
        return ZR_TRUE;
    }

    baseLength = strlen(baseName);
    return strncmp(actualName, baseName, baseLength) == 0 && actualName[baseLength] == '<';
}

static SZrObject *zr_container_self_object(ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static TZrBool zr_container_object_type_equals(SZrObject *object, const TZrChar *typeName) {
    const TZrChar *nativeName;

    if (object == ZR_NULL || object->prototype == ZR_NULL || object->prototype->name == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeName = ZrCore_String_GetNativeString(object->prototype->name);
    return zr_container_type_name_matches_base(nativeName, typeName);
}

static SZrObject *zr_container_resolve_construct_target(ZrLibCallContext *context, const TZrChar *typeName) {
    SZrObject *self;

    if (context == ZR_NULL || context->state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    self = zr_container_self_object(context);
    if (self != ZR_NULL && zr_container_object_type_equals(self, typeName)) {
        return self;
    }

    return ZrLib_Type_NewInstance(context->state, typeName);
}

static EZrValueType zr_container_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY : ZR_VALUE_TYPE_OBJECT;
}

static TZrBool zr_container_finish_object(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object) {
    if (context == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, zr_container_value_type_for_object(object));
    return ZR_TRUE;
}

static void zr_container_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void zr_container_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetNull(&fieldValue);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void zr_container_set_value_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

static void zr_container_set_object_field(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          SZrObject *valueObject) {
    SZrTypeValue fieldValue;
    if (valueObject == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetObject(state, &fieldValue, valueObject, zr_container_value_type_for_object(valueObject));
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static const SZrTypeValue *zr_container_get_field_value(SZrState *state,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

static SZrObject *zr_container_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = zr_container_get_field_value(state, object, fieldName);
    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

static TZrInt64 zr_container_get_int_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrInt64 defaultValue) {
    const SZrTypeValue *value = zr_container_get_field_value(state, object, fieldName);

    if (value == ZR_NULL) {
        return defaultValue;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

static TZrBool zr_container_call_method(SZrState *state,
                                        SZrObject *receiver,
                                        const TZrChar *methodName,
                                        const SZrTypeValue *arguments,
                                        TZrSize argumentCount,
                                        SZrTypeValue *result) {
    const SZrTypeValue *callable;
    SZrTypeValue receiverValue;

    if (state == ZR_NULL || receiver == ZR_NULL || methodName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = ZrLib_Object_GetFieldCString(state, receiver, methodName);
    if (callable == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &receiverValue, receiver, zr_container_value_type_for_object(receiver));
    return ZrLib_CallValue(state, callable, &receiverValue, arguments, argumentCount, result);
}

static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value);

static TZrBool zr_container_values_equal(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue lhsCopy;
    SZrTypeValue rhsCopy;
    SZrTypeValue result;

    if (state == ZR_NULL || lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    lhsCopy = *lhs;
    rhsCopy = *rhs;
    if (ZrCore_Value_Equal(state, &lhsCopy, &rhsCopy) || ZrCore_Value_CompareDirectly(state, &lhsCopy, &rhsCopy)) {
        return ZR_TRUE;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "equals", rhs, 1, &result) && result.type == ZR_VALUE_TYPE_BOOL) {
            return (TZrBool)(result.value.nativeObject.nativeBool != 0);
        }
    }

    return ZR_FALSE;
}

static TZrInt64 zr_container_values_compare(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue result;
    const TZrChar *lhsText;
    const TZrChar *rhsText;
    TZrDouble lhsNumber;
    TZrDouble rhsNumber;
    TZrUInt64 lhsHash;
    TZrUInt64 rhsHash;

    if (lhs == ZR_NULL && rhs == ZR_NULL) {
        return 0;
    }
    if (lhs == ZR_NULL) {
        return -1;
    }
    if (rhs == ZR_NULL) {
        return 1;
    }
    if (zr_container_values_equal(state, lhs, rhs)) {
        return 0;
    }

    if (lhs->type == ZR_VALUE_TYPE_STRING && rhs->type == ZR_VALUE_TYPE_STRING) {
        lhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, lhs->value.object));
        rhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, rhs->value.object));
        if (lhsText == ZR_NULL) {
            return rhsText == ZR_NULL ? 0 : -1;
        }
        if (rhsText == ZR_NULL) {
            return 1;
        }
        return strcmp(lhsText, rhsText);
    }

    if ((ZR_VALUE_IS_TYPE_INT(lhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(lhs->type) || ZR_VALUE_IS_TYPE_FLOAT(lhs->type)) &&
        (ZR_VALUE_IS_TYPE_INT(rhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rhs->type) || ZR_VALUE_IS_TYPE_FLOAT(rhs->type))) {
        lhsNumber = ZR_VALUE_IS_TYPE_FLOAT(lhs->type) ? lhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)lhs->value.nativeObject.nativeInt64;
        rhsNumber = ZR_VALUE_IS_TYPE_FLOAT(rhs->type) ? rhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)rhs->value.nativeObject.nativeInt64;
        return lhsNumber < rhsNumber ? -1 : 1;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "compareTo", rhs, 1, &result) &&
            (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type))) {
            return ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)
                           ? result.value.nativeObject.nativeInt64
                           : (TZrInt64)result.value.nativeObject.nativeUInt64;
        }
    }

    lhsHash = zr_container_value_hash(state, lhs);
    rhsHash = zr_container_value_hash(state, rhs);
    if (lhsHash == rhsHash) {
        return (TZrInt64)lhs->type - (TZrInt64)rhs->type;
    }
    return lhsHash < rhsHash ? -1 : 1;
}

static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value) {
    SZrTypeValue result;

    if (state == ZR_NULL || value == ZR_NULL) {
        return 0;
    }

    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, value->value.object);
        if (zr_container_call_method(state, receiver, "hashCode", ZR_NULL, 0, &result)) {
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
                return (TZrUInt64)result.value.nativeObject.nativeInt64;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
                return result.value.nativeObject.nativeUInt64;
            }
        }
    }

    return ZrCore_Value_GetHash(state, value);
}

static TZrBool zr_container_storage_set(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

static TZrBool zr_container_storage_remove_last(SZrState *state, SZrObject *array) {
    SZrTypeValue key;
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = ZrLib_Array_Length(array);
    if (length == 0) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)(length - 1));
    ZrCore_HashSet_Remove(state, &array->nodeMap, &key);
    return ZR_TRUE;
}

static TZrBool zr_container_storage_insert(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = ZrLib_Array_Length(array);
    if (index > length) {
        return ZR_FALSE;
    }

    for (TZrSize cursor = length; cursor > index; cursor--) {
        const SZrTypeValue *source = ZrLib_Array_Get(state, array, cursor - 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_set(state, array, index, value);
}

static TZrBool zr_container_storage_remove_at(SZrState *state, SZrObject *array, TZrSize index) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = ZrLib_Array_Length(array);
    if (index >= length) {
        return ZR_FALSE;
    }

    for (TZrSize cursor = index; cursor + 1 < length; cursor++) {
        const SZrTypeValue *source = ZrLib_Array_Get(state, array, cursor + 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_remove_last(state, array);
}

static SZrObject *zr_container_ensure_hidden_array(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrObject *array = zr_container_get_object_field(state, object, fieldName);
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    array = ZrLib_Array_New(state);
    if (array != ZR_NULL) {
        zr_container_set_object_field(state, object, fieldName, array);
    }
    return array;
}

static SZrObject *zr_container_make_pair(SZrState *state, const SZrTypeValue *first, const SZrTypeValue *second) {
    SZrObject *pair = ZrLib_Type_NewInstance(state, "Pair");
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    if (first != ZR_NULL) {
        zr_container_set_value_field(state, pair, "first", first);
    } else {
        zr_container_set_null_field(state, pair, "first");
    }
    if (second != ZR_NULL) {
        zr_container_set_value_field(state, pair, "second", second);
    } else {
        zr_container_set_null_field(state, pair, "second");
    }
    return pair;
}

static SZrObject *zr_container_make_linked_node(SZrState *state, const SZrTypeValue *value) {
    SZrObject *node = ZrLib_Type_NewInstance(state, "LinkedNode");
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (value != ZR_NULL) {
        zr_container_set_value_field(state, node, "value", value);
    } else {
        zr_container_set_null_field(state, node, "value");
    }
    zr_container_set_null_field(state, node, "next");
    zr_container_set_null_field(state, node, "previous");
    return node;
}

static TZrBool zr_container_map_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *key,
                                           TZrSize *outIndex) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = ZrLib_Array_Length(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(state, entries, index);
        SZrObject *entryObject;
        const SZrTypeValue *entryKey;

        if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
            continue;
        }

        entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
        entryKey = zr_container_get_field_value(state, entryObject, "first");
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool zr_container_set_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *value,
                                           TZrSize *outIndex) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (state == ZR_NULL || entries == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, value);
    length = ZrLib_Array_Length(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(state, entries, index);
        if (entryValue != ZR_NULL && zr_container_value_hash(state, entryValue) == wantedHash &&
            zr_container_values_equal(state, entryValue, value)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void zr_container_array_sync_shape(SZrState *state, SZrObject *arrayObject) {
    TZrInt64 length;
    TZrInt64 capacity;
    SZrObject *items;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return;
    }

    items = zr_container_ensure_hidden_array(state, arrayObject, kContainerItemsField);
    length = (TZrInt64)ZrLib_Array_Length(items);
    capacity = zr_container_get_int_field(state, arrayObject, "capacity", 0);
    if (capacity < length) {
        capacity = length;
    }
    zr_container_set_int_field(state, arrayObject, "length", length);
    zr_container_set_int_field(state, arrayObject, "capacity", capacity);
}

static TZrBool zr_container_array_ensure_capacity(SZrState *state, SZrObject *arrayObject, TZrSize requiredLength) {
    TZrInt64 capacity;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    capacity = zr_container_get_int_field(state, arrayObject, "capacity", 0);
    if ((TZrSize)capacity >= requiredLength) {
        return ZR_TRUE;
    }

    if (capacity <= 0) {
        capacity = 4;
    }
    while ((TZrSize)capacity < requiredLength) {
        capacity *= 2;
    }
    zr_container_set_int_field(state, arrayObject, "capacity", capacity);
    return ZR_TRUE;
}

static SZrObject *zr_container_iterator_make(SZrState *state,
                                             SZrObject *source,
                                             EZrValueType sourceType,
                                             TZrInt64 indexValue,
                                             SZrObject *nextNode,
                                             FZrNativeFunction moveNextFunction) {
    SZrObject *iterator;
    SZrClosureNative *closure;
    SZrTypeValue moveNextValue;
    SZrTypeValue sourceValue;

    if (state == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    iterator = ZrLib_Object_New(state);
    closure = ZrCore_ClosureNative_New(state, 0);
    if (iterator == ZR_NULL || closure == ZR_NULL) {
        return ZR_NULL;
    }

    closure->nativeFunction = moveNextFunction;
    ZrCore_Value_InitAsRawObject(state, &moveNextValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    moveNextValue.isNative = ZR_TRUE;
    ZrLib_Object_SetFieldCString(state, iterator, "moveNext", &moveNextValue);
    zr_container_set_null_field(state, iterator, "current");
    if (source != ZR_NULL) {
        ZrLib_Value_SetObject(state, &sourceValue, source, sourceType);
        ZrLib_Object_SetFieldCString(state, iterator, kContainerSourceField, &sourceValue);
    } else {
        zr_container_set_null_field(state, iterator, kContainerSourceField);
    }
    zr_container_set_int_field(state, iterator, kContainerIndexField, indexValue);
    zr_container_set_object_field(state, iterator, kContainerNextNodeField, nextNode);
    return iterator;
}

static SZrObject *zr_container_iterator_self(SZrState *state) {
    SZrCallInfo *callInfo;
    SZrTypeValue *selfValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = state->callInfoList;
    selfValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, selfValue->value.object);
}

static TZrInt64 zr_container_iterator_finish_move_next(SZrState *state, TZrBool ok) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, ok, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *source;
    TZrInt64 index;
    const SZrTypeValue *current;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    source = zr_container_get_object_field(state, iterator, kContainerSourceField);
    index = zr_container_get_int_field(state, iterator, kContainerIndexField, 0);
    current = (source != ZR_NULL && index >= 0) ? ZrLib_Array_Get(state, source, (TZrSize)index) : ZR_NULL;
    if (current == ZR_NULL) {
        zr_container_set_null_field(state, iterator, "current");
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    zr_container_set_value_field(state, iterator, "current", current);
    zr_container_set_int_field(state, iterator, kContainerIndexField, index + 1);
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *node;
    const SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    node = zr_container_get_object_field(state, iterator, kContainerNextNodeField);
    if (node == ZR_NULL) {
        zr_container_set_null_field(state, iterator, "current");
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    value = zr_container_get_field_value(state, node, "value");
    if (value != ZR_NULL) {
        zr_container_set_value_field(state, iterator, "current", value);
    } else {
        zr_container_set_null_field(state, iterator, "current");
    }
    zr_container_set_object_field(state, iterator, kContainerNextNodeField, zr_container_get_object_field(state, node, "next"));
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

static TZrBool zr_container_pair_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *pair;
    TZrSize argc;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    argc = ZrLib_CallContext_ArgumentCount(context);
    if (argc != 0 && argc != 2) {
        ZrLib_CallContext_RaiseArityError(context, 0, 2);
        return ZR_FALSE;
    }

    pair = zr_container_resolve_construct_target(context, "Pair");
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argc == 2) {
        zr_container_set_value_field(context->state, pair, "first", ZrLib_CallContext_Argument(context, 0));
        zr_container_set_value_field(context->state, pair, "second", ZrLib_CallContext_Argument(context, 1));
    } else {
        zr_container_set_null_field(context->state, pair, "first");
        zr_container_set_null_field(context->state, pair, "second");
    }

    return zr_container_finish_object(context, result, pair);
}

static TZrBool zr_container_pair_equals(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    const SZrTypeValue *selfFirst;
    const SZrTypeValue *selfSecond;
    const SZrTypeValue *otherFirst;
    const SZrTypeValue *otherSecond;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    if (!zr_container_object_type_equals(other, "Pair")) {
        ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    selfFirst = zr_container_get_field_value(context->state, self, "first");
    selfSecond = zr_container_get_field_value(context->state, self, "second");
    otherFirst = zr_container_get_field_value(context->state, other, "first");
    otherSecond = zr_container_get_field_value(context->state, other, "second");
    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_container_values_equal(context->state, selfFirst, otherFirst) &&
                                zr_container_values_equal(context->state, selfSecond, otherSecond));
    return ZR_TRUE;
}

static TZrBool zr_container_pair_compare(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    TZrInt64 compare;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        ZrLib_Value_SetInt(context->state, result, 1);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    compare = zr_container_values_compare(context->state,
                                          zr_container_get_field_value(context->state, self, "first"),
                                          zr_container_get_field_value(context->state, other, "first"));
    if (compare == 0) {
        compare = zr_container_values_compare(context->state,
                                              zr_container_get_field_value(context->state, self, "second"),
                                              zr_container_get_field_value(context->state, other, "second"));
    }
    ZrLib_Value_SetInt(context->state, result, compare);
    return ZR_TRUE;
}

static TZrBool zr_container_pair_hash_code(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    TZrUInt64 firstHash;
    TZrUInt64 secondHash;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL) {
        return ZR_FALSE;
    }

    firstHash = zr_container_value_hash(context->state, zr_container_get_field_value(context->state, self, "first"));
    secondHash = zr_container_value_hash(context->state, zr_container_get_field_value(context->state, self, "second"));
    ZrLib_Value_SetInt(context->state, result, (TZrInt64)((firstHash * 16777619ULL) ^ (secondHash + 31ULL)));
    return ZR_TRUE;
}

static TZrBool zr_container_array_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *arrayObject = zr_container_resolve_construct_target(context, "Array");
    TZrInt64 capacity = 0;
    SZrObject *items;

    if (arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLib_CallContext_ArgumentCount(context) == 1 && !ZrLib_CallContext_ReadInt(context, 0, &capacity)) {
        return ZR_FALSE;
    }
    if (capacity < 0) {
        ZrCore_Debug_RunError(context->state, "Array capacity must be non-negative");
    }

    items = ZrLib_Array_New(context->state);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_set_object_field(context->state, arrayObject, kContainerItemsField, items);
    zr_container_set_int_field(context->state, arrayObject, "length", 0);
    zr_container_set_int_field(context->state, arrayObject, "capacity", capacity);
    return zr_container_finish_object(context, result, arrayObject);
}

static TZrBool zr_container_array_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    value = ZrLib_CallContext_Argument(context, 0);
    length = ZrLib_Array_Length(items);
    if (value == ZR_NULL || !zr_container_array_ensure_capacity(context->state, self, length + 1) ||
        !ZrLib_Array_PushValue(context->state, items, value)) {
        return ZR_FALSE;
    }

    zr_container_array_sync_shape(context->state, self);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_insert(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    length = ZrLib_Array_Length(items);
    if (indexValue < 0 || (TZrSize)indexValue > length) {
        ZrCore_Debug_RunError(context->state, "Array.insert index out of range");
    }
    if (!zr_container_array_ensure_capacity(context->state, self, length + 1) ||
        !zr_container_storage_insert(context->state, items, (TZrSize)indexValue, ZrLib_CallContext_Argument(context, 1))) {
        return ZR_FALSE;
    }

    zr_container_array_sync_shape(context->state, self);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_remove_at(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    if (indexValue < 0 || !zr_container_storage_remove_at(context->state, items, (TZrSize)indexValue)) {
        ZrCore_Debug_RunError(context->state, "Array.removeAt index out of range");
    }

    zr_container_array_sync_shape(context->state, self);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = ZrLib_Array_New(context->state);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_set_object_field(context->state, self, kContainerItemsField, items);
    zr_container_set_int_field(context->state, self, "length", 0);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_index_of(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = ZrLib_Array_Length(items);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *candidate = ZrLib_Array_Get(context->state, items, index);
        if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
            ZrLib_Value_SetInt(context->state, result, (TZrInt64)index);
            return ZR_TRUE;
        }
    }

    ZrLib_Value_SetInt(context->state, result, -1);
    return ZR_TRUE;
}

static TZrBool zr_container_array_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue indexResult;
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_array_index_of(context, &indexResult)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result, indexResult.value.nativeObject.nativeInt64 >= 0);
    return ZR_TRUE;
}

static TZrBool zr_container_array_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    iterator = zr_container_iterator_make(context->state,
                                          items,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static TZrInt64 zr_container_native_array_get_iterator_native(SZrState *state) {
    SZrCallInfo *callInfo;
    TZrStackValuePointer base;
    SZrTypeValue *resultValue;
    SZrTypeValue *selfValue;
    SZrObject *self;
    SZrObject *iterator;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    base = callInfo->functionBase.valuePointer;
    resultValue = ZrCore_Stack_GetValue(base);
    selfValue = ZrCore_Stack_GetValue(base + 1);
    if (resultValue == ZR_NULL || selfValue == ZR_NULL ||
        selfValue->type != ZR_VALUE_TYPE_ARRAY || selfValue->value.object == ZR_NULL) {
        if (resultValue != ZR_NULL) {
            ZrLib_Value_SetNull(resultValue);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    iterator = zr_container_iterator_make(state,
                                          self,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    if (iterator == ZR_NULL) {
        ZrLib_Value_SetNull(resultValue);
    } else {
        ZrLib_Value_SetObject(state, resultValue, iterator, ZR_VALUE_TYPE_OBJECT);
    }

    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrBool zr_container_array_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    value = indexValue >= 0 ? ZrLib_Array_Get(context->state, items, (TZrSize)indexValue) : ZR_NULL;
    if (value != ZR_NULL) {
        ZrCore_Value_Copy(context->state, result, value);
    } else {
        ZrLib_Value_SetNull(result);
    }
    return ZR_TRUE;
}

static TZrBool zr_container_array_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_hidden_array(context->state, self, kContainerItemsField);
    if (indexValue < 0 || (TZrSize)indexValue >= ZrLib_Array_Length(items)) {
        ZrCore_Debug_RunError(context->state, "Array index out of range");
    }

    zr_container_storage_set(context->state, items, (TZrSize)indexValue, ZrLib_CallContext_Argument(context, 1));
    zr_container_array_sync_shape(context->state, self);
    ZrCore_Value_Copy(context->state, result, ZrLib_CallContext_Argument(context, 1));
    return ZR_TRUE;
}

static TZrBool zr_container_map_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context, "Map");
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_set_object_field(context->state, self, kContainerEntriesField, entries);
    zr_container_set_int_field(context->state, self, "count", 0);
    return zr_container_finish_object(context, result, self);
}

static TZrBool zr_container_map_contains_key(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrBool found;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    found = zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL);
    ZrLib_Value_SetBool(context->state, result, found);
    return ZR_TRUE;
}

static TZrBool zr_container_map_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    if (!zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    zr_container_set_int_field(context->state, self, "count", (TZrInt64)ZrLib_Array_Length(entries));
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_container_map_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || result == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_set_object_field(context->state, self, kContainerEntriesField, entries);
    zr_container_set_int_field(context->state, self, "count", 0);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_map_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static TZrBool zr_container_map_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;
    const SZrTypeValue *entryValue;
    SZrObject *entryObject;
    const SZrTypeValue *mappedValue;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    if (!zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    entryValue = ZrLib_Array_Get(context->state, entries, index);
    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    entryObject = ZR_CAST_OBJECT(context->state, entryValue->value.object);
    mappedValue = zr_container_get_field_value(context->state, entryObject, "second");
    if (mappedValue != ZR_NULL) {
        ZrCore_Value_Copy(context->state, result, mappedValue);
    } else {
        ZrLib_Value_SetNull(result);
    }
    return ZR_TRUE;
}

static TZrBool zr_container_map_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;
    const SZrTypeValue *keyValue;
    const SZrTypeValue *mappedValue;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    keyValue = ZrLib_CallContext_Argument(context, 0);
    mappedValue = ZrLib_CallContext_Argument(context, 1);
    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    if (zr_container_map_find_index(context->state, entries, keyValue, &index)) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(context->state, entries, index);
        if (entryValue != ZR_NULL && entryValue->type == ZR_VALUE_TYPE_OBJECT && entryValue->value.object != ZR_NULL) {
            zr_container_set_value_field(context->state, ZR_CAST_OBJECT(context->state, entryValue->value.object), "second", mappedValue);
        }
    } else {
        SZrObject *pair = zr_container_make_pair(context->state, keyValue, mappedValue);
        SZrTypeValue pairValue;
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrLib_Value_SetObject(context->state, &pairValue, pair, ZR_VALUE_TYPE_OBJECT);
        ZrLib_Array_PushValue(context->state, entries, &pairValue);
    }

    zr_container_set_int_field(context->state, self, "count", (TZrInt64)ZrLib_Array_Length(entries));
    ZrCore_Value_Copy(context->state, result, mappedValue);
    return ZR_TRUE;
}

static TZrBool zr_container_set_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context, "Set");
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_set_object_field(context->state, self, kContainerEntriesField, entries);
    zr_container_set_int_field(context->state, self, "count", 0);
    return zr_container_finish_object(context, result, self);
}

static TZrBool zr_container_set_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    value = ZrLib_CallContext_Argument(context, 0);
    if (zr_container_set_find_index(context->state, entries, value, ZR_NULL)) {
        ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    ZrLib_Array_PushValue(context->state, entries, value);
    zr_container_set_int_field(context->state, self, "count", (TZrInt64)ZrLib_Array_Length(entries));
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_container_set_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL));
    return ZR_TRUE;
}

static TZrBool zr_container_set_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    if (!zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    zr_container_set_int_field(context->state, self, "count", (TZrInt64)ZrLib_Array_Length(entries));
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_container_set_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || result == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_set_object_field(context->state, self, kContainerEntriesField, entries);
    zr_container_set_int_field(context->state, self, "count", 0);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_set_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_hidden_array(context->state, self, kContainerEntriesField);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static TZrBool zr_container_linked_node_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *node = zr_container_resolve_construct_target(context, "LinkedNode");
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 0) {
        zr_container_set_value_field(context->state, node, "value", ZrLib_CallContext_Argument(context, 0));
    } else {
        zr_container_set_null_field(context->state, node, "value");
    }
    zr_container_set_null_field(context->state, node, "next");
    zr_container_set_null_field(context->state, node, "previous");
    return zr_container_finish_object(context, result, node);
}

static void zr_container_linked_list_unlink_node(SZrState *state, SZrObject *list, SZrObject *node) {
    SZrObject *previous;
    SZrObject *next;
    TZrInt64 count;

    if (state == ZR_NULL || list == ZR_NULL || node == ZR_NULL) {
        return;
    }

    previous = zr_container_get_object_field(state, node, "previous");
    next = zr_container_get_object_field(state, node, "next");
    if (previous != ZR_NULL) {
        zr_container_set_object_field(state, previous, "next", next);
    } else {
        zr_container_set_object_field(state, list, "first", next);
    }
    if (next != ZR_NULL) {
        zr_container_set_object_field(state, next, "previous", previous);
    } else {
        zr_container_set_object_field(state, list, "last", previous);
    }

    zr_container_set_null_field(state, node, "next");
    zr_container_set_null_field(state, node, "previous");
    count = zr_container_get_int_field(state, list, "count", 0);
    zr_container_set_int_field(state, list, "count", count > 0 ? count - 1 : 0);
}

static TZrBool zr_container_linked_list_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context, "LinkedList");
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }
    zr_container_set_int_field(context->state, self, "count", 0);
    zr_container_set_null_field(context->state, self, "first");
    zr_container_set_null_field(context->state, self, "last");
    return zr_container_finish_object(context, result, self);
}

static TZrBool zr_container_linked_list_add_first(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *first;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field(context->state, self, "first");
    zr_container_set_object_field(context->state, node, "next", first);
    if (first != ZR_NULL) {
        zr_container_set_object_field(context->state, first, "previous", node);
    } else {
        zr_container_set_object_field(context->state, self, "last", node);
    }
    zr_container_set_object_field(context->state, self, "first", node);
    count = zr_container_get_int_field(context->state, self, "count", 0);
    zr_container_set_int_field(context->state, self, "count", count + 1);
    return zr_container_finish_object(context, result, node);
}

static TZrBool zr_container_linked_list_add_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *last;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field(context->state, self, "last");
    zr_container_set_object_field(context->state, node, "previous", last);
    if (last != ZR_NULL) {
        zr_container_set_object_field(context->state, last, "next", node);
    } else {
        zr_container_set_object_field(context->state, self, "first", node);
    }
    zr_container_set_object_field(context->state, self, "last", node);
    count = zr_container_get_int_field(context->state, self, "count", 0);
    zr_container_set_int_field(context->state, self, "count", count + 1);
    return zr_container_finish_object(context, result, node);
}

static TZrBool zr_container_linked_list_remove_first(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *first;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field(context->state, self, "first");
    if (first == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    value = zr_container_get_field_value(context->state, first, "value");
    if (value != ZR_NULL) {
        ZrCore_Value_Copy(context->state, result, value);
    } else {
        ZrLib_Value_SetNull(result);
    }
    zr_container_linked_list_unlink_node(context->state, self, first);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_remove_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *last;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field(context->state, self, "last");
    if (last == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    value = zr_container_get_field_value(context->state, last, "value");
    if (value != ZR_NULL) {
        ZrCore_Value_Copy(context->state, result, value);
    } else {
        ZrLib_Value_SetNull(result);
    }
    zr_container_linked_list_unlink_node(context->state, self, last);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    const SZrTypeValue *needle;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    needle = ZrLib_CallContext_Argument(context, 0);
    node = zr_container_get_object_field(context->state, self, "first");
    while (node != ZR_NULL) {
        const SZrTypeValue *value = zr_container_get_field_value(context->state, node, "value");
        if (value != ZR_NULL && zr_container_values_equal(context->state, value, needle)) {
            zr_container_linked_list_unlink_node(context->state, self, node);
            ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
            return ZR_TRUE;
        }
        node = zr_container_get_object_field(context->state, node, "next");
    }

    ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_get_object_field(context->state, self, "first");
    while (node != ZR_NULL) {
        SZrObject *next = zr_container_get_object_field(context->state, node, "next");
        zr_container_set_null_field(context->state, node, "next");
        zr_container_set_null_field(context->state, node, "previous");
        node = next;
    }

    zr_container_set_null_field(context->state, self, "first");
    zr_container_set_null_field(context->state, self, "last");
    zr_container_set_int_field(context->state, self, "count", 0);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    iterator = zr_container_iterator_make(context->state,
                                          self,
                                          ZR_VALUE_TYPE_OBJECT,
                                          0,
                                          zr_container_get_object_field(context->state, self, "first"),
                                          zr_container_linked_list_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static const ZrLibParameterDescriptor kArrayValueParameter[] = {{"value", "T", ZR_NULL}};
static const ZrLibParameterDescriptor kArrayInsertParameters[] = {{"index", "int", ZR_NULL}, {"value", "T", ZR_NULL}};
static const ZrLibParameterDescriptor kArrayIndexParameter[] = {{"index", "int", ZR_NULL}};
static const ZrLibParameterDescriptor kMapKeyParameter[] = {{"key", "K", ZR_NULL}};
static const ZrLibParameterDescriptor kMapSetItemParameters[] = {{"key", "K", ZR_NULL}, {"value", "V", ZR_NULL}};
static const ZrLibParameterDescriptor kSetValueParameter[] = {{"value", "T", ZR_NULL}};
static const ZrLibParameterDescriptor kPairParameters[] = {{"first", "K", ZR_NULL}, {"second", "V", ZR_NULL}};
static const ZrLibParameterDescriptor kPairOtherParameter[] = {{"other", "Pair<K,V>", ZR_NULL}};
static const ZrLibParameterDescriptor kLinkedNodeValueParameter[] = {{"value", "T", ZR_NULL}};

static const TZrChar *kMapKeyConstraints[] = {"Hashable", "Equatable"};
static const TZrChar *kSetValueConstraints[] = {"Hashable", "Equatable"};
static const TZrChar *kArrayImplements[] = {"ArrayLike<T>", "Iterable<T>"};
static const TZrChar *kMapImplements[] = {"Iterable<Pair<K,V>>"};
static const TZrChar *kSetImplements[] = {"Iterable<T>"};
static const TZrChar *kPairImplements[] = {"Equatable<Pair<K,V>>", "Comparable<Pair<K,V>>", "Hashable"};
static const TZrChar *kLinkedListImplements[] = {"Iterable<T>"};

static const ZrLibGenericParameterDescriptor kSingleGenericT[] = {{"T", ZR_NULL, ZR_NULL, 0}};
static const ZrLibGenericParameterDescriptor kGenericKV[] = {{"K", ZR_NULL, ZR_NULL, 0}, {"V", ZR_NULL, ZR_NULL, 0}};
static const ZrLibGenericParameterDescriptor kMapGenericParameters[] = {
        {"K", ZR_NULL, kMapKeyConstraints, ZR_ARRAY_COUNT(kMapKeyConstraints)},
        {"V", ZR_NULL, ZR_NULL, 0},
};
static const ZrLibGenericParameterDescriptor kSetGenericParameters[] = {
        {"T", ZR_NULL, kSetValueConstraints, ZR_ARRAY_COUNT(kSetValueConstraints)},
};

static const ZrLibFieldDescriptor kIteratorFields[] = {{"current", "T", ZR_NULL}};
static const ZrLibMethodDescriptor kIteratorMethods[] = {
        {"moveNext", 0, 0, ZR_NULL, "bool", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibMethodDescriptor kIterableMethods[] = {
        {"getIterator", 0, 0, ZR_NULL, "Iterator<T>", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibFieldDescriptor kArrayLikeFields[] = {{"length", "int", ZR_NULL}};
static const ZrLibMetaMethodDescriptor kArrayLikeMetaMethods[] = {
        {ZR_META_GET_ITEM, 1, 1, ZR_NULL, "T", ZR_NULL, kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)},
        {ZR_META_SET_ITEM, 2, 2, ZR_NULL, "T", ZR_NULL, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
};
static const ZrLibMethodDescriptor kEquatableMethods[] = {
        {"equals", 1, 1, ZR_NULL, "bool", ZR_NULL, ZR_FALSE, kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)},
};
static const ZrLibMethodDescriptor kComparableMethods[] = {
        {"compareTo", 1, 1, ZR_NULL, "int", ZR_NULL, ZR_FALSE, kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)},
};
static const ZrLibMethodDescriptor kHashableMethods[] = {
        {"hashCode", 0, 0, ZR_NULL, "int", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};

static const ZrLibFieldDescriptor kArrayFields[] = {{"length", "int", ZR_NULL}, {"capacity", "int", ZR_NULL}};
static const ZrLibMethodDescriptor kArrayMethods[] = {
        {"add", 1, 1, zr_container_array_add, "null", ZR_NULL, ZR_FALSE, kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)},
        {"insert", 2, 2, zr_container_array_insert, "null", ZR_NULL, ZR_FALSE, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
        {"removeAt", 1, 1, zr_container_array_remove_at, "null", ZR_NULL, ZR_FALSE, kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)},
        {"clear", 0, 0, zr_container_array_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
        {"contains", 1, 1, zr_container_array_contains, "bool", ZR_NULL, ZR_FALSE, kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)},
        {"indexOf", 1, 1, zr_container_array_index_of, "int", ZR_NULL, ZR_FALSE, kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)},
        {"getIterator", 0, 0, zr_container_array_get_iterator, "Iterator<T>", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibMetaMethodDescriptor kArrayMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_array_constructor, "Array<T>", ZR_NULL, kArrayIndexParameter, 1},
        {ZR_META_GET_ITEM, 1, 1, zr_container_array_get_item, "T", ZR_NULL, kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)},
        {ZR_META_SET_ITEM, 2, 2, zr_container_array_set_item, "T", ZR_NULL, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
};

static const ZrLibFieldDescriptor kMapFields[] = {{"count", "int", ZR_NULL}};
static const ZrLibMethodDescriptor kMapMethods[] = {
        {"containsKey", 1, 1, zr_container_map_contains_key, "bool", ZR_NULL, ZR_FALSE, kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)},
        {"remove", 1, 1, zr_container_map_remove, "bool", ZR_NULL, ZR_FALSE, kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)},
        {"clear", 0, 0, zr_container_map_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
        {"getIterator", 0, 0, zr_container_map_get_iterator, "Iterator<Pair<K,V>>", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibMetaMethodDescriptor kMapMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_map_constructor, "Map<K,V>", ZR_NULL, ZR_NULL, 0},
        {ZR_META_GET_ITEM, 1, 1, zr_container_map_get_item, "V", ZR_NULL, kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)},
        {ZR_META_SET_ITEM, 2, 2, zr_container_map_set_item, "V", ZR_NULL, kMapSetItemParameters, ZR_ARRAY_COUNT(kMapSetItemParameters)},
};

static const ZrLibFieldDescriptor kSetFields[] = {{"count", "int", ZR_NULL}};
static const ZrLibMethodDescriptor kSetMethods[] = {
        {"add", 1, 1, zr_container_set_add, "bool", ZR_NULL, ZR_FALSE, kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)},
        {"contains", 1, 1, zr_container_set_contains, "bool", ZR_NULL, ZR_FALSE, kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)},
        {"remove", 1, 1, zr_container_set_remove, "bool", ZR_NULL, ZR_FALSE, kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)},
        {"clear", 0, 0, zr_container_set_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
        {"getIterator", 0, 0, zr_container_set_get_iterator, "Iterator<T>", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibMetaMethodDescriptor kSetMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_set_constructor, "Set<T>", ZR_NULL, ZR_NULL, 0},
};

static const ZrLibFieldDescriptor kPairFields[] = {{"first", "K", ZR_NULL}, {"second", "V", ZR_NULL}};
static const ZrLibMethodDescriptor kPairMethods[] = {
        {"equals", 1, 1, zr_container_pair_equals, "bool", ZR_NULL, ZR_FALSE, kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)},
        {"compareTo", 1, 1, zr_container_pair_compare, "int", ZR_NULL, ZR_FALSE, kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)},
        {"hashCode", 0, 0, zr_container_pair_hash_code, "int", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibMetaMethodDescriptor kPairMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 2, zr_container_pair_constructor, "Pair<K,V>", ZR_NULL, kPairParameters, ZR_ARRAY_COUNT(kPairParameters)},
        {ZR_META_COMPARE, 1, 1, zr_container_pair_compare, "int", ZR_NULL, kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)},
};

static const ZrLibFieldDescriptor kLinkedListFields[] = {
        {"count", "int", ZR_NULL},
        {"first", "LinkedNode<T>", ZR_NULL},
        {"last", "LinkedNode<T>", ZR_NULL},
};
static const ZrLibMethodDescriptor kLinkedListMethods[] = {
        {"addFirst", 1, 1, zr_container_linked_list_add_first, "LinkedNode<T>", ZR_NULL, ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
        {"addLast", 1, 1, zr_container_linked_list_add_last, "LinkedNode<T>", ZR_NULL, ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
        {"removeFirst", 0, 0, zr_container_linked_list_remove_first, "T", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
        {"removeLast", 0, 0, zr_container_linked_list_remove_last, "T", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
        {"remove", 1, 1, zr_container_linked_list_remove, "bool", ZR_NULL, ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
        {"clear", 0, 0, zr_container_linked_list_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
        {"getIterator", 0, 0, zr_container_linked_list_get_iterator, "Iterator<T>", ZR_NULL, ZR_FALSE, ZR_NULL, 0},
};
static const ZrLibMetaMethodDescriptor kLinkedListMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_linked_list_constructor, "LinkedList<T>", ZR_NULL, ZR_NULL, 0},
};

static const ZrLibFieldDescriptor kLinkedNodeFields[] = {
        {"value", "T", ZR_NULL},
        {"next", "LinkedNode<T>", ZR_NULL},
        {"previous", "LinkedNode<T>", ZR_NULL},
};
static const ZrLibMetaMethodDescriptor kLinkedNodeMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_linked_node_constructor, "LinkedNode<T>", ZR_NULL, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
};

static const ZrLibTypeDescriptor g_container_types[] = {
        {"Iterable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0, kIterableMethods, ZR_ARRAY_COUNT(kIterableMethods), ZR_NULL, 0, ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "Iterable<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"Iterator", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, kIteratorFields, ZR_ARRAY_COUNT(kIteratorFields), kIteratorMethods, ZR_ARRAY_COUNT(kIteratorMethods), ZR_NULL, 0, ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "Iterator<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"ArrayLike", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, kArrayLikeFields, ZR_ARRAY_COUNT(kArrayLikeFields), ZR_NULL, 0, kArrayLikeMetaMethods, ZR_ARRAY_COUNT(kArrayLikeMetaMethods), ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "ArrayLike<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"Equatable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0, kEquatableMethods, ZR_ARRAY_COUNT(kEquatableMethods), ZR_NULL, 0, ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "Equatable<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"Comparable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0, kComparableMethods, ZR_ARRAY_COUNT(kComparableMethods), ZR_NULL, 0, ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "Comparable<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"Hashable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0, kHashableMethods, ZR_ARRAY_COUNT(kHashableMethods), ZR_NULL, 0, ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "Hashable()", ZR_NULL, 0},
        {"Array", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kArrayFields, ZR_ARRAY_COUNT(kArrayFields), kArrayMethods, ZR_ARRAY_COUNT(kArrayMethods), kArrayMetaMethods, ZR_ARRAY_COUNT(kArrayMetaMethods), ZR_NULL, ZR_NULL, kArrayImplements, ZR_ARRAY_COUNT(kArrayImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Array<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"Map", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kMapFields, ZR_ARRAY_COUNT(kMapFields), kMapMethods, ZR_ARRAY_COUNT(kMapMethods), kMapMetaMethods, ZR_ARRAY_COUNT(kMapMetaMethods), ZR_NULL, ZR_NULL, kMapImplements, ZR_ARRAY_COUNT(kMapImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Map<K,V>()", kMapGenericParameters, ZR_ARRAY_COUNT(kMapGenericParameters)},
        {"Set", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kSetFields, ZR_ARRAY_COUNT(kSetFields), kSetMethods, ZR_ARRAY_COUNT(kSetMethods), kSetMetaMethods, ZR_ARRAY_COUNT(kSetMetaMethods), ZR_NULL, ZR_NULL, kSetImplements, ZR_ARRAY_COUNT(kSetImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Set<T>()", kSetGenericParameters, ZR_ARRAY_COUNT(kSetGenericParameters)},
        {"Pair", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kPairFields, ZR_ARRAY_COUNT(kPairFields), kPairMethods, ZR_ARRAY_COUNT(kPairMethods), kPairMetaMethods, ZR_ARRAY_COUNT(kPairMetaMethods), ZR_NULL, ZR_NULL, kPairImplements, ZR_ARRAY_COUNT(kPairImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Pair<K,V>(first: K, second: V)", kGenericKV, ZR_ARRAY_COUNT(kGenericKV)},
        {"LinkedList", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedListFields, ZR_ARRAY_COUNT(kLinkedListFields), kLinkedListMethods, ZR_ARRAY_COUNT(kLinkedListMethods), kLinkedListMetaMethods, ZR_ARRAY_COUNT(kLinkedListMetaMethods), ZR_NULL, ZR_NULL, kLinkedListImplements, ZR_ARRAY_COUNT(kLinkedListImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedList<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
        {"LinkedNode", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedNodeFields, ZR_ARRAY_COUNT(kLinkedNodeFields), ZR_NULL, 0, kLinkedNodeMetaMethods, ZR_ARRAY_COUNT(kLinkedNodeMetaMethods), ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedNode<T>(value: T)", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT)},
};

static const ZrLibModuleDescriptor g_container_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.container",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = ZR_NULL,
        .functionCount = 0,
        .types = g_container_types,
        .typeCount = ZR_ARRAY_COUNT(g_container_types),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Built-in generic container and interface module.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = ZR_NULL,
        .minRuntimeAbi = 0,
        .requiredCapabilities = 0,
};

const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void) {
    return &g_container_module_descriptor;
}

static TZrBool zr_container_install_basic_array_method(SZrState *state,
                                                       const TZrChar *methodName,
                                                       FZrNativeFunction nativeFunction) {
    SZrObjectPrototype *prototype;
    SZrObject *prototypeObject;
    SZrClosureNative *closure;
    SZrTypeValue closureValue;

    if (state == ZR_NULL || state->global == ZR_NULL || methodName == ZR_NULL || nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    prototypeObject = &prototype->super;
    if (ZrLib_Object_GetFieldCString(state, prototypeObject, methodName) != ZR_NULL) {
        return ZR_TRUE;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = nativeFunction;
    ZrCore_Value_InitAsRawObject(state, &closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue.isNative = ZR_TRUE;
    ZrLib_Object_SetFieldCString(state, prototypeObject, methodName, &closureValue);
    return ZR_TRUE;
}

static TZrBool zr_container_install_basic_array_adapter(SZrGlobalState *global) {
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_install_basic_array_method(global->mainThreadState,
                                                   "getIterator",
                                                   zr_container_native_array_get_iterator_native);
}

TZrBool ZrVmLibContainer_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_container_module_descriptor) &&
           zr_container_install_basic_array_adapter(global);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibContainer_GetModuleDescriptor();
}
#endif
