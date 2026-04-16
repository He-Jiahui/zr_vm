//
// Marking, propagation, and barrier helpers for the GC.
//

#include "gc/gc_internal.h"

static TZrSize garbage_collector_mark_hash_set(SZrState *state, const SZrHashSet *set) {
    TZrSize work = 0;

    if (state == ZR_NULL || set == ZR_NULL || !set->isValid) {
        return 0;
    }

    for (TZrSize i = 0; i < set->capacity; i++) {
        SZrHashKeyValuePair *pair = set->buckets[i];

        while (pair != ZR_NULL) {
            garbage_collector_mark_value(state, &pair->key);
            garbage_collector_mark_value(state, &pair->value);
            pair = pair->next;
            work++;
        }
    }

    return work;
}

static TZrSize garbage_collector_mark_object_node_map(SZrState *state, SZrObject *object) {
    if (object == ZR_NULL) {
        return 0;
    }

    return garbage_collector_mark_hash_set(state, &object->nodeMap);
}

TZrSize garbage_collector_mark_string_roots(SZrState *state) {
    SZrGlobalState *global;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return 0;
    }

    global = state->global;

    if (global->memoryErrorMessage != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->memoryErrorMessage));
        work++;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT; bucketIndex++) {
        for (TZrSize depthIndex = 0; depthIndex < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH; depthIndex++) {
            if (global->stringHashApiCache[bucketIndex][depthIndex] != ZR_NULL) {
                garbage_collector_mark_object(
                        state,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(global->stringHashApiCache[bucketIndex][depthIndex]));
                work++;
            }
        }
    }

    for (TZrSize metaIndex = 0; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
        if (global->metaFunctionName[metaIndex] != ZR_NULL) {
            garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->metaFunctionName[metaIndex]));
            work++;
        }
    }

    if (global->stringTable != ZR_NULL && global->stringTable->stringHashSet.isValid &&
        global->stringTable->stringHashSet.buckets != ZR_NULL) {
        for (TZrSize bucketIndex = 0; bucketIndex < global->stringTable->stringHashSet.capacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = global->stringTable->stringHashSet.buckets[bucketIndex];

            while (pair != ZR_NULL) {
                garbage_collector_mark_value(state, &pair->key);
                pair = pair->next;
                work++;
            }
        }
    }

    return work;
}

TZrSize garbage_collector_mark_ignored_roots(SZrState *state) {
    SZrGarbageCollector *collector;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return 0;
    }

    collector = state->global->garbageCollector;
    for (TZrSize index = 0; index < collector->ignoredObjectCount; index++) {
        SZrRawObject *object = collector->ignoredObjects[index];

        if (object == ZR_NULL ||
            object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
            object->type == ZR_RAW_OBJECT_TYPE_INVALID ||
            ZrCore_RawObject_IsReleased(object)) {
            continue;
        }

        garbage_collector_mark_object(state, object);
        work++;
    }

    return work;
}

static TZrBool garbage_collector_minor_collection_is_active(SZrState *state) {
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    return collector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL &&
           (collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK ||
            collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE);
}

static TZrBool garbage_collector_try_mark_embedded_child_function(SZrState *state, SZrRawObject *object) {
    SZrGarbageCollector *collector;
    SZrFunction *function;
    TZrBool minorActive;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL ||
        object->type != ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_FALSE;
    }

    function = ZR_CAST_FUNCTION(state, object);
    if (function == ZR_NULL || function->ownerFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    minorActive = garbage_collector_minor_collection_is_active(state);
    if (minorActive) {
        if (collector->minorCollectionEpoch != 0u &&
            object->garbageCollectMark.minorScanEpoch == collector->minorCollectionEpoch &&
            (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object))) {
            return ZR_TRUE;
        }
        object->garbageCollectMark.minorScanEpoch = collector->minorCollectionEpoch;
    } else {
        if (object->garbageCollectMark.generation == collector->gcGeneration &&
            (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object))) {
            return ZR_TRUE;
        }
        object->garbageCollectMark.generation = collector->gcGeneration;
    }

    object->gcList = ZR_NULL;
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    ZrGarbageCollectorReallyMarkObject(state, object);
    return ZR_TRUE;
}

static TZrBool garbage_collector_try_mark_object_during_minor(SZrState *state, SZrRawObject *object) {
    SZrGarbageCollector *collector;

    if (!garbage_collector_minor_collection_is_active(state) || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID ||
        ZrCore_RawObject_IsReleased(object)) {
        return ZR_TRUE;
    }

    if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    if (collector->minorCollectionEpoch == 0u ||
        object->garbageCollectMark.minorScanEpoch == collector->minorCollectionEpoch) {
        return ZR_TRUE;
    }

    object->garbageCollectMark.minorScanEpoch = collector->minorCollectionEpoch;
    if (object->type == ZR_RAW_OBJECT_TYPE_STRING) {
        return ZR_TRUE;
    }

    object->gcList = collector->waitToScanObjectList;
    collector->waitToScanObjectList = object;
    if (object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
        ZrCore_RawObject_MarkAsWaitToScan(object);
    }
    return ZR_TRUE;
}

