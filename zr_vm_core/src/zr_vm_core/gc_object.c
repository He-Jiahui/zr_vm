//
// Object lifecycle and raw-object helpers for the GC.
//

#include "gc_internal.h"

TZrBool garbage_collector_ignore_registry_contains(SZrGarbageCollector *collector, SZrRawObject *object) {
    if (collector == ZR_NULL || object == ZR_NULL || collector->ignoredObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < collector->ignoredObjectCount; i++) {
        if (collector->ignoredObjects[i] == object) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool garbage_collector_ensure_ignore_registry_capacity(SZrGlobalState *global, TZrSize minCapacity) {
    SZrGarbageCollector *collector;
    SZrRawObject **newItems;
    TZrSize newCapacity;
    TZrSize oldBytes;
    TZrSize newBytes;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    if (collector->ignoredObjectCapacity >= minCapacity) {
        return ZR_TRUE;
    }

    newCapacity = collector->ignoredObjectCapacity > 0 ? collector->ignoredObjectCapacity : 8;
    while (newCapacity < minCapacity) {
        newCapacity *= 2;
    }

    newBytes = newCapacity * sizeof(SZrRawObject *);
    newItems = (SZrRawObject **)ZrCore_Memory_RawMallocWithType(global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newItems == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newItems, 0, newBytes);
    if (collector->ignoredObjects != ZR_NULL && collector->ignoredObjectCount > 0) {
        memcpy(newItems,
               collector->ignoredObjects,
               collector->ignoredObjectCount * sizeof(SZrRawObject *));
    }

    if (collector->ignoredObjects != ZR_NULL) {
        oldBytes = collector->ignoredObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(global,
                                      collector->ignoredObjects,
                                      oldBytes,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }

    collector->ignoredObjects = newItems;
    collector->ignoredObjectCapacity = newCapacity;
    return ZR_TRUE;
}

TZrSize garbage_collector_get_object_base_size(SZrState *state, SZrRawObject *object) {
    if (object == ZR_NULL) {
        return 0;
    }

    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return 0;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return 0;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
            return sizeof(SZrObject);
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
            return sizeof(SZrFunction);
        case ZR_RAW_OBJECT_TYPE_STRING: {
            if (state != ZR_NULL) {
                SZrString *str = ZR_CAST_STRING(state, object);
                if (str != ZR_NULL) {
                    if (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                        return sizeof(SZrString) + (TZrSize)str->shortStringLength + 1;
                    }
                    return sizeof(SZrString);
                }
            }
            return sizeof(SZrString);
        }
        case ZR_RAW_OBJECT_TYPE_BUFFER:
            return sizeof(SZrArray);
        case ZR_RAW_OBJECT_TYPE_ARRAY:
            // Runtime arrays are object-backed (SZrObject + nodeMap), not standalone SZrArray headers.
            return sizeof(SZrObject);
        case ZR_RAW_OBJECT_TYPE_CLOSURE: {
            if (state != ZR_NULL) {
                if (object->isNative) {
                    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
                    if (closure != ZR_NULL) {
                        TZrSize extraCount = closure->closureValueCount > 1 ? closure->closureValueCount - 1 : 0;
                        return sizeof(SZrClosureNative) + extraCount * sizeof(SZrTypeValue *);
                    }
                } else {
                    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
                    if (closure != ZR_NULL) {
                        TZrSize extraCount = closure->closureValueCount > 1 ? closure->closureValueCount - 1 : 0;
                        return sizeof(SZrClosure) + extraCount * sizeof(SZrClosureValue *);
                    }
                }
            }
            return object->isNative ? sizeof(SZrClosureNative) : sizeof(SZrClosure);
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
            return sizeof(SZrClosureValue);
        case ZR_RAW_OBJECT_TYPE_THREAD:
            return sizeof(SZrState);
        case ZR_RAW_OBJECT_TYPE_NATIVE_POINTER:
            return sizeof(SZrRawObject);
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            if (state != ZR_NULL) {
                struct SZrNativeData *nativeData =
                        ZR_CAST_CHECKED(state, struct SZrNativeData *, object, ZR_RAW_OBJECT_TYPE_NATIVE_DATA);
                if (nativeData != ZR_NULL) {
                    TZrSize extraCount = nativeData->valueLength > 1 ? nativeData->valueLength - 1 : 0;
                    return sizeof(struct SZrNativeData) + extraCount * sizeof(SZrTypeValue);
                }
            }
            return sizeof(struct SZrNativeData);
        }
        default:
            return sizeof(SZrRawObject);
    }
}

void garbage_collector_free_object(SZrState *state, SZrRawObject *object) {
    EZrGarbageCollectIncrementalObjectStatus oldStatus;
    SZrGlobalState *global;
    TZrSize objectSize;

    if (object == ZR_NULL || state == ZR_NULL) {
        return;
    }

    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return;
    }

    global = state->global;
    if (global == ZR_NULL) {
        return;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return;
    }

    oldStatus = object->garbageCollectMark.status;
    if (oldStatus == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED) {
        return;
    }

    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
    objectSize = garbage_collector_get_object_base_size(state, object);

    ZrCore_Ownership_NotifyObjectReleased(state, object);

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            SZrObject *obj = ZR_CAST_OBJECT(state, object);
            if (obj != ZR_NULL) {
                /* Array payload is currently owned elsewhere. */
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
        case ZR_RAW_OBJECT_TYPE_OBJECT:
            break;
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA:
            if (object->scanMarkGcFunction != ZR_NULL) {
                object->scanMarkGcFunction(state, object);
            }
            break;
        default:
            break;
    }

    ZrCore_Memory_RawFreeWithType(global, object, objectSize, ZR_MEMORY_NATIVE_TYPE_OBJECT);
}

SZrRawObject *ZrCore_RawObject_New(SZrState *state, EZrValueType type, TZrSize size, TZrBool isNative) {
    SZrGlobalState *global = state->global;
    TZrPtr memory = ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_OBJECT, size);
    SZrRawObject *object = ZR_CAST_RAW_OBJECT(memory);

    ZrCore_RawObject_Construct(object, (EZrRawObjectType)type);
    object->isNative = isNative;
    object->garbageCollectMark.status = global->garbageCollector->gcInitializeObjectStatus;
    object->garbageCollectMark.generation = global->garbageCollector->gcGeneration;
    object->next = global->garbageCollector->gcObjectList;
    global->garbageCollector->gcObjectList = object;
    return object;
}

