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

static const TZrChar *zr_debug_semantic_type_name(SZrState *state, const SZrTypeValue *value) {
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || value == ZR_NULL) {
        return "value";
    }

    prototype = zr_debug_resolve_value_prototype(state, value);
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
        prototype != ZR_NULL &&
        prototype->name != ZR_NULL) {
        return zr_debug_string_native(prototype->name);
    }

    return zr_debug_value_type_name(value->type);
}

static const TZrChar *zr_debug_semantic_ownership_name(EZrOwnershipValueKind kind) {
    switch (kind) {
        case ZR_OWNERSHIP_VALUE_KIND_UNIQUE:
            return "unique";
        case ZR_OWNERSHIP_VALUE_KIND_SHARED:
            return "shared";
        case ZR_OWNERSHIP_VALUE_KIND_WEAK:
            return "weak";
        case ZR_OWNERSHIP_VALUE_KIND_BORROWED:
            return "borrowed";
        case ZR_OWNERSHIP_VALUE_KIND_LOANED:
            return "loaned";
        case ZR_OWNERSHIP_VALUE_KIND_NONE:
        default:
            return "";
    }
}

static void zr_debug_semantic_append_ownership(const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize) {
    const TZrChar *ownershipName;
    TZrSize length;

    if (value == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    ownershipName = zr_debug_semantic_ownership_name(value->ownershipKind);
    if (ownershipName[0] == '\0') {
        return;
    }

    length = strlen(buffer);
    if (length + 1u >= bufferSize) {
        return;
    }

    snprintf(buffer + length, bufferSize - length, ", ownership %s", ownershipName);
    buffer[bufferSize - 1u] = '\0';
}

void zr_debug_value_semantic_summary(SZrState *state,
                                     const SZrTypeValue *value,
                                     TZrSize namedVariables,
                                     TZrSize indexedVariables,
                                     TZrChar *buffer,
                                     TZrSize bufferSize) {
    const TZrChar *typeName;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (value == ZR_NULL) {
        return;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            zr_debug_copy_text(buffer, bufferSize, "null value");
            break;
        case ZR_VALUE_TYPE_BOOL:
            snprintf(buffer,
                     bufferSize,
                     "logical %s",
                     value->value.nativeObject.nativeBool ? "true" : "false");
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            snprintf(buffer,
                     bufferSize,
                     "integer %lld",
                     (long long)value->value.nativeObject.nativeInt64);
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            snprintf(buffer,
                     bufferSize,
                     "integer %llu",
                     (unsigned long long)value->value.nativeObject.nativeUInt64);
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            snprintf(buffer, bufferSize, "number %.15g", value->value.nativeObject.nativeDouble);
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            typeName = zr_debug_semantic_type_name(state, value);
            if (namedVariables > 0 || indexedVariables > 0) {
                snprintf(buffer,
                         bufferSize,
                         "expandable %s, named %llu, indexed %llu",
                         typeName,
                         (unsigned long long)namedVariables,
                         (unsigned long long)indexedVariables);
            } else {
                snprintf(buffer, bufferSize, "%s value", typeName);
            }
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            snprintf(buffer, bufferSize, "callable %s", zr_debug_semantic_type_name(state, value));
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_STRING:
            zr_debug_copy_text(buffer, bufferSize, "string value");
            break;
        default:
            snprintf(buffer, bufferSize, "%s value", zr_debug_semantic_type_name(state, value));
            buffer[bufferSize - 1u] = '\0';
            break;
    }

    zr_debug_semantic_append_ownership(value, buffer, bufferSize);
}