void garbage_collector_mark_object(SZrState *state, SZrRawObject *object) {
    if (garbage_collector_try_mark_embedded_child_function(state, object)) {
        return;
    }
    if (garbage_collector_try_mark_object_during_minor(state, object)) {
        return;
    }
    if (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object)) {
        return;
    }
    if (ZrCore_RawObject_IsMarkInited(object)) {
        ZrGarbageCollectorReallyMarkObject(state, object);
    }
}

void garbage_collector_mark_value(SZrState *state, SZrTypeValue *value) {
    ZrCore_Gc_ValueStaticAssertIsAlive(state, value);
    if (ZrCore_Value_IsGarbageCollectable(value)) {
        SZrRawObject *object = value->value.object;

        if (garbage_collector_try_mark_embedded_child_function(state, object)) {
            return;
        }
        if (garbage_collector_try_mark_object_during_minor(state, object)) {
            return;
        }
        if (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object)) {
            return;
        }
        if (ZrCore_RawObject_IsMarkInited(object)) {
            ZrGarbageCollectorReallyMarkObject(state, object);
        }
    }
}

static void garbage_collector_mark_string_if_present(SZrState *state, SZrString *stringValue) {
    if (state != ZR_NULL && stringValue != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    }
}

static void garbage_collector_mark_typed_type_ref(SZrState *state, const SZrFunctionTypedTypeRef *typeRef) {
    if (state == ZR_NULL || typeRef == ZR_NULL) {
        return;
    }

    garbage_collector_mark_string_if_present(state, typeRef->typeName);
    garbage_collector_mark_string_if_present(state, typeRef->elementTypeName);
}

static void garbage_collector_mark_metadata_parameters(SZrState *state,
                                                       const SZrFunctionMetadataParameter *parameters,
                                                       TZrUInt32 parameterCount,
                                                       TZrSize *work) {
    if (state == ZR_NULL || parameters == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionMetadataParameter *parameter = &parameters[index];

        garbage_collector_mark_string_if_present(state, parameter->name);
        garbage_collector_mark_typed_type_ref(state, &parameter->type);
        if (parameter->hasDefaultValue) {
            garbage_collector_mark_value(state, (SZrTypeValue *)&parameter->defaultValue);
            if (work != ZR_NULL) {
                (*work)++;
            }
        }
        if (parameter->hasDecoratorMetadata) {
            garbage_collector_mark_value(state, (SZrTypeValue *)&parameter->decoratorMetadataValue);
            if (work != ZR_NULL) {
                (*work)++;
            }
        }
        for (TZrUInt32 decoratorIndex = 0; decoratorIndex < parameter->decoratorCount; decoratorIndex++) {
            garbage_collector_mark_string_if_present(state, parameter->decoratorNames[decoratorIndex]);
        }
    }

    if (work != ZR_NULL) {
        *work += parameterCount;
    }
}

static void garbage_collector_mark_function_if_present(SZrState *state, SZrFunction *function, TZrSize *work) {
    if (state == ZR_NULL || function == ZR_NULL) {
        return;
    }

    garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    if (work != ZR_NULL) {
        (*work)++;
    }
}

static void garbage_collector_mark_call_info_function(SZrState *state,
                                                      SZrState *threadState,
                                                      const SZrCallInfo *callInfo,
                                                      TZrSize *work) {
    SZrFunction *function;

    if (state == ZR_NULL || threadState == ZR_NULL || callInfo == ZR_NULL) {
        return;
    }

    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(threadState, (SZrCallInfo *)callInfo);
    garbage_collector_mark_function_if_present(state, function, work);
}

