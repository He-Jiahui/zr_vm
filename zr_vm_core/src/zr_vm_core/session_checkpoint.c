#include "zr_vm_core/session_checkpoint.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct ZrCheckpointValue {
    SZrTypeValue scalar;
    TZrSize objectIndex;
    TZrBool isObject;
} ZrCheckpointValue;

typedef struct ZrCheckpointPair {
    ZrCheckpointValue key;
    ZrCheckpointValue value;
} ZrCheckpointPair;

typedef struct ZrCheckpointObject {
    SZrGcRootHandle handle;
    EZrRawObjectType type;
    EZrObjectInternalType internalType;
    TZrSize pairCount;
    ZrCheckpointPair *pairs;
    TZrSize proPairCount;
    ZrCheckpointPair *proPairs;
    TZrSize prototypeIndex;
    TZrBool hasPrototype;
    TZrSize functionIndex;
    TZrBool hasFunction;
    TZrSize *captureIndexes;
    TZrSize captureCount;
    TZrSize closureValueIndex;
    TZrBool hasClosureValue;
    ZrCheckpointValue closureValue;
    TZrUInt32 memberVersion;
    TZrUInt8 moduleInitState;
    TZrUInt32 moduleMetadataGeneration;
    TZrUInt32 *descriptorReady;
    TZrSize descriptorCount;
    TZrInt64 *rawIntData;
    TZrSize rawIntLength;
    TZrSize rawIntCapacity;
    EZrSuperArrayStorageMode rawIntStorageMode;
    TZrUInt64 rawIntStorageGeneration;
} ZrCheckpointObject;

struct SZrSessionCheckpoint {
    SZrState *state;
    ZrCheckpointObject *objects;
    TZrSize objectCount;
    TZrSize objectCapacity;
    ZrCheckpointValue loadedModulesRegistry;
    ZrCheckpointValue zrObject;
    ZrCheckpointValue nullValue;
    ZrCheckpointValue unhandledExceptionHandler;
    TZrBool hasUnhandledExceptionHandler;
    TZrSize errorPrototypeIndex;
    TZrBool hasErrorPrototype;
    TZrSize stackFramePrototypeIndex;
    TZrBool hasStackFramePrototype;
    TZrSize basicTypePrototypeIndexes[ZR_VALUE_TYPE_ENUM_MAX];
    TZrBool hasBasicTypePrototype[ZR_VALUE_TYPE_ENUM_MAX];
    TZrSize metaFunctionNameIndexes[ZR_META_ENUM_MAX];
    TZrBool hasMetaFunctionName[ZR_META_ENUM_MAX];
};

#define ZR_CHECKPOINT_NO_INDEX ((TZrSize)-1)

static TZrBool checkpoint_grow(void **memory, TZrSize *capacity, TZrSize elementSize, TZrSize required) {
    TZrSize next;
    void *grown;

    if (required <= *capacity) {
        return ZR_TRUE;
    }
    next = *capacity == 0u ? 8u : *capacity;
    while (next < required) {
        if (next > (TZrSize)-1 / 2u) {
            return ZR_FALSE;
        }
        next *= 2u;
    }
    if (elementSize != 0u && next > (TZrSize)-1 / elementSize) {
        return ZR_FALSE;
    }
    grown = realloc(*memory, next * elementSize);
    if (grown == ZR_NULL) {
        return ZR_FALSE;
    }
    *memory = grown;
    *capacity = next;
    return ZR_TRUE;
}

static TZrBool checkpoint_value_is_recoverable(const SZrTypeValue *value) {
    if (value == ZR_NULL || value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        value->ownershipControl != ZR_NULL || value->ownershipWeakRef != ZR_NULL) {
        return ZR_FALSE;
    }
    if (value->type == ZR_VALUE_TYPE_NATIVE_POINTER ||
        value->type == ZR_VALUE_TYPE_NATIVE_DATA ||
        value->type == ZR_VALUE_TYPE_VM_MEMORY ||
        value->type == ZR_VALUE_TYPE_UNKNOWN || value->type >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_FALSE;
    }
    if (!value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return ZR_TRUE;
    }
    return value->type != ZR_VALUE_TYPE_THREAD;
}

