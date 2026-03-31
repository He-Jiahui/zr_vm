//
// Marking, propagation, and barrier helpers for the GC.
//

#include "gc_internal.h"

void garbage_collector_mark_object(SZrState *state, SZrRawObject *object) {
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

        if (ZR_GC_IS_REFERENCED(object) || ZR_GC_IS_WAIT_TO_SCAN(object)) {
            return;
        }
        if (ZrCore_RawObject_IsMarkInited(object)) {
            ZrGarbageCollectorReallyMarkObject(state, object);
        }
    }
}

void garbage_collector_link_to_gray_list(SZrRawObject *object, SZrRawObject **list) {
    SZrRawObject *current = *list;
    TZrSize checkCount = 0;
    const TZrSize maxCheckCount = 10000;

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

                for (TZrSize i = 0; i < closure->closureValueCount; i++) {
                    if (closure->closureValuesExtend[i] != ZR_NULL) {
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

TZrSize ZrGarbageCollectorPropagateMark(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrRawObject *object = global->garbageCollector->waitToScanObjectList;
    TZrSize work = 0;

    if (object == ZR_NULL) {
        return 0;
    }

    if (ZR_GC_IS_REFERENCED(object)) {
        global->garbageCollector->waitToScanObjectList = object->gcList;
        object->gcList = ZR_NULL;
        return 0;
    }

    ZR_GC_SET_REFERENCED(object);
    global->garbageCollector->waitToScanObjectList = object->gcList;
    object->gcList = ZR_NULL;

    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_OBJECT: {
            SZrObject *obj = ZR_CAST_OBJECT(state, object);

            if (obj->nodeMap.isValid) {
                for (TZrSize i = 0; i < obj->nodeMap.capacity; i++) {
                    SZrHashKeyValuePair *pair = obj->nodeMap.buckets[i];

                    while (pair != ZR_NULL) {
                        garbage_collector_mark_value(state, &pair->key);
                        garbage_collector_mark_value(state, &pair->value);
                        pair = pair->next;
                        work++;
                    }
                }
            }
            if (obj->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
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
            }
            if (obj->prototype != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(obj->prototype));
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_ARRAY: {
            SZrObject *obj = ZR_CAST_OBJECT(state, object);

            if (obj != ZR_NULL) {
                SZrArray *array = (SZrArray *)obj;

                if (array->head != ZR_NULL && array->isValid && array->length > 0) {
                    if (array->elementSize == sizeof(SZrTypeValue)) {
                        SZrTypeValue *elements = (SZrTypeValue *)array->head;
                        for (TZrSize i = 0; i < array->length; i++) {
                            garbage_collector_mark_value(state, &elements[i]);
                            work++;
                        }
                    } else {
                        work = 1;
                    }
                } else {
                    work = 1;
                }
            } else {
                work = 1;
            }
            break;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE: {
            if (object->isNative) {
                SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);

                for (TZrSize i = 0; i < closure->closureValueCount; i++) {
                    if (closure->closureValuesExtend[i] != ZR_NULL) {
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
            if (function->sourceCodeList != ZR_NULL) {
                garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function->sourceCodeList));
            }
            work = function->closureValueLength + function->constantValueLength + function->childFunctionLength;
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
            while (callInfo != ZR_NULL) {
                if (callInfo->functionBase.valuePointer != ZR_NULL) {
                    TZrStackValuePointer funcBase = callInfo->functionBase.valuePointer;
                    TZrStackValuePointer funcTop = callInfo->next != ZR_NULL
                                                           ? callInfo->next->functionBase.valuePointer
                                                           : threadState->stackTop.valuePointer;

                    while (funcBase < funcTop) {
                        garbage_collector_mark_value(state, &funcBase->value);
                        funcBase++;
                    }
                }
                callInfo = callInfo->next;
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

TZrSize ZrGarbageCollectorPropagateAll(SZrState *state) {
    SZrGlobalState *global = state->global;
    TZrSize total = 0;
    TZrSize iterationCount = 0;
    const TZrSize maxIterations = 10000;

    while (global->garbageCollector->waitToScanObjectList != ZR_NULL) {
        TZrSize work;

        if (++iterationCount > maxIterations) {
            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            break;
        }

        work = ZrGarbageCollectorPropagateMark(state);
        total += work;
        if (work == 0 && global->garbageCollector->waitToScanObjectList != ZR_NULL) {
            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            break;
        }
    }

    return total;
}

void ZrGarbageCollectorRestartCollection(SZrState *state) {
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

void ZrCore_GarbageCollector_Barrier(SZrState *state, SZrRawObject *object, SZrRawObject *valueObject) {
    SZrGlobalState *global;

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

    if (!ZrCore_RawObject_IsMarkReferenced(object) || !ZrCore_RawObject_IsMarkInited(valueObject) ||
        ZrCore_RawObject_IsUnreferenced(state, object) || ZrCore_RawObject_IsUnreferenced(state, valueObject)) {
        return;
    }

    if (ZrCore_GarbageCollector_IsInvariant(global)) {
        garbage_collector_mark_object(state, valueObject);
        if (ZrCore_RawObject_IsGenerationalThroughBarrier(object) &&
            !ZrCore_RawObject_IsGenerationalThroughBarrier(valueObject)) {
            ZrCore_RawObject_SetGenerationalStatus(
                    valueObject, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER);
        }
    } else if (ZR_GC_IS_REFERENCED(object) && ZR_GC_IS_INITED(valueObject)) {
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
    if (ZrCore_RawObject_IsMarkReferenced(object) && ZrCore_RawObject_IsMarkInited(valueObject)) {
        ZrCore_GarbageCollector_Barrier(state, object, valueObject);
    }
}