static void garbage_collector_mark_object_prototype_graph(SZrState *state,
                                                          SZrObjectPrototype *prototype,
                                                          TZrSize *work) {
    if (state == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    garbage_collector_mark_string_if_present(state, prototype->name);
    if (prototype->name != ZR_NULL && work != ZR_NULL) {
        (*work)++;
    }
    if (prototype->superPrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->superPrototype));
        if (work != ZR_NULL) {
            (*work)++;
        }
    }

    for (TZrUInt32 metaIndex = 0; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
        SZrMeta *meta = prototype->metaTable.metas[metaIndex];

        if (meta != ZR_NULL) {
            garbage_collector_mark_function_if_present(state, meta->function, work);
        }
    }

    for (TZrUInt32 memberIndex = 0; memberIndex < prototype->memberDescriptorCount; memberIndex++) {
        SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[memberIndex];

        garbage_collector_mark_string_if_present(state, descriptor->name);
        garbage_collector_mark_string_if_present(state, descriptor->ownerTypeName);
        garbage_collector_mark_string_if_present(state, descriptor->baseDefinitionOwnerTypeName);
        garbage_collector_mark_string_if_present(state, descriptor->baseDefinitionName);
        garbage_collector_mark_function_if_present(state, descriptor->getterFunction, work);
        garbage_collector_mark_function_if_present(state, descriptor->setterFunction, work);
        if (work != ZR_NULL) {
            (*work) += descriptor->name != ZR_NULL ? 1u : 0u;
            (*work) += descriptor->ownerTypeName != ZR_NULL ? 1u : 0u;
            (*work) += descriptor->baseDefinitionOwnerTypeName != ZR_NULL ? 1u : 0u;
            (*work) += descriptor->baseDefinitionName != ZR_NULL ? 1u : 0u;
        }
    }

    garbage_collector_mark_function_if_present(state, prototype->indexContract.getByIndexFunction, work);
    garbage_collector_mark_function_if_present(state, prototype->indexContract.setByIndexFunction, work);
    garbage_collector_mark_function_if_present(state, prototype->indexContract.containsKeyFunction, work);
    garbage_collector_mark_function_if_present(state, prototype->indexContract.getLengthFunction, work);
    garbage_collector_mark_function_if_present(state, prototype->iterableContract.iterInitFunction, work);
    garbage_collector_mark_function_if_present(state, prototype->iteratorContract.moveNextFunction, work);
    garbage_collector_mark_function_if_present(state, prototype->iteratorContract.currentFunction, work);
    garbage_collector_mark_string_if_present(state, prototype->iteratorContract.currentMemberName);
    if (prototype->iteratorContract.currentMemberName != ZR_NULL && work != ZR_NULL) {
        (*work)++;
    }

    for (TZrUInt32 fieldIndex = 0; fieldIndex < prototype->managedFieldCount; fieldIndex++) {
        garbage_collector_mark_string_if_present(state, prototype->managedFields[fieldIndex].name);
        if (prototype->managedFields[fieldIndex].name != ZR_NULL && work != ZR_NULL) {
            (*work)++;
        }
    }

    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;

        if (work != ZR_NULL) {
            *work += garbage_collector_mark_hash_set(state, &structPrototype->keyOffsetMap);
        } else {
            (void)garbage_collector_mark_hash_set(state, &structPrototype->keyOffsetMap);
        }
    }
}

void garbage_collector_link_to_gray_list(SZrRawObject *object, SZrRawObject **list) {
    SZrRawObject *current = *list;
    TZrSize checkCount = 0;
    const TZrSize maxCheckCount = ZR_GC_GRAY_LIST_DUPLICATE_SCAN_LIMIT;

    if (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object)) {
        return;
    }

    while (current != ZR_NULL && checkCount < maxCheckCount) {
        if (current == object) {
            return;
        }
        current = current->gcList;
        checkCount++;
    }

    object->gcList = *list;
    *list = object;
    ZrCore_RawObject_MarkAsWaitToScan(object);
}

void garbage_collector_to_gc_list_and_mark_wait_to_scan(SZrRawObject *object, SZrRawObject **list) {
    garbage_collector_link_to_gray_list(object, list);
}