static TZrBool checkpoint_object_is_recoverable(const SZrRawObject *raw) {
    if (raw == ZR_NULL || raw->resourceLifecycleState != ZR_RESOURCE_LIFECYCLE_NONE) {
        return ZR_FALSE;
    }
    switch (raw->type) {
        case ZR_RAW_OBJECT_TYPE_STRING:
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
            return raw->finalizerData == ZR_NULL;
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            const SZrObject *object = (const SZrObject *)raw;
            if (raw->isNative || object->inlineArrayLength != 0u ||
                object->internalType >= ZR_OBJECT_INTERNAL_TYPE_CUSTOM_EXTENSION_START) {
                return ZR_FALSE;
            }
            return raw->finalizerData == ZR_NULL;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
            return !raw->isNative && raw->finalizerData == ZR_NULL;
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
            if (!raw->isNative) {
                return raw->finalizerData == ZR_NULL;
            }
            return ((const SZrClosureNative *)raw)->closureValueCount == 0u &&
                   raw->finalizerData == ZR_NULL;
        default:
            return ZR_FALSE;
    }
}

static TZrSize checkpoint_find_object(const SZrSessionCheckpoint *checkpoint, const SZrRawObject *raw) {
    TZrSize index;
    SZrRawObject *resolved;

    if (checkpoint == ZR_NULL || raw == ZR_NULL) {
        return ZR_CHECKPOINT_NO_INDEX;
    }
    for (index = 0u; index < checkpoint->objectCount; index++) {
        resolved = ZR_NULL;
        if (ZrCore_GcRootHandle_Resolve(checkpoint->state,
                                        &checkpoint->objects[index].handle,
                                        &resolved) &&
            resolved == raw) {
            return index;
        }
    }
    return ZR_CHECKPOINT_NO_INDEX;
}

static TZrBool checkpoint_capture_object(SZrSessionCheckpoint *checkpoint,
                                          SZrRawObject *raw,
                                          TZrSize *outIndex);
static TZrBool checkpoint_capture_value(SZrSessionCheckpoint *checkpoint,
                                         const SZrTypeValue *value,
                                         ZrCheckpointValue *outValue);