TZrBool ZrCore_RawObject_IsUnreferenced(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global;
    EZrGarbageCollectIncrementalObjectStatus status;
    EZrGarbageCollectGeneration generation;

    if (object == ZR_NULL || state == ZR_NULL) {
        return ZR_TRUE;
    }

    if ((TZrPtr)object < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return ZR_TRUE;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_TRUE;
    }

    if (garbage_collector_ignore_registry_contains(global->garbageCollector, object)) {
        return ZR_FALSE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return ZR_TRUE;
    }

    status = object->garbageCollectMark.status;
    generation = object->garbageCollectMark.generation;
    return status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED ||
           status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED ||
           (status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED &&
            generation != global->garbageCollector->gcGeneration);
}

void ZrCore_RawObject_MarkAsInit(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;
    EZrGarbageCollectGeneration generation = global->garbageCollector->gcGeneration;

    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    object->garbageCollectMark.generation = generation;
}

void ZrCore_RawObject_MarkAsPermanent(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;

    if (object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
        return;
    }

    ZR_ASSERT(object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED);

    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
    object->next = global->garbageCollector->permanentObjectList;
    global->garbageCollector->permanentObjectList = object;
}

void ZrCore_Gc_ValueStaticAssertIsAlive(SZrState *state, SZrTypeValue *value) {
    ZR_ASSERT(!value->isGarbageCollectable ||
              ((value->type == (EZrValueType)value->value.object->type) &&
               ((state == ZR_NULL) || !ZrCore_Gc_RawObjectIsDead(state->global, value->value.object))));
}