void ZrGarbageCollectorReallyMarkObject(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global;

    if (object == ZR_NULL || state == ZR_NULL) {
        return;
    }

    global = state->global;
    if (global == ZR_NULL) {
        return;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX || object->type == ZR_RAW_OBJECT_TYPE_INVALID) {
        return;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_STRING:
            ZR_GC_SET_REFERENCED(object);
            break;
        case ZR_RAW_OBJECT_TYPE_CLOSURE: {
            if (object->isNative) {
                SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);

                if (closure->aotShimFunction != ZR_NULL) {
                    garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure->aotShimFunction));
                }

                for (TZrSize i = 0; i < closure->closureValueCount; i++) {
                    SZrRawObject *captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, i);
                    if (captureOwner != ZR_NULL) {
                        garbage_collector_mark_object(state, captureOwner);
                    } else if (closure->closureValuesExtend[i] != ZR_NULL) {
                        garbage_collector_mark_value(state, closure->closureValuesExtend[i]);
                    }
                }
            } else {
                SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);

                if (closure->function != ZR_NULL) {
                    garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure->function));
                }
                for (TZrSize i = 0; i < closure->closureValueCount; i++) {
                    if (closure->closureValuesExtend[i] != ZR_NULL) {
                        garbage_collector_mark_object(
                                state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[i]));
                    }
                }
            }
            garbage_collector_link_to_gray_list(object, &global->garbageCollector->waitToScanObjectList);
            break;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE: {
            SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, object);

            if (ZrCore_ClosureValue_IsClosed(closureValue)) {
                ZR_GC_SET_REFERENCED(object);
            } else {
                garbage_collector_link_to_gray_list(object, &global->garbageCollector->waitToScanObjectList);
            }
            garbage_collector_mark_value(state, &closureValue->value.valuePointer->value);
            break;
        }
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            struct SZrNativeData *nativeData = ZR_CAST(struct SZrNativeData *, object);

            ZR_GC_SET_REFERENCED(object);
            for (TZrUInt32 i = 0; i < nativeData->valueLength; i++) {
                garbage_collector_mark_value(state, &nativeData->valueExtend[i]);
            }

            if (object->scanMarkGcFunction != ZR_NULL) {
                object->scanMarkGcFunction(state, object);
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_BUFFER:
        case ZR_RAW_OBJECT_TYPE_ARRAY:
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_THREAD:
            garbage_collector_link_to_gray_list(object, &global->garbageCollector->waitToScanObjectList);
            break;
        default:
            ZR_ASSERT(ZR_FALSE);
            break;
    }
}

