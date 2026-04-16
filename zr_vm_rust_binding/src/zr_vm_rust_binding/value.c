#include "internal.h"

#include <stdlib.h>
#include <string.h>

static const SZrTypeValue *zr_rust_binding_live_value_ref(const ZrRustBindingValue *value) {
    if (value == ZR_NULL || value->storageKind != ZR_RUST_BINDING_VALUE_STORAGE_VM) {
        return ZR_NULL;
    }

    return ZrLib_TempValueRoot_Value((ZrLibTempValueRoot *)&value->storage.vmValue.root);
}

static TZrBool zr_rust_binding_owned_array_reserve(ZrRustBindingOwnedArray *arrayValue, TZrSize minimumCapacity) {
    ZrRustBindingValue **newItems;
    TZrSize newCapacity;

    if (arrayValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (arrayValue->capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = arrayValue->capacity == 0U ? 4U : arrayValue->capacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newItems = (ZrRustBindingValue **)realloc(arrayValue->items, newCapacity * sizeof(*newItems));
    if (newItems == ZR_NULL) {
        return ZR_FALSE;
    }

    arrayValue->items = newItems;
    arrayValue->capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_owned_object_reserve(ZrRustBindingOwnedObject *objectValue, TZrSize minimumCapacity) {
    ZrRustBindingOwnedObjectField *newFields;
    TZrSize newCapacity;

    if (objectValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (objectValue->capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = objectValue->capacity == 0U ? 4U : objectValue->capacity * 2U;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2U;
    }

    newFields = (ZrRustBindingOwnedObjectField *)realloc(objectValue->fields, newCapacity * sizeof(*newFields));
    if (newFields == ZR_NULL) {
        return ZR_FALSE;
    }

    objectValue->fields = newFields;
    objectValue->capacity = newCapacity;
    return ZR_TRUE;
}

static TZrSize zr_rust_binding_owned_object_find_field(const ZrRustBindingOwnedObject *objectValue,
                                                       const TZrChar *fieldName) {
    TZrSize index;

    if (objectValue == ZR_NULL || fieldName == ZR_NULL) {
        return 0U;
    }

    for (index = 0; index < objectValue->count; index++) {
        if (objectValue->fields[index].name != ZR_NULL &&
            strcmp(objectValue->fields[index].name, fieldName) == 0) {
            return index;
        }
    }

    return objectValue->count;
}

static ZrRustBindingValue *zr_rust_binding_value_clone(const ZrRustBindingValue *value);

static TZrBool zr_rust_binding_owned_array_push_clone(ZrRustBindingOwnedArray *arrayValue,
                                                      const ZrRustBindingValue *element) {
    ZrRustBindingValue *clone;

    if (arrayValue == ZR_NULL || element == ZR_NULL ||
        !zr_rust_binding_owned_array_reserve(arrayValue, arrayValue->count + 1U)) {
        return ZR_FALSE;
    }

    clone = zr_rust_binding_value_clone(element);
    if (clone == ZR_NULL) {
        return ZR_FALSE;
    }

    arrayValue->items[arrayValue->count++] = clone;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_owned_object_set_clone(ZrRustBindingOwnedObject *objectValue,
                                                      const TZrChar *fieldName,
                                                      const ZrRustBindingValue *fieldValue) {
    TZrSize index;
    ZrRustBindingValue *clone;
    TZrChar *fieldNameCopy;

    if (objectValue == ZR_NULL || fieldName == ZR_NULL || fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    clone = zr_rust_binding_value_clone(fieldValue);
    if (clone == ZR_NULL) {
        return ZR_FALSE;
    }

    index = zr_rust_binding_owned_object_find_field(objectValue, fieldName);
    if (index < objectValue->count) {
        zr_rust_binding_value_free_impl(objectValue->fields[index].value);
        objectValue->fields[index].value = clone;
        return ZR_TRUE;
    }

    if (!zr_rust_binding_owned_object_reserve(objectValue, objectValue->count + 1U)) {
        zr_rust_binding_value_free_impl(clone);
        return ZR_FALSE;
    }

    fieldNameCopy = zr_rust_binding_strdup(fieldName);
    if (fieldNameCopy == ZR_NULL) {
        zr_rust_binding_value_free_impl(clone);
        return ZR_FALSE;
    }

    objectValue->fields[objectValue->count].name = fieldNameCopy;
    objectValue->fields[objectValue->count].value = clone;
    objectValue->count++;
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_materialize_owned_array(SZrState *state,
                                                       const ZrRustBindingOwnedArray *arrayValue,
                                                       SZrTypeValue *outValue) {
    ZrLibTempValueRoot arrayRoot;
    SZrObject *arrayObject;
    TZrSize index;

    if (state == ZR_NULL || arrayValue == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_TempValueRoot_Begin(state, &arrayRoot)) {
        return ZR_FALSE;
    }

    arrayObject = ZrLib_Array_New(state);
    if (arrayObject == ZR_NULL || !ZrLib_TempValueRoot_SetObject(&arrayRoot, arrayObject, ZR_VALUE_TYPE_ARRAY)) {
        ZrLib_TempValueRoot_End(&arrayRoot);
        return ZR_FALSE;
    }

    for (index = 0; index < arrayValue->count; index++) {
        ZrLibTempValueRoot elementRoot;
        SZrTypeValue *elementValue;

        if (!ZrLib_TempValueRoot_Begin(state, &elementRoot)) {
            ZrLib_TempValueRoot_End(&arrayRoot);
            return ZR_FALSE;
        }

        elementValue = ZrLib_TempValueRoot_Value(&elementRoot);
        if (elementValue == ZR_NULL ||
            !zr_rust_binding_materialize_value(state, arrayValue->items[index], elementValue) ||
            !ZrLib_Array_PushValue(state, arrayObject, elementValue)) {
            ZrLib_TempValueRoot_End(&elementRoot);
            ZrLib_TempValueRoot_End(&arrayRoot);
            return ZR_FALSE;
        }

        ZrLib_TempValueRoot_End(&elementRoot);
    }

    ZrCore_Value_Copy(state, outValue, ZrLib_TempValueRoot_Value(&arrayRoot));
    ZrLib_TempValueRoot_End(&arrayRoot);
    return ZR_TRUE;
}

static TZrBool zr_rust_binding_materialize_owned_object(SZrState *state,
                                                        const ZrRustBindingOwnedObject *objectValue,
                                                        SZrTypeValue *outValue) {
    ZrLibTempValueRoot objectRoot;
    SZrObject *object;
    TZrSize index;

    if (state == ZR_NULL || objectValue == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_TempValueRoot_Begin(state, &objectRoot)) {
        return ZR_FALSE;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !ZrLib_TempValueRoot_SetObject(&objectRoot, object, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_FALSE;
    }

    for (index = 0; index < objectValue->count; index++) {
        ZrLibTempValueRoot fieldRoot;
        SZrTypeValue *fieldValue;

        if (!ZrLib_TempValueRoot_Begin(state, &fieldRoot)) {
            ZrLib_TempValueRoot_End(&objectRoot);
            return ZR_FALSE;
        }

        fieldValue = ZrLib_TempValueRoot_Value(&fieldRoot);
        if (fieldValue == ZR_NULL ||
            !zr_rust_binding_materialize_value(state, objectValue->fields[index].value, fieldValue)) {
            ZrLib_TempValueRoot_End(&fieldRoot);
            ZrLib_TempValueRoot_End(&objectRoot);
            return ZR_FALSE;
        }

        ZrLib_Object_SetFieldCString(state, object, objectValue->fields[index].name, fieldValue);
        ZrLib_TempValueRoot_End(&fieldRoot);
    }

    ZrCore_Value_Copy(state, outValue, ZrLib_TempValueRoot_Value(&objectRoot));
    ZrLib_TempValueRoot_End(&objectRoot);
    return ZR_TRUE;
}

TZrChar *zr_rust_binding_strdup(const TZrChar *value) {
    TZrSize length;
    TZrChar *copy;

    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(value);
    copy = (TZrChar *)malloc(length + 1U);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

TZrBool zr_rust_binding_copy_string_to_buffer(const TZrChar *source, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize length;

    if (source == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(source);
    if (length + 1U > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, source, length + 1U);
    return ZR_TRUE;
}

ZrRustBindingValueKind zr_rust_binding_map_value_kind(EZrValueType valueType) {
    if (ZR_VALUE_IS_TYPE_NULL(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_NULL;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_BOOL;
    }
    if (ZR_VALUE_IS_TYPE_INT(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_INT;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_FLOAT;
    }
    if (ZR_VALUE_IS_TYPE_STRING(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_STRING;
    }
    if (ZR_VALUE_IS_TYPE_ARRAY(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_ARRAY;
    }
    if (ZR_VALUE_IS_TYPE_OBJECT(valueType) || ZR_VALUE_IS_TYPE_THREAD(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_OBJECT;
    }
    if (ZR_VALUE_IS_TYPE_FUNCTION(valueType) || ZR_VALUE_IS_TYPE_CLOSURE(valueType) ||
        ZR_VALUE_IS_TYPE_CLOSURE_VALUE(valueType)) {
        return ZR_RUST_BINDING_VALUE_KIND_FUNCTION;
    }
    if (valueType == ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_RUST_BINDING_VALUE_KIND_NATIVE_POINTER;
    }
    return ZR_RUST_BINDING_VALUE_KIND_UNKNOWN;
}

ZrRustBindingOwnershipKind zr_rust_binding_map_ownership_kind(EZrOwnershipValueKind ownershipKind) {
    switch (ownershipKind) {
        case ZR_OWNERSHIP_VALUE_KIND_UNIQUE:
            return ZR_RUST_BINDING_OWNERSHIP_KIND_UNIQUE;
        case ZR_OWNERSHIP_VALUE_KIND_SHARED:
            return ZR_RUST_BINDING_OWNERSHIP_KIND_SHARED;
        case ZR_OWNERSHIP_VALUE_KIND_WEAK:
            return ZR_RUST_BINDING_OWNERSHIP_KIND_WEAK;
        case ZR_OWNERSHIP_VALUE_KIND_BORROWED:
            return ZR_RUST_BINDING_OWNERSHIP_KIND_BORROWED;
        case ZR_OWNERSHIP_VALUE_KIND_LOANED:
            return ZR_RUST_BINDING_OWNERSHIP_KIND_LOANED;
        case ZR_OWNERSHIP_VALUE_KIND_NONE:
        default:
            return ZR_RUST_BINDING_OWNERSHIP_KIND_NONE;
    }
}

ZrRustBindingExecutionOwner *zr_rust_binding_execution_owner_new(SZrGlobalState *global,
                                                                 const ZrRustBindingRuntime *runtime) {
    ZrRustBindingExecutionOwner *owner;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    owner = (ZrRustBindingExecutionOwner *)calloc(1, sizeof(*owner));
    if (owner == ZR_NULL) {
        return ZR_NULL;
    }

    owner->global = global;
    owner->ownsGlobal = ZR_TRUE;
    owner->refCount = 1U;
    if (!zr_rust_binding_runtime_capture_native_modules(runtime, &owner->nativeModules, &owner->nativeModuleCount)) {
        free(owner);
        return ZR_NULL;
    }
    return owner;
}

void zr_rust_binding_execution_owner_retain(ZrRustBindingExecutionOwner *owner) {
    if (owner != ZR_NULL) {
        owner->refCount++;
    }
}

void zr_rust_binding_execution_owner_release(ZrRustBindingExecutionOwner *owner) {
    if (owner == ZR_NULL) {
        return;
    }

    ZR_ASSERT(owner->refCount > 0U);
    owner->refCount--;
    if (owner->refCount != 0U) {
        return;
    }

    if (owner->ownsGlobal && owner->global != ZR_NULL) {
        ZrLibrary_CommonState_CommonGlobalState_Free(owner->global);
    }
    if (owner->nativeModules != ZR_NULL) {
        for (TZrSize index = 0; index < owner->nativeModuleCount; index++) {
            zr_rust_binding_native_module_release(owner->nativeModules[index]);
        }
        free(owner->nativeModules);
    }
    free(owner);
}

ZrRustBindingValue *zr_rust_binding_value_alloc(void) {
    return (ZrRustBindingValue *)calloc(1, sizeof(ZrRustBindingValue));
}

void zr_rust_binding_value_free_impl(ZrRustBindingValue *value) {
    TZrSize index;

    if (value == ZR_NULL) {
        return;
    }

    if (value->storageKind == ZR_RUST_BINDING_VALUE_STORAGE_VM) {
        ZrLib_TempValueRoot_End(&value->storage.vmValue.root);
        zr_rust_binding_execution_owner_release(value->storage.vmValue.owner);
    } else if (value->kind == ZR_RUST_BINDING_VALUE_KIND_STRING) {
        free(value->storage.stringValue);
    } else if (value->kind == ZR_RUST_BINDING_VALUE_KIND_ARRAY) {
        for (index = 0; index < value->storage.arrayValue.count; index++) {
            zr_rust_binding_value_free_impl(value->storage.arrayValue.items[index]);
        }
        free(value->storage.arrayValue.items);
    } else if (value->kind == ZR_RUST_BINDING_VALUE_KIND_OBJECT) {
        for (index = 0; index < value->storage.objectValue.count; index++) {
            free(value->storage.objectValue.fields[index].name);
            zr_rust_binding_value_free_impl(value->storage.objectValue.fields[index].value);
        }
        free(value->storage.objectValue.fields);
    }

    free(value);
}

static ZrRustBindingValue *zr_rust_binding_value_clone(const ZrRustBindingValue *value) {
    ZrRustBindingValue *clone;
    TZrSize index;

    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    clone = zr_rust_binding_value_alloc();
    if (clone == ZR_NULL) {
        return ZR_NULL;
    }

    clone->kind = value->kind;
    clone->ownershipKind = value->ownershipKind;
    clone->storageKind = value->storageKind;

    if (value->storageKind == ZR_RUST_BINDING_VALUE_STORAGE_VM) {
        const SZrTypeValue *liveValue = zr_rust_binding_live_value_ref(value);
        SZrState *state;

        if (liveValue == ZR_NULL || value->storage.vmValue.owner == ZR_NULL ||
            value->storage.vmValue.owner->global == ZR_NULL) {
            zr_rust_binding_value_free_impl(clone);
            return ZR_NULL;
        }

        state = value->storage.vmValue.owner->global->mainThreadState;
        zr_rust_binding_execution_owner_retain(value->storage.vmValue.owner);
        clone->storage.vmValue.owner = value->storage.vmValue.owner;
        if (!ZrLib_TempValueRoot_Begin(state, &clone->storage.vmValue.root) ||
            !ZrLib_TempValueRoot_SetValue(&clone->storage.vmValue.root, liveValue)) {
            zr_rust_binding_value_free_impl(clone);
            return ZR_NULL;
        }
        return clone;
    }

    switch (value->kind) {
        case ZR_RUST_BINDING_VALUE_KIND_BOOL:
            clone->storage.boolValue = value->storage.boolValue;
            break;
        case ZR_RUST_BINDING_VALUE_KIND_INT:
            clone->storage.intValue = value->storage.intValue;
            break;
        case ZR_RUST_BINDING_VALUE_KIND_FLOAT:
            clone->storage.floatValue = value->storage.floatValue;
            break;
        case ZR_RUST_BINDING_VALUE_KIND_STRING:
            clone->storage.stringValue = zr_rust_binding_strdup(value->storage.stringValue != ZR_NULL
                                                                        ? value->storage.stringValue
                                                                        : "");
            if (clone->storage.stringValue == ZR_NULL) {
                zr_rust_binding_value_free_impl(clone);
                return ZR_NULL;
            }
            break;
        case ZR_RUST_BINDING_VALUE_KIND_ARRAY:
            for (index = 0; index < value->storage.arrayValue.count; index++) {
                if (!zr_rust_binding_owned_array_push_clone(&clone->storage.arrayValue,
                                                            value->storage.arrayValue.items[index])) {
                    zr_rust_binding_value_free_impl(clone);
                    return ZR_NULL;
                }
            }
            break;
        case ZR_RUST_BINDING_VALUE_KIND_OBJECT:
            for (index = 0; index < value->storage.objectValue.count; index++) {
                if (!zr_rust_binding_owned_object_set_clone(&clone->storage.objectValue,
                                                            value->storage.objectValue.fields[index].name,
                                                            value->storage.objectValue.fields[index].value)) {
                    zr_rust_binding_value_free_impl(clone);
                    return ZR_NULL;
                }
            }
            break;
        default:
            break;
    }

    return clone;
}

ZrRustBindingValue *zr_rust_binding_value_new_live(ZrRustBindingExecutionOwner *owner, const SZrTypeValue *value) {
    ZrRustBindingValue *handle;
    SZrState *state;

    if (owner == ZR_NULL || value == ZR_NULL || owner->global == ZR_NULL || owner->global->mainThreadState == ZR_NULL) {
        return ZR_NULL;
    }

    handle = zr_rust_binding_value_alloc();
    if (handle == ZR_NULL) {
        return ZR_NULL;
    }

    state = owner->global->mainThreadState;
    zr_rust_binding_execution_owner_retain(owner);
    handle->kind = zr_rust_binding_map_value_kind(value->type);
    handle->ownershipKind = zr_rust_binding_map_ownership_kind(value->ownershipKind);
    handle->storageKind = ZR_RUST_BINDING_VALUE_STORAGE_VM;
    handle->storage.vmValue.owner = owner;
    if (!ZrLib_TempValueRoot_Begin(state, &handle->storage.vmValue.root) ||
        !ZrLib_TempValueRoot_SetValue(&handle->storage.vmValue.root, value)) {
        zr_rust_binding_value_free_impl(handle);
        return ZR_NULL;
    }
    return handle;
}

TZrBool zr_rust_binding_materialize_value(SZrState *state, const ZrRustBindingValue *value, SZrTypeValue *outValue) {
    const SZrTypeValue *liveValue;

    if (state == ZR_NULL || value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(outValue);
    if (value->storageKind == ZR_RUST_BINDING_VALUE_STORAGE_VM) {
        if (value->storage.vmValue.owner == ZR_NULL || value->storage.vmValue.owner->global != state->global) {
            return ZR_FALSE;
        }

        liveValue = zr_rust_binding_live_value_ref(value);
        if (liveValue == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Value_Copy(state, outValue, liveValue);
        return ZR_TRUE;
    }

    switch (value->kind) {
        case ZR_RUST_BINDING_VALUE_KIND_NULL:
            ZrLib_Value_SetNull(outValue);
            return ZR_TRUE;
        case ZR_RUST_BINDING_VALUE_KIND_BOOL:
            ZrLib_Value_SetBool(state, outValue, value->storage.boolValue);
            return ZR_TRUE;
        case ZR_RUST_BINDING_VALUE_KIND_INT:
            ZrLib_Value_SetInt(state, outValue, value->storage.intValue);
            return ZR_TRUE;
        case ZR_RUST_BINDING_VALUE_KIND_FLOAT:
            ZrLib_Value_SetFloat(state, outValue, value->storage.floatValue);
            return ZR_TRUE;
        case ZR_RUST_BINDING_VALUE_KIND_STRING:
            ZrLib_Value_SetString(state, outValue, value->storage.stringValue != ZR_NULL ? value->storage.stringValue : "");
            return ZR_TRUE;
        case ZR_RUST_BINDING_VALUE_KIND_ARRAY:
            return zr_rust_binding_materialize_owned_array(state, &value->storage.arrayValue, outValue);
        case ZR_RUST_BINDING_VALUE_KIND_OBJECT:
            return zr_rust_binding_materialize_owned_object(state, &value->storage.objectValue, outValue);
        default:
            return ZR_FALSE;
    }
}

static ZrRustBindingStatus zr_rust_binding_new_owned_value(ZrRustBindingValueKind kind, ZrRustBindingValue **outValue) {
    ZrRustBindingValue *value;

    if (outValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "outValue is null");
    }

    value = zr_rust_binding_value_alloc();
    if (value == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to allocate value");
    }

    value->kind = kind;
    value->ownershipKind = ZR_RUST_BINDING_OWNERSHIP_KIND_NONE;
    value->storageKind = ZR_RUST_BINDING_VALUE_STORAGE_OWNED;
    *outValue = value;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_NewNull(ZrRustBindingValue **outValue) {
    return zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_NULL, outValue);
}

ZrRustBindingStatus ZrRustBinding_Value_NewBool(TZrBool boolValue, ZrRustBindingValue **outValue) {
    ZrRustBindingStatus status = zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_BOOL, outValue);
    if (status == ZR_RUST_BINDING_STATUS_OK) {
        (*outValue)->storage.boolValue = boolValue;
    }
    return status;
}

ZrRustBindingStatus ZrRustBinding_Value_NewInt(TZrInt64 intValue, ZrRustBindingValue **outValue) {
    ZrRustBindingStatus status = zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_INT, outValue);
    if (status == ZR_RUST_BINDING_STATUS_OK) {
        (*outValue)->storage.intValue = intValue;
    }
    return status;
}

ZrRustBindingStatus ZrRustBinding_Value_NewFloat(TZrFloat64 floatValue, ZrRustBindingValue **outValue) {
    ZrRustBindingStatus status = zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_FLOAT, outValue);
    if (status == ZR_RUST_BINDING_STATUS_OK) {
        (*outValue)->storage.floatValue = floatValue;
    }
    return status;
}

ZrRustBindingStatus ZrRustBinding_Value_NewString(const TZrChar *stringValue, ZrRustBindingValue **outValue) {
    ZrRustBindingStatus status;

    if (stringValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "stringValue is null");
    }

    status = zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_STRING, outValue);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return status;
    }

    (*outValue)->storage.stringValue = zr_rust_binding_strdup(stringValue);
    if ((*outValue)->storage.stringValue == ZR_NULL) {
        zr_rust_binding_value_free_impl(*outValue);
        *outValue = ZR_NULL;
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to copy string value");
    }
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_NewArray(ZrRustBindingValue **outValue) {
    return zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_ARRAY, outValue);
}

ZrRustBindingStatus ZrRustBinding_Value_NewObject(ZrRustBindingValue **outValue) {
    return zr_rust_binding_new_owned_value(ZR_RUST_BINDING_VALUE_KIND_OBJECT, outValue);
}

ZrRustBindingStatus ZrRustBinding_Value_Free(ZrRustBindingValue *value) {
    zr_rust_binding_value_free_impl(value);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingValueKind ZrRustBinding_Value_GetKind(const ZrRustBindingValue *value) {
    return value != ZR_NULL ? value->kind : ZR_RUST_BINDING_VALUE_KIND_UNKNOWN;
}

ZrRustBindingOwnershipKind ZrRustBinding_Value_GetOwnershipKind(const ZrRustBindingValue *value) {
    return value != ZR_NULL ? value->ownershipKind : ZR_RUST_BINDING_OWNERSHIP_KIND_NONE;
}

ZrRustBindingStatus ZrRustBinding_Value_ReadBool(const ZrRustBindingValue *value, TZrBool *outBoolValue) {
    const SZrTypeValue *liveValue;

    if (value == ZR_NULL || outBoolValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or outBoolValue is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_BOOL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not bool");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    *outBoolValue = liveValue != ZR_NULL ? (TZrBool)liveValue->value.nativeObject.nativeBool : value->storage.boolValue;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_ReadInt(const ZrRustBindingValue *value, TZrInt64 *outIntValue) {
    const SZrTypeValue *liveValue;

    if (value == ZR_NULL || outIntValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or outIntValue is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_INT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not int");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    if (liveValue != ZR_NULL) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(liveValue->type)) {
            *outIntValue = liveValue->value.nativeObject.nativeInt64;
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(liveValue->type)) {
            *outIntValue = (TZrInt64)liveValue->value.nativeObject.nativeUInt64;
        } else {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "vm value is not int");
        }
    } else {
        *outIntValue = value->storage.intValue;
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_ReadFloat(const ZrRustBindingValue *value, TZrFloat64 *outFloatValue) {
    const SZrTypeValue *liveValue;

    if (value == ZR_NULL || outFloatValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or outFloatValue is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_FLOAT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not float");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    *outFloatValue = liveValue != ZR_NULL ? liveValue->value.nativeObject.nativeDouble : value->storage.floatValue;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_ReadString(const ZrRustBindingValue *value,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    const SZrTypeValue *liveValue;
    const TZrChar *source;

    if (value == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or output buffer is invalid");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_STRING) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not string");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    source = liveValue != ZR_NULL
                     ? ZrCore_String_GetNativeString((SZrString *)liveValue->value.object)
                     : (value->storage.stringValue != ZR_NULL ? value->storage.stringValue : "");
    if (!zr_rust_binding_copy_string_to_buffer(source, buffer, bufferSize)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL, "output buffer is too small");
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_Array_Length(const ZrRustBindingValue *value, TZrSize *outLength) {
    const SZrTypeValue *liveValue;

    if (value == ZR_NULL || outLength == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or outLength is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_ARRAY) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not array");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    *outLength = liveValue != ZR_NULL ? ZrLib_Array_Length((SZrObject *)liveValue->value.object) : value->storage.arrayValue.count;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_Array_Get(const ZrRustBindingValue *value,
                                                  TZrSize index,
                                                  ZrRustBindingValue **outElement) {
    const SZrTypeValue *liveValue;
    const SZrTypeValue *elementValue;

    if (value == ZR_NULL || outElement == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or outElement is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_ARRAY) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not array");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    if (liveValue != ZR_NULL) {
        elementValue = ZrLib_Array_Get(value->storage.vmValue.owner->global->mainThreadState,
                                       (SZrObject *)liveValue->value.object,
                                       index);
        if (elementValue == ZR_NULL) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "array index out of range");
        }

        *outElement = zr_rust_binding_value_new_live(value->storage.vmValue.owner, elementValue);
        if (*outElement == ZR_NULL) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to create array element handle");
        }
    } else {
        if (index >= value->storage.arrayValue.count) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "array index out of range");
        }

        *outElement = zr_rust_binding_value_clone(value->storage.arrayValue.items[index]);
        if (*outElement == ZR_NULL) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to clone array element");
        }
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_Array_Push(ZrRustBindingValue *value, const ZrRustBindingValue *element) {
    if (value == ZR_NULL || element == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value or element is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_ARRAY) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not array");
    }

    if (value->storageKind == ZR_RUST_BINDING_VALUE_STORAGE_VM) {
        ZrLibTempValueRoot elementRoot;
        SZrTypeValue *elementValue;
        SZrState *state = value->storage.vmValue.owner->global->mainThreadState;
        const SZrTypeValue *liveValue = zr_rust_binding_live_value_ref(value);

        if (liveValue == ZR_NULL || !ZrLib_TempValueRoot_Begin(state, &elementRoot)) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to root array element");
        }

        elementValue = ZrLib_TempValueRoot_Value(&elementRoot);
        if (elementValue == ZR_NULL ||
            !zr_rust_binding_materialize_value(state, element, elementValue) ||
            !ZrLib_Array_PushValue(state, (SZrObject *)liveValue->value.object, elementValue)) {
            ZrLib_TempValueRoot_End(&elementRoot);
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR, "failed to push array element");
        }

        ZrLib_TempValueRoot_End(&elementRoot);
    } else if (!zr_rust_binding_owned_array_push_clone(&value->storage.arrayValue, element)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to push owned array element");
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_Object_Get(const ZrRustBindingValue *value,
                                                   const TZrChar *fieldName,
                                                   ZrRustBindingValue **outFieldValue) {
    const SZrTypeValue *liveValue;
    const SZrTypeValue *fieldValue;
    TZrSize index;

    if (value == ZR_NULL || fieldName == ZR_NULL || outFieldValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "value, fieldName, or outFieldValue is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_OBJECT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not object");
    }

    liveValue = zr_rust_binding_live_value_ref(value);
    if (liveValue != ZR_NULL) {
        fieldValue = ZrLib_Object_GetFieldCString(value->storage.vmValue.owner->global->mainThreadState,
                                                  (SZrObject *)liveValue->value.object,
                                                  fieldName);
        if (fieldValue == ZR_NULL) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "object field not found: %s", fieldName);
        }

        *outFieldValue = zr_rust_binding_value_new_live(value->storage.vmValue.owner, fieldValue);
        if (*outFieldValue == ZR_NULL) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to create object field handle");
        }
    } else {
        index = zr_rust_binding_owned_object_find_field(&value->storage.objectValue, fieldName);
        if (index >= value->storage.objectValue.count) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND, "object field not found: %s", fieldName);
        }

        *outFieldValue = zr_rust_binding_value_clone(value->storage.objectValue.fields[index].value);
        if (*outFieldValue == ZR_NULL) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to clone object field");
        }
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Value_Object_Set(ZrRustBindingValue *value,
                                                   const TZrChar *fieldName,
                                                   const ZrRustBindingValue *fieldValue) {
    if (value == ZR_NULL || fieldName == ZR_NULL || fieldValue == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "value, fieldName, or fieldValue is null");
    }
    if (value->kind != ZR_RUST_BINDING_VALUE_KIND_OBJECT) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "value is not object");
    }

    if (value->storageKind == ZR_RUST_BINDING_VALUE_STORAGE_VM) {
        ZrLibTempValueRoot fieldRoot;
        SZrTypeValue *materializedField;
        SZrState *state = value->storage.vmValue.owner->global->mainThreadState;
        const SZrTypeValue *liveValue = zr_rust_binding_live_value_ref(value);

        if (liveValue == ZR_NULL || !ZrLib_TempValueRoot_Begin(state, &fieldRoot)) {
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to root object field");
        }

        materializedField = ZrLib_TempValueRoot_Value(&fieldRoot);
        if (materializedField == ZR_NULL || !zr_rust_binding_materialize_value(state, fieldValue, materializedField)) {
            ZrLib_TempValueRoot_End(&fieldRoot);
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR, "failed to materialize object field");
        }

        ZrLib_Object_SetFieldCString(state, (SZrObject *)liveValue->value.object, fieldName, materializedField);
        ZrLib_TempValueRoot_End(&fieldRoot);
    } else if (!zr_rust_binding_owned_object_set_clone(&value->storage.objectValue, fieldName, fieldValue)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to set owned object field");
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}