static TZrBool checkpoint_capture_function_refs(SZrSessionCheckpoint *checkpoint,
                                                 SZrFunction *function) {
    TZrSize ignored;
    TZrUInt32 index;

    if (function->ownerFunction != ZR_NULL &&
        !checkpoint_capture_object(checkpoint,
                                   ZR_CAST_RAW_OBJECT_AS_SUPER(function->ownerFunction),
                                   &ignored)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->constantValueLength; index++) {
        ZrCheckpointValue captured;
        if (!checkpoint_capture_value(checkpoint, &function->constantValueList[index], &captured)) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->childFunctionLength; index++) {
        if (!checkpoint_capture_object(checkpoint,
                                       ZR_CAST_RAW_OBJECT_AS_SUPER(&function->childFunctionList[index]),
                                       &ignored)) {
            return ZR_FALSE;
        }
    }
    if (function->cachedStatelessClosure != ZR_NULL &&
        !checkpoint_capture_object(checkpoint,
                                   ZR_CAST_RAW_OBJECT_AS_SUPER(function->cachedStatelessClosure),
                                   &ignored)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->prototypeInstancesLength; index++) {
        if (function->prototypeInstances[index] != ZR_NULL &&
            !checkpoint_capture_object(checkpoint,
                ZR_CAST_RAW_OBJECT_AS_SUPER(function->prototypeInstances[index]),
                &ignored)) {
            return ZR_FALSE;
        }
    }
    if (function->hasDecoratorMetadata) {
        ZrCheckpointValue captured;
        if (!checkpoint_capture_value(checkpoint, &function->decoratorMetadataValue, &captured)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool checkpoint_capture_value(SZrSessionCheckpoint *checkpoint,
                                         const SZrTypeValue *value,
                                         ZrCheckpointValue *outValue) {
    TZrSize objectIndex;

    if (checkpoint == ZR_NULL || value == ZR_NULL || outValue == ZR_NULL ||
        !checkpoint_value_is_recoverable(value)) {
        return ZR_FALSE;
    }
    memset(outValue, 0, sizeof(*outValue));
    outValue->scalar = *value;
    outValue->objectIndex = ZR_CHECKPOINT_NO_INDEX;
    if (!value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return ZR_TRUE;
    }
    objectIndex = ZR_CHECKPOINT_NO_INDEX;
    if (!checkpoint_capture_object(checkpoint, value->value.object, &objectIndex)) {
        return ZR_FALSE;
    }
    outValue->isObject = ZR_TRUE;
    outValue->objectIndex = objectIndex;
    outValue->scalar.value.object = ZR_NULL;
    return ZR_TRUE;
}

static TZrBool checkpoint_capture_pairs(SZrSessionCheckpoint *checkpoint,
                                        SZrHashSet *set,
                                        ZrCheckpointPair **outPairs,
                                        TZrSize *outCount) {
    TZrSize bucketIndex;
    TZrSize count = 0u;
    ZrCheckpointPair *pairs = ZR_NULL;

    if (set == ZR_NULL || !set->isValid) {
        *outPairs = ZR_NULL;
        *outCount = 0u;
        return ZR_TRUE;
    }
    if (set->elementCount > 0u) {
        pairs = (ZrCheckpointPair *)calloc(set->elementCount, sizeof(*pairs));
        if (pairs == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    for (bucketIndex = 0u; bucketIndex < set->capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (count >= set->elementCount ||
                !checkpoint_capture_value(checkpoint, &pair->key, &pairs[count].key) ||
                !checkpoint_capture_value(checkpoint, &pair->value, &pairs[count].value)) {
                free(pairs);
                return ZR_FALSE;
            }
            count++;
            pair = pair->next;
        }
    }
    *outPairs = pairs;
    *outCount = count;
    return ZR_TRUE;
}

static TZrBool checkpoint_capture_object(SZrSessionCheckpoint *checkpoint,
                                          SZrRawObject *raw,
                                          TZrSize *outIndex) {
    ZrCheckpointObject *snapshot;
    SZrObject *object;
    TZrSize index;
    TZrSize capacity;

    if (checkpoint == ZR_NULL || raw == ZR_NULL || outIndex == ZR_NULL ||
        !checkpoint_object_is_recoverable(raw)) {
        return ZR_FALSE;
    }
    index = checkpoint_find_object(checkpoint, raw);
    if (index != ZR_CHECKPOINT_NO_INDEX) {
        *outIndex = index;
        return ZR_TRUE;
    }
    if (!checkpoint_grow((void **)&checkpoint->objects,
                         &checkpoint->objectCapacity,
                         sizeof(*checkpoint->objects),
                         checkpoint->objectCount + 1u)) {
        return ZR_FALSE;
    }
    index = checkpoint->objectCount++;
    snapshot = &checkpoint->objects[index];
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->prototypeIndex = ZR_CHECKPOINT_NO_INDEX;
    snapshot->functionIndex = ZR_CHECKPOINT_NO_INDEX;
    snapshot->closureValueIndex = ZR_CHECKPOINT_NO_INDEX;
    if (!ZrCore_GcRootHandle_Create(checkpoint->state, raw, &snapshot->handle)) {
        return ZR_FALSE;
    }
    snapshot->type = raw->type;
    if (raw->type == ZR_RAW_OBJECT_TYPE_OBJECT || raw->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
        ZrCheckpointPair *capturedPairs = ZR_NULL;
        TZrSize capturedPairCount = 0u;
        object = (SZrObject *)raw;
        checkpoint->objects[index].internalType = object->internalType;
        checkpoint->objects[index].memberVersion = object->memberVersion;
        if (object->prototype != ZR_NULL &&
            !checkpoint_capture_object(checkpoint,
                                        ZR_CAST_RAW_OBJECT_AS_SUPER(object->prototype),
                                        &checkpoint->objects[index].prototypeIndex)) {
            return ZR_FALSE;
        }
        checkpoint->objects[index].hasPrototype = object->prototype != ZR_NULL;
        if (!checkpoint_capture_pairs(checkpoint, &object->nodeMap,
                                      &capturedPairs, &capturedPairCount)) {
            return ZR_FALSE;
        }
        checkpoint->objects[index].pairs = capturedPairs;
        checkpoint->objects[index].pairCount = capturedPairCount;
        if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
            SZrObjectModule *module = (SZrObjectModule *)object;
            capturedPairs = ZR_NULL;
            capturedPairCount = 0u;
            checkpoint->objects[index].moduleInitState = module->initState;
            checkpoint->objects[index].moduleMetadataGeneration = module->metadataGeneration;
            if (!checkpoint_capture_pairs(checkpoint, &module->proNodeMap,
                                          &capturedPairs, &capturedPairCount)) {
                return ZR_FALSE;
            }
            checkpoint->objects[index].proPairs = capturedPairs;
            checkpoint->objects[index].proPairCount = capturedPairCount;
            snapshot = &checkpoint->objects[index];
            snapshot->descriptorCount = module->exportDescriptorLength;
            if (snapshot->descriptorCount > 0u) {
                snapshot->descriptorReady = (TZrUInt32 *)calloc(snapshot->descriptorCount,
                                                                  sizeof(*snapshot->descriptorReady));
                if (snapshot->descriptorReady == ZR_NULL) {
                    return ZR_FALSE;
                }
                for (capacity = 0u; capacity < snapshot->descriptorCount; capacity++) {
                    snapshot->descriptorReady[capacity] = module->exportDescriptors[capacity].isReady;
                }
            }
        }
        if (object->superArrayRawIntData != ZR_NULL) {
            snapshot = &checkpoint->objects[index];
            snapshot->rawIntLength = object->superArrayRawIntLength;
            snapshot->rawIntCapacity = object->superArrayRawIntCapacity;
            if (snapshot->rawIntCapacity > 0u) {
                snapshot->rawIntData = (TZrInt64 *)calloc(snapshot->rawIntCapacity, sizeof(TZrInt64));
                if (snapshot->rawIntData == ZR_NULL) {
                    return ZR_FALSE;
                }
                memcpy(snapshot->rawIntData, object->superArrayRawIntData,
                       snapshot->rawIntLength * sizeof(TZrInt64));
            }
        }
        checkpoint->objects[index].rawIntStorageMode = object->superArrayStorageMode;
        checkpoint->objects[index].rawIntStorageGeneration = object->superArrayStorageGeneration;
    } else if (raw->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        if (!checkpoint_capture_function_refs(checkpoint, (SZrFunction *)raw)) {
            return ZR_FALSE;
        }
    } else if (raw->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        if (!raw->isNative) {
            SZrClosure *closure = (SZrClosure *)raw;
            TZrSize captureCount = closure->closureValueCount;
            checkpoint->objects[index].captureCount = captureCount;
            snapshot = &checkpoint->objects[index];
            if (captureCount > 0u) {
                snapshot->captureIndexes = (TZrSize *)malloc(captureCount * sizeof(TZrSize));
                if (snapshot->captureIndexes == ZR_NULL) {
                    return ZR_FALSE;
                }
                for (capacity = 0u; capacity < captureCount; capacity++) {
                    if (closure->closureValuesExtend[capacity] == ZR_NULL ||
                        !checkpoint_capture_object(checkpoint,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[capacity]),
                            &checkpoint->objects[index].captureIndexes[capacity])) {
                        return ZR_FALSE;
                    }
                }
            }
            if (closure->function != ZR_NULL &&
                !checkpoint_capture_object(checkpoint,
                                           ZR_CAST_RAW_OBJECT_AS_SUPER(closure->function),
                                           &checkpoint->objects[index].functionIndex)) {
                return ZR_FALSE;
            }
            checkpoint->objects[index].hasFunction = closure->function != ZR_NULL;
        }
    } else if (raw->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
        SZrClosureValue *closureValue = (SZrClosureValue *)raw;
        SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
        if (!ZrCore_ClosureValue_IsClosed(closureValue) || value == ZR_NULL ||
            !checkpoint_capture_value(checkpoint, value,
                                      &checkpoint->objects[index].closureValue)) {
            return ZR_FALSE;
        }
        checkpoint->objects[index].hasClosureValue = ZR_TRUE;
    }
    *outIndex = index;
    return ZR_TRUE;
}

static TZrBool checkpoint_restore_value(SZrState *state,
                                        const SZrSessionCheckpoint *checkpoint,
                                        const ZrCheckpointValue *source,
                                        SZrTypeValue *outValue) {
    SZrRawObject *object;

    if (source == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    *outValue = source->scalar;
    if (!source->isObject) {
        return ZR_TRUE;
    }
    if (source->objectIndex >= checkpoint->objectCount ||
        !ZrCore_GcRootHandle_Resolve(state,
                                     &checkpoint->objects[source->objectIndex].handle,
                                     &object)) {
        return ZR_FALSE;
    }
    outValue->value.object = object;
    return ZR_TRUE;
}

static TZrBool checkpoint_restore_map(SZrState *state,
                                             const SZrSessionCheckpoint *checkpoint,
                                             SZrRawObject *owner,
                                             SZrHashSet *set,
                                             const ZrCheckpointPair *pairs,
                                             TZrSize pairCount) {
    TZrSize index;
    TZrSize capacityLog2 = 4u;
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;
    SZrTypeValue value;

    if (set->isValid) {
        ZrCore_HashSet_Deconstruct(state, set);
    }
    while (capacityLog2 < sizeof(TZrSize) * CHAR_BIT - 1u &&
           ((TZrSize)1u << capacityLog2) < pairCount + 1u) {
        capacityLog2++;
    }
    ZrCore_HashSet_Construct(set);
    ZrCore_HashSet_Init(state, set, capacityLog2);
    if (!set->isValid || !ZrCore_HashSet_EnsurePairPoolForElementCount(state, set, pairCount)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < pairCount; index++) {
        if (!checkpoint_restore_value(state, checkpoint, &pairs[index].key, &key) ||
            !checkpoint_restore_value(state, checkpoint, &pairs[index].value, &value)) {
            return ZR_FALSE;
        }
        pair = ZrCore_HashSet_Add(state, set, &key);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        pair->value = value;
        ZrCore_Value_Barrier(state, owner, &pair->key);
        ZrCore_Value_Barrier(state, owner, &pair->value);
    }
    return ZR_TRUE;
}

static TZrBool checkpoint_restore(SZrState *state, const SZrSessionCheckpoint *checkpoint) {
    TZrSize index;
    SZrRawObject *raw;
    SZrTypeValue value;

    if (state == ZR_NULL || checkpoint == ZR_NULL || checkpoint->state != state) {
        return ZR_FALSE;
    }
    if (!checkpoint_restore_value(state, checkpoint, &checkpoint->loadedModulesRegistry, &value)) {
        return ZR_FALSE;
    }
    state->global->loadedModulesRegistry = value;
    if (!checkpoint_restore_value(state, checkpoint, &checkpoint->nullValue, &value)) {
        return ZR_FALSE;
    }
    state->global->nullValue = value;
    if (!checkpoint_restore_value(state, checkpoint, &checkpoint->zrObject, &value)) {
        return ZR_FALSE;
    }
    state->global->zrObject = value;
    state->global->hasUnhandledExceptionHandler = checkpoint->hasUnhandledExceptionHandler;
    if (checkpoint->hasUnhandledExceptionHandler) {
        if (!checkpoint_restore_value(state, checkpoint, &checkpoint->unhandledExceptionHandler, &value)) {
            return ZR_FALSE;
        }
        state->global->unhandledExceptionHandler = value;
    } else {
        ZrCore_Value_ResetAsNull(&state->global->unhandledExceptionHandler);
    }
    state->global->errorPrototype = ZR_NULL;
    if (checkpoint->hasErrorPrototype &&
        !ZrCore_GcRootHandle_Resolve(state,
            &checkpoint->objects[checkpoint->errorPrototypeIndex].handle,
            (SZrRawObject **)&state->global->errorPrototype)) {
        return ZR_FALSE;
    }
    state->global->stackFramePrototype = ZR_NULL;
    if (checkpoint->hasStackFramePrototype &&
        !ZrCore_GcRootHandle_Resolve(state,
            &checkpoint->objects[checkpoint->stackFramePrototypeIndex].handle,
            (SZrRawObject **)&state->global->stackFramePrototype)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < ZR_VALUE_TYPE_ENUM_MAX; index++) {
        state->global->basicTypeObjectPrototype[index] = ZR_NULL;
        if (checkpoint->hasBasicTypePrototype[index] &&
            !ZrCore_GcRootHandle_Resolve(state,
                &checkpoint->objects[checkpoint->basicTypePrototypeIndexes[index]].handle,
                (SZrRawObject **)&state->global->basicTypeObjectPrototype[index])) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < ZR_META_ENUM_MAX; index++) {
        state->global->metaFunctionName[index] = ZR_NULL;
        if (checkpoint->hasMetaFunctionName[index] &&
            !ZrCore_GcRootHandle_Resolve(state,
                &checkpoint->objects[checkpoint->metaFunctionNameIndexes[index]].handle,
                (SZrRawObject **)&state->global->metaFunctionName[index])) {
            return ZR_FALSE;
        }
    }
    memset(state->global->stringHashApiCache, 0, sizeof(state->global->stringHashApiCache));
    memset(state->global->stringConcatPairCache, 0, sizeof(state->global->stringConcatPairCache));
    for (index = 0u; index < checkpoint->objectCount; index++) {
        ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        if (!ZrCore_GcRootHandle_Resolve(state, &snapshot->handle, &raw)) {
            return ZR_FALSE;
        }
        if (snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT || snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            SZrObject *object = (SZrObject *)raw;
            if (snapshot->hasPrototype) {
                if (snapshot->prototypeIndex >= checkpoint->objectCount ||
                    !ZrCore_GcRootHandle_Resolve(state,
                        &checkpoint->objects[snapshot->prototypeIndex].handle,
                        &raw)) {
                    return ZR_FALSE;
                }
                object->prototype = (SZrObjectPrototype *)raw;
            } else {
                object->prototype = ZR_NULL;
            }
            if (!checkpoint_restore_map(state, checkpoint,
                                        ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                                        &object->nodeMap,
                                        snapshot->pairs, snapshot->pairCount)) {
                return ZR_FALSE;
            }
            object->memberVersion = snapshot->memberVersion;
            object->cachedHiddenItemsPair = ZR_NULL;
            object->cachedHiddenItemsObject = ZR_NULL;
            object->cachedLengthPair = ZR_NULL;
            object->cachedCapacityPair = ZR_NULL;
            object->cachedStringLookupPair = ZR_NULL;
            object->cachedStringLookupPair2 = ZR_NULL;
            object->cachedIteratorSourcePair = ZR_NULL;
            object->cachedIteratorCurrentPair = ZR_NULL;
            object->cachedIteratorIndexPair = ZR_NULL;
            object->cachedIteratorNextNodePair = ZR_NULL;
            if (snapshot->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                SZrObjectModule *module = (SZrObjectModule *)object;
                if (module->exportDescriptorLength != snapshot->descriptorCount) {
                    return ZR_FALSE;
                }
                module->initState = snapshot->moduleInitState;
                module->metadataGeneration = snapshot->moduleMetadataGeneration;
                if (!checkpoint_restore_map(state, checkpoint,
                                            ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                                            &module->proNodeMap,
                                            snapshot->proPairs, snapshot->proPairCount)) {
                    return ZR_FALSE;
                }
                for (TZrSize descriptorIndex = 0u;
                     descriptorIndex < snapshot->descriptorCount;
                     descriptorIndex++) {
                    module->exportDescriptors[descriptorIndex].isReady =
                            (TZrUInt8)snapshot->descriptorReady[descriptorIndex];
                }
            }
            if (snapshot->rawIntData != ZR_NULL) {
                if (object->superArrayRawIntCapacity != snapshot->rawIntCapacity) {
                    object->superArrayRawIntData = ZR_CAST(TZrInt64 *,
                        ZrCore_Memory_Allocate(state->global,
                            object->superArrayRawIntData,
                            object->superArrayRawIntCapacity * sizeof(TZrInt64),
                            snapshot->rawIntCapacity * sizeof(TZrInt64),
                            ZR_MEMORY_NATIVE_TYPE_ARRAY));
                    object->superArrayRawIntCapacity = snapshot->rawIntCapacity;
                }
                if (snapshot->rawIntCapacity > 0u && object->superArrayRawIntData == ZR_NULL) {
                    return ZR_FALSE;
                }
                memcpy(object->superArrayRawIntData, snapshot->rawIntData,
                       snapshot->rawIntCapacity * sizeof(TZrInt64));
                object->superArrayRawIntLength = snapshot->rawIntLength;
            } else if (object->superArrayRawIntData != ZR_NULL) {
                ZrCore_Memory_Allocate(state->global,
                                       object->superArrayRawIntData,
                                       object->superArrayRawIntCapacity * sizeof(TZrInt64),
                                       0u,
                                       ZR_MEMORY_NATIVE_TYPE_ARRAY);
                object->superArrayRawIntData = ZR_NULL;
                object->superArrayRawIntLength = 0u;
                object->superArrayRawIntCapacity = 0u;
            }
            object->superArrayStorageMode = snapshot->rawIntStorageMode;
            object->superArrayStorageGeneration = snapshot->rawIntStorageGeneration;
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
            if (!raw->isNative) {
                SZrClosure *closure = (SZrClosure *)raw;
                closure->function = ZR_NULL;
                if (snapshot->hasFunction) {
                    if (!ZrCore_GcRootHandle_Resolve(state,
                        &checkpoint->objects[snapshot->functionIndex].handle,
                        (SZrRawObject **)&closure->function)) {
                        return ZR_FALSE;
                    }
                }
                for (TZrSize captureIndex = 0u; captureIndex < snapshot->captureCount; captureIndex++) {
                    if (!ZrCore_GcRootHandle_Resolve(state,
                        &checkpoint->objects[snapshot->captureIndexes[captureIndex]].handle,
                        (SZrRawObject **)&closure->closureValuesExtend[captureIndex])) {
                        return ZR_FALSE;
                    }
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE && snapshot->hasClosureValue) {
            SZrClosureValue *closureValue = (SZrClosureValue *)raw;
            SZrTypeValue restored;
            if (!checkpoint_restore_value(state, checkpoint, &snapshot->closureValue, &restored)) {
                return ZR_FALSE;
            }
            ZrCore_Value_Copy(state, ZrCore_ClosureValue_GetValue(closureValue), &restored);
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            SZrFunction *function = (SZrFunction *)raw;
            for (TZrUInt32 cacheIndex = 0u;
                 function->callSiteCaches != ZR_NULL &&
                 cacheIndex < function->callSiteCacheLength;
                 cacheIndex++) {
                SZrFunctionCallSiteCacheEntry *cache = &function->callSiteCaches[cacheIndex];
                cache->picSlotCount = 0u;
                cache->picNextInsertIndex = 0u;
                cache->runtimeHitCount = 0u;
                cache->runtimeMissCount = 0u;
                memset(cache->picSlots, 0, sizeof(cache->picSlots));
                ZrCore_CallBinding_Invalidate(&cache->binding);
            }
        }
    }
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

TZrBool ZrCore_SessionCheckpoint_Create(SZrState *state, SZrSessionCheckpoint **outCheckpoint) {
    SZrSessionCheckpoint *checkpoint;
    SZrGcDomainPauseDiagnostic pauseDiagnostic;
    TZrBool captured;

    if (state == ZR_NULL || state->global == ZR_NULL || outCheckpoint == ZR_NULL ||
        state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList != &state->baseCallInfo ||
        state->executionBudget != ZR_NULL || state->nestedNativeCalls != 0u ||
        state->aotGcRootFrameDepth != 0u || state->stackClosureValueList != ZR_NULL ||
        state->toBeClosedValueList.valuePointer != state->stackBase.valuePointer ||
        state->exceptionHandlerStackLength != 0u) {
        return ZR_FALSE;
    }
    memset(&pauseDiagnostic, 0, sizeof(pauseDiagnostic));
    if (!ZrCore_GcDomain_StopTheWorldBegin(state, 1000u, &pauseDiagnostic)) {
        return ZR_FALSE;
    }
    checkpoint = (SZrSessionCheckpoint *)calloc(1, sizeof(*checkpoint));
    if (checkpoint == ZR_NULL) {
        ZrCore_GcDomain_StopTheWorldEnd(state);
        return ZR_FALSE;
    }
    checkpoint->state = state;
    checkpoint->errorPrototypeIndex = ZR_CHECKPOINT_NO_INDEX;
    checkpoint->stackFramePrototypeIndex = ZR_CHECKPOINT_NO_INDEX;
    for (TZrSize index = 0u; index < ZR_VALUE_TYPE_ENUM_MAX; index++) {
        checkpoint->basicTypePrototypeIndexes[index] = ZR_CHECKPOINT_NO_INDEX;
    }
    for (TZrSize index = 0u; index < ZR_META_ENUM_MAX; index++) {
        checkpoint->metaFunctionNameIndexes[index] = ZR_CHECKPOINT_NO_INDEX;
    }
    if (!checkpoint_capture_value(checkpoint, &state->global->loadedModulesRegistry,
                                  &checkpoint->loadedModulesRegistry) ||
        !checkpoint_capture_value(checkpoint, &state->global->nullValue,
                                  &checkpoint->nullValue) ||
        !checkpoint_capture_value(checkpoint, &state->global->zrObject,
                                  &checkpoint->zrObject)) {
        captured = ZR_FALSE;
    } else {
        captured = ZR_TRUE;
    }
    checkpoint->hasUnhandledExceptionHandler = state->global->hasUnhandledExceptionHandler;
    if (checkpoint->hasUnhandledExceptionHandler &&
        !checkpoint_capture_value(checkpoint, &state->global->unhandledExceptionHandler,
                                  &checkpoint->unhandledExceptionHandler)) {
        captured = ZR_FALSE;
    }
    if (captured && state->global->errorPrototype != ZR_NULL) {
        checkpoint->hasErrorPrototype = ZR_TRUE;
        captured = checkpoint_capture_object(checkpoint,
            ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->errorPrototype),
            &checkpoint->errorPrototypeIndex);
    }
    if (captured && state->global->stackFramePrototype != ZR_NULL) {
        checkpoint->hasStackFramePrototype = ZR_TRUE;
        captured = checkpoint_capture_object(checkpoint,
            ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->stackFramePrototype),
            &checkpoint->stackFramePrototypeIndex);
    }
    for (TZrSize index = 0u; captured && index < ZR_VALUE_TYPE_ENUM_MAX; index++) {
        if (state->global->basicTypeObjectPrototype[index] != ZR_NULL) {
            checkpoint->hasBasicTypePrototype[index] = ZR_TRUE;
            captured = checkpoint_capture_object(checkpoint,
                ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->basicTypeObjectPrototype[index]),
                &checkpoint->basicTypePrototypeIndexes[index]);
        }
    }
    for (TZrSize index = 0u; captured && index < ZR_META_ENUM_MAX; index++) {
        if (state->global->metaFunctionName[index] != ZR_NULL) {
            checkpoint->hasMetaFunctionName[index] = ZR_TRUE;
            captured = checkpoint_capture_object(checkpoint,
                ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->metaFunctionName[index]),
                &checkpoint->metaFunctionNameIndexes[index]);
        }
    }
    ZrCore_GcDomain_StopTheWorldEnd(state);
    if (!captured) {
        ZrCore_SessionCheckpoint_Free(state, checkpoint);
        return ZR_FALSE;
    }
    *outCheckpoint = checkpoint;
    return ZR_TRUE;
}

TZrBool ZrCore_SessionCheckpoint_Rollback(SZrState *state, const SZrSessionCheckpoint *checkpoint) {
    SZrGcDomainPauseDiagnostic pauseDiagnostic;
    TZrBool restored;
    if (state == ZR_NULL || checkpoint == ZR_NULL || checkpoint->state != state ||
        state->executionBudget != ZR_NULL || state->nestedNativeCalls != 0u ||
        state->aotGcRootFrameDepth != 0u || state->stackClosureValueList != ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&pauseDiagnostic, 0, sizeof(pauseDiagnostic));
    if (!ZrCore_GcDomain_StopTheWorldBegin(state, 1000u, &pauseDiagnostic)) {
        return ZR_FALSE;
    }
    if ((state->threadStatus != ZR_THREAD_STATUS_FINE ||
         state->callInfoList != &state->baseCallInfo || state->hasCurrentException) &&
        ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE) != ZR_THREAD_STATUS_FINE) {
        ZrCore_GcDomain_StopTheWorldEnd(state);
        return ZR_FALSE;
    }
    restored = checkpoint_restore(state, checkpoint);
    ZrCore_GcDomain_StopTheWorldEnd(state);
    return restored;
}

void ZrCore_SessionCheckpoint_Free(SZrState *state, SZrSessionCheckpoint *checkpoint) {
    if (checkpoint == ZR_NULL) {
        return;
    }
    if (state != ZR_NULL && checkpoint->state == state) {
        for (TZrSize index = 0u; index < checkpoint->objectCount; index++) {
            ZrCore_GcRootHandle_Release(state, &checkpoint->objects[index].handle);
        }
    }
    for (TZrSize index = 0u; index < checkpoint->objectCount; index++) {
        free(checkpoint->objects[index].pairs);
        free(checkpoint->objects[index].proPairs);
        free(checkpoint->objects[index].captureIndexes);
        free(checkpoint->objects[index].descriptorReady);
        free(checkpoint->objects[index].rawIntData);
    }
    free(checkpoint->objects);
    free(checkpoint);
}