static TZrSize garbage_collector_scan_object(SZrState *state, SZrRawObject *object) {
    TZrSize work = 0;

    if (state == ZR_NULL || object == ZR_NULL) {
        return 0;
    }

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            SZrObject *obj = ZR_CAST_OBJECT(state, object);

            work += garbage_collector_mark_object_node_map(state, obj);
            if (obj != ZR_NULL && obj->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
                garbage_collector_mark_object_prototype_graph(state, (SZrObjectPrototype *)obj, &work);
            }
            if (obj != ZR_NULL && obj->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                SZrObjectModule *module = (SZrObjectModule *)obj;

                if (module->moduleName != ZR_NULL) {
                    garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module->moduleName));
                    work++;
                }
                if (module->fullPath != ZR_NULL) {
                    garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module->fullPath));
                    work++;
                }
                if (module->proNodeMap.isValid) {
                    for (TZrSize i = 0; i < module->proNodeMap.capacity; i++) {
                        SZrHashKeyValuePair *pair = module->proNodeMap.buckets[i];

                        while (pair != ZR_NULL) {
                            garbage_collector_mark_value(state, &pair->key);
                            garbage_collector_mark_value(state, &pair->value);
                            pair = pair->next;
                            work++;
                        }
                    }
                }
                for (TZrUInt32 descriptorIndex = 0; descriptorIndex < module->exportDescriptorLength; descriptorIndex++) {
                    SZrModuleExportDescriptor *descriptor = &module->exportDescriptors[descriptorIndex];

                    if (descriptor->name != ZR_NULL) {
                        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->name));
                        work++;
                    }
                }
            }
            if (obj->prototype != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(obj->prototype));
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE: {
            if (object->isNative) {
                SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);

                for (TZrSize i = 0; i < closure->closureValueCount; i++) {
                    SZrRawObject *captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, i);
                    if (captureOwner != ZR_NULL) {
                        garbage_collector_mark_object(state, captureOwner);
                    } else if (closure->closureValuesExtend[i] != ZR_NULL) {
                        garbage_collector_mark_value(state, closure->closureValuesExtend[i]);
                    }
                }
                work = closure->closureValueCount;
            } else {
                SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);

                if (closure->function != ZR_NULL) {
                    garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure->function));
                }
                for (TZrSize i = 0; i < closure->closureValueCount; i++) {
                    if (closure->closureValuesExtend[i] != ZR_NULL) {
                        garbage_collector_mark_object(
                                state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[i]));
                    }
                }
                work = closure->closureValueCount + (closure->function != ZR_NULL ? 1 : 0);
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE: {
            SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, object);
            SZrTypeValue *value = closureValue != ZR_NULL ? ZrCore_ClosureValue_GetValue(closureValue) : ZR_NULL;

            if (value != ZR_NULL) {
                garbage_collector_mark_value(state, value);
                work = 1;
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_FUNCTION: {
            SZrFunction *function = ZR_CAST_FUNCTION(state, object);

            if (function->ownerFunction != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->ownerFunction));
                work++;
            }
            if (function->functionName != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->functionName));
            }
            for (TZrUInt32 i = 0; i < function->closureValueLength; i++) {
                if (function->closureValueList[i].name != ZR_NULL) {
                    garbage_collector_mark_object(
                            state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->closureValueList[i].name));
                }
            }
            for (TZrUInt32 i = 0; i < function->constantValueLength; i++) {
                garbage_collector_mark_value(state, &function->constantValueList[i]);
            }
            for (TZrUInt32 i = 0; i < function->catchClauseCount; i++) {
                if (function->catchClauseList[i].typeName != ZR_NULL) {
                    garbage_collector_mark_object(
                            state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->catchClauseList[i].typeName));
                }
            }
            for (TZrUInt32 i = 0; i < function->childFunctionLength; i++) {
                if (function->childFunctionList[i].super.type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                    garbage_collector_mark_object(state, &function->childFunctionList[i].super);
                }
            }
            if (function->runtimeDecoratorMetadata != ZR_NULL) {
                garbage_collector_mark_object(state,
                                              ZR_CAST_RAW_OBJECT_AS_SUPER(function->runtimeDecoratorMetadata));
            }
            if (function->runtimeDecoratorDecorators != ZR_NULL) {
                garbage_collector_mark_object(state,
                                              ZR_CAST_RAW_OBJECT_AS_SUPER(function->runtimeDecoratorDecorators));
            }
            if (function->cachedStatelessClosure != ZR_NULL) {
                garbage_collector_mark_object(state,
                                              ZR_CAST_RAW_OBJECT_AS_SUPER(function->cachedStatelessClosure));
                work++;
            }
            if (function->sourceCodeList != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->sourceCodeList));
            }
            if (function->sourceHash != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->sourceHash));
            }
            for (TZrUInt32 i = 0; i < function->localVariableLength; i++) {
                garbage_collector_mark_string_if_present(state, function->localVariableList[i].name);
            }
            for (TZrUInt32 i = 0; i < function->exportedVariableLength; i++) {
                garbage_collector_mark_string_if_present(state, function->exportedVariables[i].name);
            }
            for (TZrUInt32 i = 0; i < function->typedLocalBindingLength; i++) {
                garbage_collector_mark_string_if_present(state, function->typedLocalBindings[i].name);
                garbage_collector_mark_typed_type_ref(state, &function->typedLocalBindings[i].type);
            }
            for (TZrUInt32 i = 0; i < function->typedExportedSymbolLength; i++) {
                SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[i];

                garbage_collector_mark_string_if_present(state, symbol->name);
                garbage_collector_mark_typed_type_ref(state, &symbol->valueType);
                for (TZrUInt32 parameterIndex = 0; parameterIndex < symbol->parameterCount; parameterIndex++) {
                    garbage_collector_mark_typed_type_ref(state, &symbol->parameterTypes[parameterIndex]);
                }
            }
            if (function->staticImports != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->staticImportLength; i++) {
                    garbage_collector_mark_string_if_present(state, function->staticImports[i]);
                }
            }
            if (function->moduleEntryEffects != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->moduleEntryEffectLength; i++) {
                    garbage_collector_mark_string_if_present(state, function->moduleEntryEffects[i].moduleName);
                    garbage_collector_mark_string_if_present(state, function->moduleEntryEffects[i].symbolName);
                }
            }
            if (function->exportedCallableSummaries != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->exportedCallableSummaryLength; i++) {
                    SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[i];

                    garbage_collector_mark_string_if_present(state, summary->name);
                    if (summary->effects != ZR_NULL) {
                        for (TZrUInt32 effectIndex = 0; effectIndex < summary->effectCount; effectIndex++) {
                            garbage_collector_mark_string_if_present(state, summary->effects[effectIndex].moduleName);
                            garbage_collector_mark_string_if_present(state, summary->effects[effectIndex].symbolName);
                        }
                    }
                }
            }
            if (function->topLevelCallableBindings != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->topLevelCallableBindingLength; i++) {
                    garbage_collector_mark_string_if_present(state, function->topLevelCallableBindings[i].name);
                }
            }
            garbage_collector_mark_metadata_parameters(state,
                                                       function->parameterMetadata,
                                                       function->parameterMetadataCount,
                                                       &work);
            for (TZrUInt32 i = 0; i < function->compileTimeVariableInfoLength; i++) {
                SZrFunctionCompileTimeVariableInfo *info = &function->compileTimeVariableInfos[i];

                garbage_collector_mark_string_if_present(state, info->name);
                garbage_collector_mark_typed_type_ref(state, &info->type);
                for (TZrUInt32 bindingIndex = 0; bindingIndex < info->pathBindingCount; bindingIndex++) {
                    garbage_collector_mark_string_if_present(state, info->pathBindings[bindingIndex].path);
                    garbage_collector_mark_string_if_present(state, info->pathBindings[bindingIndex].targetName);
                }
            }
            for (TZrUInt32 i = 0; i < function->compileTimeFunctionInfoLength; i++) {
                SZrFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[i];

                garbage_collector_mark_string_if_present(state, info->name);
                garbage_collector_mark_typed_type_ref(state, &info->returnType);
                garbage_collector_mark_metadata_parameters(state, info->parameters, info->parameterCount, &work);
            }
            for (TZrUInt32 i = 0; i < function->testInfoLength; i++) {
                SZrFunctionTestInfo *info = &function->testInfos[i];

                garbage_collector_mark_string_if_present(state, info->name);
                garbage_collector_mark_metadata_parameters(state, info->parameters, info->parameterCount, &work);
            }
            if (function->escapeBindings != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->escapeBindingLength; i++) {
                    garbage_collector_mark_string_if_present(state, function->escapeBindings[i].name);
                }
            }
            if (function->hasDecoratorMetadata) {
                garbage_collector_mark_value(state, &function->decoratorMetadataValue);
                work++;
            }
            for (TZrUInt32 i = 0; i < function->decoratorCount; i++) {
                garbage_collector_mark_string_if_present(state, function->decoratorNames[i]);
            }
            for (TZrUInt32 i = 0; i < function->memberEntryLength; i++) {
                garbage_collector_mark_string_if_present(state, function->memberEntries[i].symbol);
            }
            if (function->prototypeInstances != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->prototypeInstancesLength; i++) {
                    if (function->prototypeInstances[i] != ZR_NULL) {
                        garbage_collector_mark_object(state,
                                                      ZR_CAST_RAW_OBJECT_AS_SUPER(function->prototypeInstances[i]));
                        work++;
                    }
                }
            }
            for (TZrUInt32 i = 0; i < function->semIrTypeTableLength; i++) {
                garbage_collector_mark_typed_type_ref(state, &function->semIrTypeTable[i]);
            }
            if (function->callSiteCaches != ZR_NULL) {
                for (TZrUInt32 i = 0; i < function->callSiteCacheLength; i++) {
                    SZrFunctionCallSiteCacheEntry *cacheEntry = &function->callSiteCaches[i];

                    garbage_collector_sanitize_callsite_cache_pic(function, i, "mark", cacheEntry);
                    TZrUInt32 picLimit = cacheEntry->picSlotCount;

                    if (picLimit > ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY) {
                        picLimit = ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;
                    }
                    for (TZrUInt32 picIndex = 0; picIndex < picLimit; picIndex++) {
                        SZrFunctionCallSitePicSlot *slot = &cacheEntry->picSlots[picIndex];

                        if (slot->cachedReceiverObject != ZR_NULL) {
                            garbage_collector_mark_object(state,
                                                          ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedReceiverObject));
                            work++;
                        }
                        if (slot->cachedReceiverPrototype != ZR_NULL) {
                            garbage_collector_mark_object(state,
                                                          ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedReceiverPrototype));
                            work++;
                        }
                        if (slot->cachedOwnerPrototype != ZR_NULL) {
                            garbage_collector_mark_object(state,
                                                          ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedOwnerPrototype));
                            work++;
                        }
                        garbage_collector_mark_string_if_present(state, slot->cachedMemberName);
                        garbage_collector_mark_function_if_present(state, slot->cachedFunction, &work);
                    }
                }
            }
            work += function->closureValueLength + function->constantValueLength + function->childFunctionLength;
            break;
        }
        case ZR_RAW_OBJECT_TYPE_THREAD: {
            SZrState *threadState = ZR_CAST(SZrState *, object);
            TZrStackValuePointer stackPtr = threadState->stackBase.valuePointer;
            TZrStackValuePointer stackTop = threadState->stackTop.valuePointer;
            SZrCallInfo *callInfo;
            SZrClosureValue *closureValue;

            while (stackPtr < stackTop) {
                garbage_collector_mark_value(state, &stackPtr->value);
                stackPtr++;
                work++;
            }

            closureValue = threadState->stackClosureValueList;
            while (closureValue != ZR_NULL) {
                SZrTypeValue *closureValueData;

                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue));
                closureValueData = ZrCore_ClosureValue_GetValue(closureValue);
                if (closureValueData != ZR_NULL) {
                    garbage_collector_mark_value(state, closureValueData);
                }
                closureValue = closureValue->link.next;
            }

            if (threadState->hasCurrentException) {
                garbage_collector_mark_value(state, &threadState->currentException);
                work++;
            }
            if (threadState->pendingControl.hasValue) {
                garbage_collector_mark_value(state, &threadState->pendingControl.value);
                work++;
            }

            callInfo = threadState->callInfoList;
            {
                TZrStackValuePointer youngerFrameBase = ZR_NULL;

                while (callInfo != ZR_NULL) {
                    if (callInfo->functionBase.valuePointer != ZR_NULL) {
                        TZrStackValuePointer funcBase = callInfo->functionBase.valuePointer;
                        TZrStackValuePointer funcTop = youngerFrameBase;

                        if (funcTop == ZR_NULL) {
                            funcTop = ZR_CALL_INFO_IS_VM(callInfo) && callInfo->functionTop.valuePointer != ZR_NULL
                                              ? callInfo->functionTop.valuePointer
                                              : threadState->stackTop.valuePointer;
                        }

                        if (funcTop != ZR_NULL && funcTop > funcBase) {
                            while (funcBase < funcTop) {
                                garbage_collector_mark_value(state, &funcBase->value);
                                funcBase++;
                                work++;
                            }
                        }

                        garbage_collector_mark_call_info_function(state, threadState, callInfo, &work);
                        youngerFrameBase = callInfo->functionBase.valuePointer;
                    }
                    callInfo = callInfo->previous;
                }
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            struct SZrNativeData *nativeData = ZR_CAST(struct SZrNativeData *, object);

            for (TZrUInt32 i = 0; i < nativeData->valueLength; i++) {
                garbage_collector_mark_value(state, &nativeData->valueExtend[i]);
                work++;
            }
            break;
        }
        default:
            ZR_ASSERT(ZR_FALSE);
            return 0;
    }

    return work;
}

