#include "session_checkpoint_private.h"

void checkpoint_commit_restore_plan(
        SZrState *state,
        const SZrSessionCheckpoint *checkpoint,
        ZrCheckpointRestorePlan *plan,
        ZrCheckpointRestoreRoots roots) {
    TZrSize index;

    /* No allocation, GC, or recoverable failure is permitted after ResetThread. */

    state->global->loadedModulesRegistry = roots.loadedModulesRegistry;
    state->global->nullValue = roots.nullValue;
    state->global->zrObject = roots.zrObject;
    state->global->hasUnhandledExceptionHandler = checkpoint->hasUnhandledExceptionHandler;
    state->global->unhandledExceptionHandler = roots.unhandledExceptionHandler;
    state->global->errorPrototype = roots.errorPrototype;
    state->global->stackFramePrototype = roots.stackFramePrototype;
    for (index = 0u; index < ZR_VALUE_TYPE_ENUM_MAX; index++) {
        state->global->basicTypeObjectPrototype[index] = roots.basicTypePrototypes[index];
    }
    for (index = 0u; index < ZR_META_ENUM_MAX; index++) {
        state->global->metaFunctionName[index] = roots.metaFunctionNames[index];
    }
    memset(state->global->stringHashApiCache, 0, sizeof(state->global->stringHashApiCache));
    memset(state->global->stringConcatPairCache, 0, sizeof(state->global->stringConcatPairCache));

    for (index = 0u; index < checkpoint->objectCount; index++) {
        const ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        ZrCheckpointRestorePlan *prepared = &plan[index];
        SZrRawObject *raw = prepared->object;

        if (snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT ||
            snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            SZrObject *object = (SZrObject *)raw;
            SZrHashSet previousNodeMap = object->nodeMap;
            object->nodeMap = prepared->nodeMap;
            prepared->nodeMap = previousNodeMap;
            object->prototype = prepared->prototype;
            if (prepared->prototype != ZR_NULL) {
                ZrCore_RawObject_Barrier(state, raw,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(prepared->prototype));
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
            for (TZrSize bucketIndex = 0u; bucketIndex < object->nodeMap.capacity; bucketIndex++) {
                for (SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
                     pair != ZR_NULL; pair = pair->next) {
                    ZrCore_Value_Barrier(state, raw, &pair->key);
                    ZrCore_Value_Barrier(state, raw, &pair->value);
                }
            }
            if (snapshot->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                SZrObjectModule *module = (SZrObjectModule *)object;
                SZrHashSet previousProNodeMap = module->proNodeMap;
                TZrSize oldDescriptorBytes = (TZrSize)module->exportDescriptorLength *
                                             sizeof(*module->exportDescriptors);
                module->proNodeMap = prepared->proNodeMap;
                prepared->proNodeMap = previousProNodeMap;
                module->moduleName = prepared->moduleName;
                module->fullPath = prepared->fullPath;
                module->pathHash = snapshot->modulePathHash;
                module->initState = snapshot->moduleInitState;
                module->reserved0 = snapshot->moduleReserved0;
                module->reserved1 = snapshot->moduleReserved1;
                module->metadataGeneration = snapshot->moduleMetadataGeneration;
                module->hasMetadataRuntime = snapshot->moduleHasMetadataRuntime;
                module->metadataRuntime = snapshot->metadataRuntime;
                module->metadataRuntime.module = module->hasMetadataRuntime ? module : ZR_NULL;
                module->metadataRuntime.metadataFunction = prepared->metadataFunction;
                if (module->moduleName != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(module->moduleName));
                }
                if (module->fullPath != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(module->fullPath));
                }
                if (module->metadataRuntime.metadataFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(module->metadataRuntime.metadataFunction));
                }
                for (TZrSize bucketIndex = 0u; bucketIndex < module->proNodeMap.capacity; bucketIndex++) {
                    for (SZrHashKeyValuePair *pair = module->proNodeMap.buckets[bucketIndex];
                         pair != ZR_NULL; pair = pair->next) {
                        ZrCore_Value_Barrier(state, raw, &pair->key);
                        ZrCore_Value_Barrier(state, raw, &pair->value);
                    }
                }
                if (module->exportDescriptors != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(state->global, module->exportDescriptors,
                            oldDescriptorBytes, ZR_MEMORY_NATIVE_TYPE_OBJECT);
                }
                module->exportDescriptors = prepared->descriptors;
                prepared->descriptors = ZR_NULL;
                module->exportDescriptorLength = (TZrUInt32)snapshot->descriptorCount;
                for (TZrSize descriptorIndex = 0u;
                     descriptorIndex < snapshot->descriptorCount; descriptorIndex++) {
                    if (module->exportDescriptors[descriptorIndex].name != ZR_NULL) {
                        ZrCore_RawObject_Barrier(state, raw,
                                ZR_CAST_RAW_OBJECT_AS_SUPER(module->exportDescriptors[descriptorIndex].name));
                    }
                }
            }
            if (snapshot->prototype != ZR_NULL) {
                const ZrCheckpointPrototype *prototypeSnapshot = snapshot->prototype;
                SZrObjectPrototype *prototype = (SZrObjectPrototype *)object;
                if (prototype->memberDescriptors != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(state->global, prototype->memberDescriptors,
                            (TZrSize)prototype->memberDescriptorCapacity *
                                sizeof(*prototype->memberDescriptors),
                            ZR_MEMORY_NATIVE_TYPE_OBJECT);
                }
                if (prototype->interfaceDispatchEntries != ZR_NULL) {
                    ZrCore_Memory_RawFree(state->global, prototype->interfaceDispatchEntries,
                            (TZrSize)prototype->interfaceDispatchCount *
                                sizeof(*prototype->interfaceDispatchEntries));
                }
                if (prototype->managedFields != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(state->global, prototype->managedFields,
                            (TZrSize)prototype->managedFieldCapacity *
                                sizeof(*prototype->managedFields),
                            ZR_MEMORY_NATIVE_TYPE_OBJECT);
                }
                for (TZrSize metaIndex = 0u; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
                    if (prototype->metaTable.metas[metaIndex] != ZR_NULL) {
                        state->global->allocator(state->global->userAllocationArguments,
                                prototype->metaTable.metas[metaIndex], sizeof(SZrMeta), 0u,
                                ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                    }
                    prototype->metaTable.metas[metaIndex] = prepared->prototypeMetas[metaIndex];
                    prepared->prototypeMetas[metaIndex] = ZR_NULL;
                }
                if (prototypeSnapshot->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                    SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;
                    SZrHashSet previousMap = structPrototype->keyOffsetMap;
                    structPrototype->keyOffsetMap = prepared->prototypeKeyOffsetMap;
                    prepared->prototypeKeyOffsetMap = previousMap;
                }

                prototype->name = prepared->prototypeName;
                prototype->type = prototypeSnapshot->type;
                prototype->superPrototype = prepared->prototypeSuper;
                prototype->shapeId = prototypeSnapshot->shapeId;
                prototype->shapeGeneration = prototypeSnapshot->shapeGeneration;
                prototype->layoutGeneration = prototypeSnapshot->layoutGeneration;
                prototype->memberDescriptors = prepared->prototypeMemberDescriptors;
                prepared->prototypeMemberDescriptors = ZR_NULL;
                prototype->memberDescriptorCount = (TZrUInt32)prototypeSnapshot->memberDescriptorCount;
                prototype->memberDescriptorCapacity = (TZrUInt32)prototypeSnapshot->memberDescriptorCapacity;
                prototype->interfaceDispatchEntries = prepared->prototypeInterfaceDispatchEntries;
                prepared->prototypeInterfaceDispatchEntries = ZR_NULL;
                prototype->interfaceDispatchCount = (TZrUInt32)prototypeSnapshot->interfaceDispatchCount;
                prototype->indexContract = prepared->prototypeIndexContract;
                prototype->iterableContract = prepared->prototypeIterableContract;
                prototype->iteratorContract = prepared->prototypeIteratorContract;
                prototype->protocolMask = prototypeSnapshot->protocolMask;
                prototype->dynamicMemberCapable = prototypeSnapshot->dynamicMemberCapable;
                prototype->reserved0 = prototypeSnapshot->reserved0;
                prototype->reserved1 = prototypeSnapshot->reserved1;
                prototype->modifierFlags = prototypeSnapshot->modifierFlags;
                prototype->nextVirtualSlotIndex = prototypeSnapshot->nextVirtualSlotIndex;
                prototype->nextPropertyIdentity = prototypeSnapshot->nextPropertyIdentity;
                prototype->layoutByteSize = prototypeSnapshot->layoutByteSize;
                prototype->layoutByteAlign = prototypeSnapshot->layoutByteAlign;
                prototype->managedFields = prepared->prototypeManagedFields;
                prepared->prototypeManagedFields = ZR_NULL;
                prototype->managedFieldCount = (TZrUInt32)prototypeSnapshot->managedFieldCount;
                prototype->managedFieldCapacity = (TZrUInt32)prototypeSnapshot->managedFieldCapacity;

                if (prototype->name != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->name));
                }
                if (prototype->superPrototype != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->superPrototype));
                }
                for (TZrSize metaIndex = 0u; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
                    if (prototype->metaTable.metas[metaIndex] != ZR_NULL &&
                        prototype->metaTable.metas[metaIndex]->function != ZR_NULL) {
                        ZrCore_RawObject_Barrier(state, raw,
                                ZR_CAST_RAW_OBJECT_AS_SUPER(
                                        prototype->metaTable.metas[metaIndex]->function));
                    }
                }
                for (TZrSize descriptorIndex = 0u;
                     descriptorIndex < prototypeSnapshot->memberDescriptorCount;
                     descriptorIndex++) {
                    SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[descriptorIndex];
                    SZrRawObject *references[8] = {
                        descriptor->name == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->name),
                        descriptor->getterFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->getterFunction),
                        descriptor->setterFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->setterFunction),
                        descriptor->initializerFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->initializerFunction),
                        descriptor->methodFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->methodFunction),
                        descriptor->ownerTypeName == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->ownerTypeName),
                        descriptor->baseDefinitionOwnerTypeName == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->baseDefinitionOwnerTypeName),
                        descriptor->baseDefinitionName == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor->baseDefinitionName) };
                    for (TZrSize referenceIndex = 0u; referenceIndex < 8u; referenceIndex++) {
                        if (references[referenceIndex] != ZR_NULL) {
                            ZrCore_RawObject_Barrier(state, raw, references[referenceIndex]);
                        }
                    }
                }
                for (TZrSize dispatchIndex = 0u;
                     dispatchIndex < prototypeSnapshot->interfaceDispatchCount;
                     dispatchIndex++) {
                    SZrInterfaceDispatchEntry *entry = &prototype->interfaceDispatchEntries[dispatchIndex];
                    if (entry->interfacePrototype != ZR_NULL) {
                        ZrCore_RawObject_Barrier(state, raw,
                                ZR_CAST_RAW_OBJECT_AS_SUPER(entry->interfacePrototype));
                    }
                    if (entry->implementationPrototype != ZR_NULL) {
                        ZrCore_RawObject_Barrier(state, raw,
                                ZR_CAST_RAW_OBJECT_AS_SUPER(entry->implementationPrototype));
                    }
                }
                if (prototype->indexContract.getByIndexFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->indexContract.getByIndexFunction));
                }
                if (prototype->indexContract.setByIndexFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->indexContract.setByIndexFunction));
                }
                if (prototype->indexContract.containsKeyFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->indexContract.containsKeyFunction));
                }
                if (prototype->indexContract.getLengthFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->indexContract.getLengthFunction));
                }
                if (prototype->iterableContract.iterInitFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->iterableContract.iterInitFunction));
                }
                if (prototype->iteratorContract.moveNextFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->iteratorContract.moveNextFunction));
                }
                if (prototype->iteratorContract.currentFunction != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->iteratorContract.currentFunction));
                }
                if (prototype->iteratorContract.currentMemberName != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->iteratorContract.currentMemberName));
                }
                for (TZrSize fieldIndex = 0u;
                     fieldIndex < prototypeSnapshot->managedFieldCount;
                     fieldIndex++) {
                    if (prototype->managedFields[fieldIndex].name != ZR_NULL) {
                        ZrCore_RawObject_Barrier(state, raw,
                                ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->managedFields[fieldIndex].name));
                    }
                }
                if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                    SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;
                    for (TZrSize bucketIndex = 0u;
                         bucketIndex < structPrototype->keyOffsetMap.capacity;
                         bucketIndex++) {
                        for (SZrHashKeyValuePair *pair = structPrototype->keyOffsetMap.buckets[bucketIndex];
                             pair != ZR_NULL; pair = pair->next) {
                            ZrCore_Value_Barrier(state, raw, &pair->key);
                            ZrCore_Value_Barrier(state, raw, &pair->value);
                        }
                    }
                }
            }
            if (object->superArrayRawIntData != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global, object->superArrayRawIntData,
                        object->superArrayRawIntCapacity * sizeof(TZrInt64),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            object->superArrayRawIntData = prepared->rawIntData;
            prepared->rawIntData = ZR_NULL;
            object->superArrayRawIntLength = snapshot->hasRawIntData ? snapshot->rawIntLength : 0u;
            object->superArrayRawIntCapacity = snapshot->hasRawIntData ? snapshot->rawIntCapacity : 0u;
            object->superArrayStorageMode = snapshot->rawIntStorageMode;
            object->superArrayStorageGeneration = snapshot->rawIntStorageGeneration;
            object->superArrayRawIntDirty = snapshot->rawIntDirty;
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
            SZrClosure *closure = (SZrClosure *)raw;
            if (!closure->super.isNative) {
                closure->function = prepared->closureFunction;
                if (closure->function != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(closure->function));
                }
                for (TZrSize captureIndex = 0u; captureIndex < snapshot->captureCount; captureIndex++) {
                    closure->closureValuesExtend[captureIndex] =
                            (SZrClosureValue *)prepared->closureCaptures[captureIndex];
                    ZrCore_RawObject_Barrier(state, raw, prepared->closureCaptures[captureIndex]);
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE && snapshot->hasClosureValue) {
            SZrClosureValue *closureValue = (SZrClosureValue *)raw;
            SZrTypeValue *destination = ZrCore_ClosureValue_GetValue(closureValue);
            /* Preflight proved the destination has no ownership.  Preserve the
             * graph-indexed value and alias identity without invoking the
             * allocating CopySlow path after ResetThread. */
            *destination = prepared->closureValue;
            ZrCore_Value_Barrier(state, raw, destination);
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            SZrFunction *function = (SZrFunction *)raw;
            function->callBindingGeneration = snapshot->functionCallBindingGeneration;
            function->metadataCodeRegistration = snapshot->functionMetadataCodeRegistration;
            function->metadataTypeLayoutCount = snapshot->functionMetadataTypeLayoutCount;
            function->metadataGcDescriptorCount = snapshot->functionMetadataGcDescriptorCount;
            if (function->prototypeInstances != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, function->prototypeInstances,
                        function->prototypeInstancesLength * sizeof(*function->prototypeInstances));
            }
            function->cachedStatelessClosure = prepared->cachedStatelessClosure;
            if (function->cachedStatelessClosure != ZR_NULL) {
                ZrCore_RawObject_Barrier(state, raw,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(function->cachedStatelessClosure));
            }
            function->prototypeInstances = prepared->prototypeInstances;
            prepared->prototypeInstances = ZR_NULL;
            function->prototypeInstancesLength = (TZrUInt32)snapshot->prototypeInstanceCount;
            for (TZrSize instanceIndex = 0u;
                 instanceIndex < snapshot->prototypeInstanceCount;
                 instanceIndex++) {
                if (function->prototypeInstances[instanceIndex] != ZR_NULL) {
                    ZrCore_RawObject_Barrier(state, raw,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(function->prototypeInstances[instanceIndex]));
                }
            }
            for (TZrSize constantIndex = 0u;
                 constantIndex < snapshot->functionConstantCount; constantIndex++) {
                function->constantValueList[constantIndex] = prepared->functionConstants[constantIndex];
                ZrCore_Value_Barrier(state, raw, &function->constantValueList[constantIndex]);
            }
            if (snapshot->hasDecoratorMetadata) {
                function->hasDecoratorMetadata = ZR_TRUE;
                function->decoratorMetadataValue = prepared->decoratorMetadata;
                ZrCore_Value_Barrier(state, raw, &function->decoratorMetadataValue);
            } else {
                function->hasDecoratorMetadata = ZR_FALSE;
                ZrCore_Value_ResetAsNull(&function->decoratorMetadataValue);
            }
            for (TZrSize symbolIndex = 0u;
                 symbolIndex < snapshot->typedExportMetadataTokenCount; symbolIndex++) {
                function->typedExportedSymbols[symbolIndex].metadataToken =
                        snapshot->typedExportMetadataTokens[symbolIndex];
            }
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
    checkpoint_release_restore_plan(state, checkpoint, plan);
}
