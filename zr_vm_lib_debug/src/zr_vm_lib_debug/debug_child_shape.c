#include "debug_internal.h"

TZrSize zr_debug_count_visible_object_entries(const SZrHashSet *set) {
    TZrSize bucketIndex;
    TZrSize count = 0;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return 0;
    }

    for (bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
        while (pair != ZR_NULL) {
            const TZrChar *keyText = pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL
                                             ? ZrCore_String_GetNativeString(ZR_CAST_STRING(ZR_NULL, pair->key.value.object))
                                             : ZR_NULL;
            if (keyText == ZR_NULL || strncmp(keyText, "__zr_", 5) != 0) {
                count++;
            }
            pair = pair->next;
        }
    }

    return count;
}

void zr_debug_value_child_shape(SZrState *state,
                                const SZrTypeValue *value,
                                TZrSize *outNamedVariables,
                                TZrSize *outIndexedVariables) {
    SZrObject *object;
    SZrObject *indexedStorage = ZR_NULL;
    SZrObjectPrototype *prototype;
    const TZrChar *indexedSyntheticName = ZR_NULL;
    TZrSize indexedCount = 0;
    TZrSize namedVariables = 0;
    TZrSize indexedVariables = 0;

    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }

    if (state == ZR_NULL || value == ZR_NULL ||
        !(value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL) {
        return;
    }

    if (zr_debug_try_resolve_indexed_storage(state, value, &indexedStorage, &indexedSyntheticName, &indexedCount) &&
        object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        indexedVariables = indexedCount;
    } else {
        namedVariables = zr_debug_count_visible_object_entries(&object->nodeMap);
        if (indexedStorage != ZR_NULL && indexedSyntheticName != ZR_NULL) {
            namedVariables++;
        }

        prototype = zr_debug_resolve_value_prototype(state, value);
        if (prototype != ZR_NULL) {
            namedVariables += ZR_DEBUG_INSTANCE_SYNTHETIC_FIELD_COUNT;
        }
    }

    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = namedVariables;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = indexedVariables;
    }
}