TZrSize ZrGarbageCollectorPropagateMark(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrRawObject *object = global->garbageCollector->waitToScanObjectList;

    if (object == ZR_NULL) {
        return 0;
    }

    global->garbageCollector->waitToScanObjectList = object->gcList;
    object->gcList = ZR_NULL;

    if (!ZR_GC_IS_REFERENCED(object) &&
        object->garbageCollectMark.status != ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT) {
        ZR_GC_SET_REFERENCED(object);
    }

    return garbage_collector_scan_object(state, object);
}

ZR_CORE_API TZrSize ZrGarbageCollectorPropagateAll(SZrState *state) {
    SZrGlobalState *global = state->global;
    TZrSize total = 0;
    TZrSize iterationCount = 0;
    const TZrSize maxIterations = ZR_GC_PROPAGATE_ALL_ITERATION_LIMIT;

    while (global->garbageCollector->waitToScanObjectList != ZR_NULL) {
        TZrSize work;

        if (++iterationCount > maxIterations) {
            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            break;
        }

        work = ZrGarbageCollectorPropagateMark(state);
        total += work;
    }

    return total;
}

ZR_CORE_API void ZrGarbageCollectorRestartCollection(SZrState *state) {
    SZrGlobalState *global;
    SZrRawObject *stateObject;

    if (state == ZR_NULL) {
        return;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->waitToScanObjectList = ZR_NULL;
    global->garbageCollector->waitToScanAgainObjectList = ZR_NULL;
    global->garbageCollector->waitToReleaseObjectList = ZR_NULL;

    stateObject = ZR_CAST_RAW_OBJECT_AS_SUPER(state);
    if (stateObject != ZR_NULL &&
        stateObject->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
        stateObject->type != ZR_RAW_OBJECT_TYPE_INVALID) {
        ZrGarbageCollectorReallyMarkObject(state, stateObject);
    }

    if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry)) {
        garbage_collector_mark_value(state, &global->loadedModulesRegistry);
    }
    if (global->errorPrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->errorPrototype));
    }
    if (global->stackFramePrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->stackFramePrototype));
    }
    if (global->hasUnhandledExceptionHandler &&
        ZrCore_Value_IsGarbageCollectable(&global->unhandledExceptionHandler)) {
        garbage_collector_mark_value(state, &global->unhandledExceptionHandler);
    }

    garbage_collector_mark_string_roots(state);
    garbage_collector_mark_ignored_roots(state);

    for (TZrSize i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        if (global->basicTypeObjectPrototype[i] != ZR_NULL) {
            garbage_collector_mark_object(
                    state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->basicTypeObjectPrototype[i]));
        }
    }

    if (global->mainThreadState != ZR_NULL) {
        SZrState *threadState = global->mainThreadState;
        SZrRawObject *threadObject = ZR_CAST_RAW_OBJECT_AS_SUPER(threadState);

        if (threadObject != ZR_NULL &&
            threadObject->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
            threadObject->type != ZR_RAW_OBJECT_TYPE_INVALID) {
            ZrGarbageCollectorReallyMarkObject(state, threadObject);
        }
    }

    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION;
}

static TZrBool garbage_collector_object_is_old_or_pinned(const SZrRawObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE ||
           object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED ||
           object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT ||
           object->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT;
}

static TZrBool garbage_collector_object_is_young(const SZrRawObject *object) {
    return object != ZR_NULL && object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
}

static TZrBool garbage_collector_object_is_barrier_source_live(const SZrRawObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_RawObject_IsMarkReferenced((SZrRawObject *)object) ||
           object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
}

static void garbage_collector_remember_object(SZrGlobalState *global, SZrRawObject *object) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return;
    }
    if (!garbage_collector_object_can_hold_gc_references(object)) {
        return;
    }

    if (garbage_collector_remembered_registry_contains(global->garbageCollector, object)) {
        return;
    }

    if (!garbage_collector_ensure_remembered_registry_capacity(global,
                                                               global->garbageCollector->rememberedObjectCount + 1)) {
        return;
    }

    global->garbageCollector->rememberedObjects[global->garbageCollector->rememberedObjectCount++] = object;
    global->garbageCollector->statsSnapshot.rememberedObjectCount =
            (TZrUInt32)global->garbageCollector->rememberedObjectCount;
}

void ZrCore_GarbageCollector_Barrier(SZrState *state, SZrRawObject *object, SZrRawObject *valueObject) {
    SZrGlobalState *global;
    TZrBool sourceIsLive;
    TZrUInt32 propagatedEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    EZrGarbageCollectPromotionReason promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;

    if (state == ZR_NULL || object == ZR_NULL || valueObject == ZR_NULL) {
        return;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnored(global, valueObject)) {
        ZrCore_GarbageCollector_UnignoreObject(global, valueObject);
    }

    if (ZrCore_RawObject_IsUnreferenced(state, object) || ZrCore_RawObject_IsUnreferenced(state, valueObject)) {
        return;
    }

    /*
     * Remembered-set maintenance is a generational invariant, not a tri-color
     * marking invariant. Old-to-young writes must be recorded even while the
     * source object is idle/INITED between collections.
     */
    if (garbage_collector_object_is_old_or_pinned(object) && garbage_collector_object_is_young(valueObject)) {
        garbage_collector_remember_object(global, object);
        if (object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED ||
            object->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_PINNED) {
            propagatedEscapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE;
            promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
        } else if (object->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT) {
            propagatedEscapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
            promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT;
        } else {
            propagatedEscapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE;
            promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE;
        }

        ZrCore_GarbageCollector_MarkRawObjectEscaped(state,
                                                     valueObject,
                                                     propagatedEscapeFlags,
                                                     valueObject->garbageCollectMark.anchorScopeDepth,
                                                     promotionReason);
    }

    sourceIsLive = garbage_collector_object_is_barrier_source_live(object);
    if (!sourceIsLive || !ZrCore_RawObject_IsMarkInited(valueObject)) {
        return;
    }

    if (ZrCore_GarbageCollector_IsInvariant(global)) {
        garbage_collector_mark_object(state, valueObject);
        if (ZrCore_RawObject_IsGenerationalThroughBarrier(object) &&
            !ZrCore_RawObject_IsGenerationalThroughBarrier(valueObject)) {
            ZrCore_RawObject_SetGenerationalStatus(
                    valueObject, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER);
        }
    } else if (ZR_GC_IS_INITED(valueObject)) {
        garbage_collector_mark_object(state, valueObject);
    } else if (ZrCore_GarbageCollector_IsSweeping(global) &&
               global->garbageCollector->gcMode == ZR_GARBAGE_COLLECT_MODE_INCREMENTAL) {
        ZrCore_RawObject_MarkAsInit(state, valueObject);
    }
}

void ZrCore_GarbageCollector_BarrierBack(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;

    ZR_ASSERT(ZrCore_RawObject_IsMarkReferenced(object) && !ZrCore_RawObject_IsUnreferenced(state, object));
    ZR_ASSERT((global->garbageCollector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL) ==
              (ZrCore_RawObject_IsGenerationalThroughBarrier(object) &&
               object->garbageCollectMark.generationalStatus !=
                       ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED));
    if (object->garbageCollectMark.generationalStatus ==
        ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED_PREVIOUS) {
        ZrCore_RawObject_MarkAsWaitToScan(object);
        ZrCore_RawObject_SetGenerationalStatus(
                object, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED);
    } else {
        garbage_collector_to_gc_list_and_mark_wait_to_scan(
                object, &global->garbageCollector->waitToScanAgainObjectList);
    }
    if (ZrCore_RawObject_IsGenerationalThroughBarrier(object)) {
        ZrCore_RawObject_SetGenerationalStatus(object, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED);
    }
}

void ZrCore_RawObject_Barrier(SZrState *state, SZrRawObject *object, SZrRawObject *valueObject) {
    ZrCore_GarbageCollector_Barrier(state, object, valueObject);
}
