#include "session_checkpoint_private.h"

static TZrBool checkpoint_capture_pairs(SZrSessionCheckpoint *checkpoint,
                                        SZrHashSet *set,
                                        ZrCheckpointPair **outPairs,
                                        TZrSize *outCount);

/* 所有快照侧数组统一检查倍增和字节乘法溢出；失败时保持原缓冲区。 */
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

/* 拒绝借用所有权、原生指针和线程值，防止逻辑快照假装拥有外部生命周期。 */
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

/* 白名单只接受没有资源终结义务的 VM 对象；扩展对象需自行定义恢复契约。 */
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
            if (raw->isNative || object->inlineArrayLayoutFunction != ZR_NULL ||
                object->inlineArrayElementLayoutHash != 0u ||
                object->inlineArrayObjectByteSize != 0u ||
                object->inlineArrayElementLayoutId != 0u ||
                object->inlineArrayElementByteOffset != 0u ||
                object->inlineArrayElementByteSize != 0u || object->inlineArrayLength != 0u ||
                object->internalType >= ZR_OBJECT_INTERNAL_TYPE_CUSTOM_EXTENSION_START) {
                return ZR_FALSE;
            }
            return raw->finalizerData == ZR_NULL;
        }
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE:
            return !raw->isNative && raw->finalizerData == ZR_NULL;
        case ZR_RAW_OBJECT_TYPE_CLOSURE:
            /* Native callback/shim/binding state is not represented by the
             * graph-indexed checkpoint contract, even for zero captures. */
            return !raw->isNative && raw->finalizerData == ZR_NULL;
        default:
            return ZR_FALSE;
    }
}

/* 通过 GC 句柄查重，环和别名都引用同一快照节点。 */
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

static TZrBool checkpoint_capture_optional_object_ref(SZrSessionCheckpoint *checkpoint,
                                                       SZrRawObject *raw,
                                                       TZrSize *outIndex) {
    if (outIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    *outIndex = ZR_CHECKPOINT_NO_INDEX;
    return raw == ZR_NULL || checkpoint_capture_object(checkpoint, raw, outIndex);
}

static TZrBool checkpoint_capture_prototype(SZrSessionCheckpoint *checkpoint,
                                             SZrObjectPrototype *prototype,
                                             TZrSize objectIndex) {
    ZrCheckpointPrototype *snapshot;
    TZrSize index;

    if (checkpoint == ZR_NULL || prototype == ZR_NULL ||
        prototype->type <= ZR_OBJECT_PROTOTYPE_TYPE_INVALID ||
        prototype->type >= ZR_OBJECT_PROTOTYPE_TYPE_MAX ||
        prototype->memberDescriptorCount > prototype->memberDescriptorCapacity ||
        prototype->managedFieldCount > prototype->managedFieldCapacity ||
        prototype->memberDescriptorCapacity >
                (TZrSize)-1 / sizeof(*prototype->memberDescriptors) ||
        prototype->interfaceDispatchCount >
                (TZrSize)-1 / sizeof(*prototype->interfaceDispatchEntries) ||
        prototype->managedFieldCapacity >
                (TZrSize)-1 / sizeof(*prototype->managedFields) ||
        ((prototype->memberDescriptorCapacity == 0u) !=
         (prototype->memberDescriptors == ZR_NULL)) ||
        (prototype->memberDescriptorCount > 0u && prototype->memberDescriptors == ZR_NULL) ||
        ((prototype->interfaceDispatchCount == 0u) !=
         (prototype->interfaceDispatchEntries == ZR_NULL)) ||
        ((prototype->managedFieldCapacity == 0u) !=
         (prototype->managedFields == ZR_NULL)) ||
        (prototype->managedFieldCount > 0u && prototype->managedFields == ZR_NULL)) {
        return ZR_FALSE;
    }
    snapshot = (ZrCheckpointPrototype *)calloc(1u, sizeof(*snapshot));
    if (snapshot == ZR_NULL) {
        return ZR_FALSE;
    }
    checkpoint->objects[objectIndex].prototype = snapshot;
    snapshot->nameIndex = ZR_CHECKPOINT_NO_INDEX;
    snapshot->superPrototypeIndex = ZR_CHECKPOINT_NO_INDEX;
    snapshot->iteratorMemberNameIndex = ZR_CHECKPOINT_NO_INDEX;
    snapshot->iterableFunctionIndex = ZR_CHECKPOINT_NO_INDEX;
    for (index = 0u; index < ZR_META_ENUM_MAX; index++) {
        snapshot->metaFunctionIndexes[index] = ZR_CHECKPOINT_NO_INDEX;
    }
    for (index = 0u; index < 4u; index++) {
        snapshot->indexFunctionIndexes[index] = ZR_CHECKPOINT_NO_INDEX;
    }
    for (index = 0u; index < 2u; index++) {
        snapshot->iteratorFunctionIndexes[index] = ZR_CHECKPOINT_NO_INDEX;
    }

    snapshot->type = prototype->type;
    snapshot->shapeId = prototype->shapeId;
    snapshot->shapeGeneration = prototype->shapeGeneration;
    snapshot->layoutGeneration = prototype->layoutGeneration;
    snapshot->memberDescriptorCount = prototype->memberDescriptorCount;
    snapshot->memberDescriptorCapacity = prototype->memberDescriptorCapacity;
    snapshot->interfaceDispatchCount = prototype->interfaceDispatchCount;
    snapshot->protocolMask = prototype->protocolMask;
    snapshot->dynamicMemberCapable = prototype->dynamicMemberCapable;
    snapshot->reserved0 = prototype->reserved0;
    snapshot->reserved1 = prototype->reserved1;
    snapshot->modifierFlags = prototype->modifierFlags;
    snapshot->nextVirtualSlotIndex = prototype->nextVirtualSlotIndex;
    snapshot->nextPropertyIdentity = prototype->nextPropertyIdentity;
    snapshot->layoutByteSize = prototype->layoutByteSize;
    snapshot->layoutByteAlign = prototype->layoutByteAlign;
    snapshot->managedFieldCount = prototype->managedFieldCount;
    snapshot->managedFieldCapacity = prototype->managedFieldCapacity;

    if (!checkpoint_capture_optional_object_ref(checkpoint,
            prototype->name == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->name),
            &snapshot->nameIndex) ||
        !checkpoint_capture_optional_object_ref(checkpoint,
            prototype->superPrototype == ZR_NULL ? ZR_NULL :
                ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->superPrototype),
            &snapshot->superPrototypeIndex)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < ZR_META_ENUM_MAX; index++) {
        const SZrMeta *meta = prototype->metaTable.metas[index];
        if (meta == ZR_NULL) {
            continue;
        }
        snapshot->hasMeta[index] = ZR_TRUE;
        snapshot->metaTypes[index] = meta->metaType;
        if (!checkpoint_capture_optional_object_ref(checkpoint,
                meta->function == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function),
                &snapshot->metaFunctionIndexes[index])) {
            return ZR_FALSE;
        }
    }

    if (snapshot->memberDescriptorCount > 0u) {
        if (snapshot->memberDescriptorCount > (TZrSize)-1 / sizeof(*snapshot->memberDescriptors)) {
            return ZR_FALSE;
        }
        snapshot->memberDescriptors = (ZrCheckpointMemberDescriptor *)calloc(
                snapshot->memberDescriptorCount, sizeof(*snapshot->memberDescriptors));
        if (snapshot->memberDescriptors == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0u; index < snapshot->memberDescriptorCount; index++) {
            const SZrMemberDescriptor *source = &prototype->memberDescriptors[index];
            ZrCheckpointMemberDescriptor *descriptor = &snapshot->memberDescriptors[index];
            SZrRawObject *references[8];
            descriptor->value = *source;
            references[0] = source->name == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(source->name);
            references[1] = source->getterFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(source->getterFunction);
            references[2] = source->setterFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(source->setterFunction);
            references[3] = source->initializerFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(source->initializerFunction);
            references[4] = source->methodFunction == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(source->methodFunction);
            references[5] = source->ownerTypeName == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(source->ownerTypeName);
            references[6] = source->baseDefinitionOwnerTypeName == ZR_NULL ? ZR_NULL :
                    ZR_CAST_RAW_OBJECT_AS_SUPER(source->baseDefinitionOwnerTypeName);
            references[7] = source->baseDefinitionName == ZR_NULL ? ZR_NULL :
                    ZR_CAST_RAW_OBJECT_AS_SUPER(source->baseDefinitionName);
            descriptor->value.name = ZR_NULL;
            descriptor->value.getterFunction = ZR_NULL;
            descriptor->value.setterFunction = ZR_NULL;
            descriptor->value.initializerFunction = ZR_NULL;
            descriptor->value.methodFunction = ZR_NULL;
            descriptor->value.ownerTypeName = ZR_NULL;
            descriptor->value.baseDefinitionOwnerTypeName = ZR_NULL;
            descriptor->value.baseDefinitionName = ZR_NULL;
            for (TZrSize referenceIndex = 0u; referenceIndex < 8u; referenceIndex++) {
                descriptor->objectIndexes[referenceIndex] = ZR_CHECKPOINT_NO_INDEX;
                if (!checkpoint_capture_optional_object_ref(checkpoint, references[referenceIndex],
                        &descriptor->objectIndexes[referenceIndex])) {
                    return ZR_FALSE;
                }
            }
        }
    }

    if (snapshot->interfaceDispatchCount > 0u) {
        if (snapshot->interfaceDispatchCount > (TZrSize)-1 / sizeof(*snapshot->interfaceDispatchEntries)) {
            return ZR_FALSE;
        }
        snapshot->interfaceDispatchEntries = (ZrCheckpointInterfaceDispatchEntry *)calloc(
                snapshot->interfaceDispatchCount, sizeof(*snapshot->interfaceDispatchEntries));
        if (snapshot->interfaceDispatchEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0u; index < snapshot->interfaceDispatchCount; index++) {
            const SZrInterfaceDispatchEntry *source = &prototype->interfaceDispatchEntries[index];
            ZrCheckpointInterfaceDispatchEntry *entry = &snapshot->interfaceDispatchEntries[index];
            entry->value = *source;
            entry->value.interfacePrototype = ZR_NULL;
            entry->value.implementationPrototype = ZR_NULL;
            if (!checkpoint_capture_optional_object_ref(checkpoint,
                    source->interfacePrototype == ZR_NULL ? ZR_NULL :
                        ZR_CAST_RAW_OBJECT_AS_SUPER(source->interfacePrototype),
                    &entry->interfacePrototypeIndex) ||
                !checkpoint_capture_optional_object_ref(checkpoint,
                    source->implementationPrototype == ZR_NULL ? ZR_NULL :
                        ZR_CAST_RAW_OBJECT_AS_SUPER(source->implementationPrototype),
                    &entry->implementationPrototypeIndex)) {
                return ZR_FALSE;
            }
        }
    }

    snapshot->indexContract = prototype->indexContract;
    snapshot->indexContract.getByIndexFunction = ZR_NULL;
    snapshot->indexContract.setByIndexFunction = ZR_NULL;
    snapshot->indexContract.containsKeyFunction = ZR_NULL;
    snapshot->indexContract.getLengthFunction = ZR_NULL;
    snapshot->indexContract.getByIndexKnownNativeCallable = ZR_NULL;
    snapshot->indexContract.setByIndexKnownNativeCallable = ZR_NULL;
    snapshot->indexContract.getByIndexKnownNativeFunction = ZR_NULL;
    snapshot->indexContract.setByIndexKnownNativeFunction = ZR_NULL;
    memset(&snapshot->indexContract.getByIndexKnownNativeDirectDispatch, 0,
           sizeof(snapshot->indexContract.getByIndexKnownNativeDirectDispatch));
    memset(&snapshot->indexContract.setByIndexKnownNativeDirectDispatch, 0,
           sizeof(snapshot->indexContract.setByIndexKnownNativeDirectDispatch));
    {
        SZrFunction *functions[4] = { prototype->indexContract.getByIndexFunction,
            prototype->indexContract.setByIndexFunction,
            prototype->indexContract.containsKeyFunction,
            prototype->indexContract.getLengthFunction };
        for (index = 0u; index < 4u; index++) {
            if (!checkpoint_capture_optional_object_ref(checkpoint,
                    functions[index] == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(functions[index]),
                    &snapshot->indexFunctionIndexes[index])) {
                return ZR_FALSE;
            }
        }
    }
    snapshot->iterableContract = prototype->iterableContract;
    snapshot->iterableContract.iterInitFunction = ZR_NULL;
    if (!checkpoint_capture_optional_object_ref(checkpoint,
            prototype->iterableContract.iterInitFunction == ZR_NULL ? ZR_NULL :
                ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->iterableContract.iterInitFunction),
            &snapshot->iterableFunctionIndex)) {
        return ZR_FALSE;
    }
    snapshot->iteratorContract = prototype->iteratorContract;
    snapshot->iteratorContract.moveNextFunction = ZR_NULL;
    snapshot->iteratorContract.currentFunction = ZR_NULL;
    snapshot->iteratorContract.currentMemberName = ZR_NULL;
    {
        SZrFunction *functions[2] = { prototype->iteratorContract.moveNextFunction,
                                       prototype->iteratorContract.currentFunction };
        for (index = 0u; index < 2u; index++) {
            if (!checkpoint_capture_optional_object_ref(checkpoint,
                    functions[index] == ZR_NULL ? ZR_NULL : ZR_CAST_RAW_OBJECT_AS_SUPER(functions[index]),
                    &snapshot->iteratorFunctionIndexes[index])) {
                return ZR_FALSE;
            }
        }
    }
    if (!checkpoint_capture_optional_object_ref(checkpoint,
            prototype->iteratorContract.currentMemberName == ZR_NULL ? ZR_NULL :
                ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->iteratorContract.currentMemberName),
            &snapshot->iteratorMemberNameIndex)) {
        return ZR_FALSE;
    }

    if (snapshot->managedFieldCount > 0u) {
        if (snapshot->managedFieldCount > (TZrSize)-1 / sizeof(*snapshot->managedFields)) {
            return ZR_FALSE;
        }
        snapshot->managedFields = (ZrCheckpointManagedField *)calloc(
                snapshot->managedFieldCount, sizeof(*snapshot->managedFields));
        if (snapshot->managedFields == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0u; index < snapshot->managedFieldCount; index++) {
            snapshot->managedFields[index].value = prototype->managedFields[index];
            snapshot->managedFields[index].value.name = ZR_NULL;
            snapshot->managedFields[index].nameIndex = ZR_CHECKPOINT_NO_INDEX;
            if (!checkpoint_capture_optional_object_ref(checkpoint,
                    prototype->managedFields[index].name == ZR_NULL ? ZR_NULL :
                        ZR_CAST_RAW_OBJECT_AS_SUPER(prototype->managedFields[index].name),
                    &snapshot->managedFields[index].nameIndex)) {
                return ZR_FALSE;
            }
        }
    }

    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        const SZrStructPrototype *structPrototype = (const SZrStructPrototype *)prototype;
        snapshot->keyOffsetMapValid = structPrototype->keyOffsetMap.isValid;
        if (!checkpoint_capture_pairs(checkpoint, &structPrototype->keyOffsetMap,
                                      &snapshot->keyOffsetPairs,
                                      &snapshot->keyOffsetPairCount)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 函数体本身不克隆；遍历其引用以保留仍由常量、子函数和缓存指向的对象。 */
static TZrBool checkpoint_capture_function_refs(SZrSessionCheckpoint *checkpoint,
                                                 SZrFunction *function,
                                                 TZrSize objectIndex) {
    TZrSize ignored;
    TZrUInt32 index;
    TZrSize *prototypeInstanceIndexes;
    TZrSize cachedStatelessClosureIndex = ZR_CHECKPOINT_NO_INDEX;

    if (function->constantValueLength > 0u) {
        checkpoint->objects[objectIndex].functionConstants =
                (ZrCheckpointValue *)calloc(function->constantValueLength,
                                             sizeof(*checkpoint->objects[objectIndex].functionConstants));
        if (checkpoint->objects[objectIndex].functionConstants == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    checkpoint->objects[objectIndex].functionConstantCount = function->constantValueLength;
    for (index = 0u; index < function->constantValueLength; index++) {
        ZrCheckpointValue captured;
        if (!checkpoint_capture_value(checkpoint,
                                      &function->constantValueList[index],
                                      &captured)) {
            return ZR_FALSE;
        }
        checkpoint->objects[objectIndex].functionConstants[index] = captured;
    }
    if (function->typedExportedSymbolLength > 0u) {
        if (function->typedExportedSymbols == ZR_NULL) {
            return ZR_FALSE;
        }
        checkpoint->objects[objectIndex].typedExportMetadataTokens =
                (TZrUInt32 *)calloc(function->typedExportedSymbolLength,
                                    sizeof(*checkpoint->objects[objectIndex].typedExportMetadataTokens));
        if (checkpoint->objects[objectIndex].typedExportMetadataTokens == ZR_NULL) {
            return ZR_FALSE;
        }
        checkpoint->objects[objectIndex].typedExportMetadataTokenCount =
                function->typedExportedSymbolLength;
        for (index = 0u; index < function->typedExportedSymbolLength; index++) {
            checkpoint->objects[objectIndex].typedExportMetadataTokens[index] =
                    function->typedExportedSymbols[index].metadataToken;
        }
    }
    if (function->childFunctionLength > 0u &&
        (function->childFunctionList == ZR_NULL ||
         function->childFunctionLength > (TZrSize)-1 / sizeof(*function->childFunctionList))) {
        return ZR_FALSE;
    }
    if (function->ownerFunction != ZR_NULL &&
        !checkpoint_capture_object(checkpoint,
                                   ZR_CAST_RAW_OBJECT_AS_SUPER(function->ownerFunction),
                                   &ignored)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->childFunctionLength; index++) {
        if (!checkpoint_capture_object(checkpoint,
                                       ZR_CAST_RAW_OBJECT_AS_SUPER(&function->childFunctionList[index]),
                                       &ignored)) {
            return ZR_FALSE;
        }
    }
    checkpoint->objects[objectIndex].hasCachedStatelessClosure =
            function->cachedStatelessClosure != ZR_NULL;
    if (checkpoint->objects[objectIndex].hasCachedStatelessClosure &&
        !checkpoint_capture_object(checkpoint,
                                   ZR_CAST_RAW_OBJECT_AS_SUPER(function->cachedStatelessClosure),
                                   &cachedStatelessClosureIndex)) {
        return ZR_FALSE;
    }
    checkpoint->objects[objectIndex].cachedStatelessClosureIndex = cachedStatelessClosureIndex;
    checkpoint->objects[objectIndex].prototypeInstanceCount = function->prototypeInstancesLength;
    if ((function->prototypeInstancesLength == 0u) != (function->prototypeInstances == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (function->prototypeInstancesLength > 0u) {
        if (function->prototypeInstancesLength > (TZrSize)-1 / sizeof(*prototypeInstanceIndexes)) {
            return ZR_FALSE;
        }
        prototypeInstanceIndexes = (TZrSize *)malloc(
                function->prototypeInstancesLength * sizeof(*prototypeInstanceIndexes));
        if (prototypeInstanceIndexes == ZR_NULL) {
            return ZR_FALSE;
        }
        checkpoint->objects[objectIndex].prototypeInstanceIndexes = prototypeInstanceIndexes;
        for (index = 0u; index < function->prototypeInstancesLength; index++) {
            prototypeInstanceIndexes[index] = ZR_CHECKPOINT_NO_INDEX;
            if (function->prototypeInstances[index] != ZR_NULL &&
                !checkpoint_capture_object(checkpoint,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(function->prototypeInstances[index]),
                    &ignored)) {
                return ZR_FALSE;
            }
            if (function->prototypeInstances[index] != ZR_NULL) {
                prototypeInstanceIndexes[index] = ignored;
            }
        }
    }
    if (function->hasDecoratorMetadata) {
        ZrCheckpointValue captured;
        if (!checkpoint_capture_value(checkpoint, &function->decoratorMetadataValue, &captured)) {
            return ZR_FALSE;
        }
        checkpoint->objects[objectIndex].hasDecoratorMetadata = ZR_TRUE;
        checkpoint->objects[objectIndex].decoratorMetadata = captured;
    }
    return ZR_TRUE;
}

/* 标量按值保存；GC 指针交给对象图索引和句柄，避免直接保存移动中的地址。 */
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

/* 按实际桶链保存键值；失败时本地释放未挂入快照的数组，已登记对象根由 Create 清理。 */
static TZrBool checkpoint_capture_pairs(SZrSessionCheckpoint *checkpoint,
                                        SZrHashSet *set,
                                        ZrCheckpointPair **outPairs,
                                        TZrSize *outCount) {
    TZrSize bucketIndex;
    TZrSize count = 0u;
    TZrSize poolCapacity = 0u;
    TZrSize poolUsed = 0u;
    TZrSize poolSteps = 0u;
    TZrSize activePoolPairCount = 0u;
    const SZrHashPairPoolBlock *block;
    TZrBool activeFound = ZR_FALSE;
    TZrBool tailFound = ZR_FALSE;
    ZrCheckpointPair *pairs = ZR_NULL;

    if (set == ZR_NULL || outPairs == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!set->isValid) {
        if (set->buckets != ZR_NULL || set->bucketSize != 0u || set->capacity != 0u ||
            set->elementCount != 0u || set->pairPoolHead != ZR_NULL ||
            set->pairPoolActive != ZR_NULL || set->pairPoolTail != ZR_NULL ||
            set->pairPoolCapacity != 0u || set->pairPoolUsed != 0u) {
            return ZR_FALSE;
        }
        *outPairs = ZR_NULL;
        *outCount = 0u;
        return ZR_TRUE;
    }
    if (set->capacity == 0u || (set->capacity & (set->capacity - 1u)) != 0u ||
        set->buckets == ZR_NULL ||
        set->capacity > (TZrSize)-1 / sizeof(*set->buckets) ||
        set->bucketSize != set->capacity * sizeof(*set->buckets) ||
        set->pairPoolUsed > set->pairPoolCapacity ||
        (set->pairPoolCapacity == 0u &&
         (set->pairPoolHead != ZR_NULL || set->pairPoolActive != ZR_NULL ||
          set->pairPoolTail != ZR_NULL || set->pairPoolUsed != 0u)) ||
        (set->pairPoolCapacity > 0u &&
         (set->pairPoolHead == ZR_NULL || set->pairPoolActive == ZR_NULL ||
          set->pairPoolTail == ZR_NULL))) {
        return ZR_FALSE;
    }
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        TZrSize extraPairCapacity;
        if (poolSteps++ >= set->pairPoolCapacity || block->capacity == 0u ||
            block->used > block->capacity) {
            return ZR_FALSE;
        }
        extraPairCapacity = block->capacity > ZR_HASH_PAIR_POOL_INLINE_COUNT
                ? block->capacity - ZR_HASH_PAIR_POOL_INLINE_COUNT : 0u;
        if (extraPairCapacity > ((TZrSize)-1 - sizeof(*block)) /
                                  sizeof(SZrHashKeyValuePair) ||
            poolCapacity > (TZrSize)-1 - block->capacity ||
            poolUsed > (TZrSize)-1 - block->used) {
            return ZR_FALSE;
        }
        poolCapacity += block->capacity;
        poolUsed += block->used;
        if (block == set->pairPoolActive) {
            activeFound = ZR_TRUE;
        }
        if (block == set->pairPoolTail) {
            tailFound = ZR_TRUE;
            if (block->next != ZR_NULL) {
                return ZR_FALSE;
            }
        }
        if (poolCapacity > set->pairPoolCapacity || poolUsed > set->pairPoolUsed) {
            return ZR_FALSE;
        }
    }
    if (poolCapacity != set->pairPoolCapacity || poolUsed != set->pairPoolUsed ||
        (set->pairPoolCapacity > 0u && (!activeFound || !tailFound))) {
        return ZR_FALSE;
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
            TZrBool pairInPool = ZR_FALSE;
            if (!checkpoint_hash_pair_is_valid_pool_member(set, pair, &pairInPool) ||
                (pairInPool && activePoolPairCount >= set->pairPoolUsed) ||
                count >= set->elementCount ||
                !checkpoint_capture_value(checkpoint, &pair->key, &pairs[count].key) ||
                !checkpoint_capture_value(checkpoint, &pair->value, &pairs[count].value)) {
                free(pairs);
                return ZR_FALSE;
            }
            if (pairInPool) {
                activePoolPairCount++;
            }
            count++;
            pair = pair->next;
        }
    }
    if (count != set->elementCount) {
        free(pairs);
        return ZR_FALSE;
    }
    *outPairs = pairs;
    *outCount = count;
    return ZR_TRUE;
}

/* 先登记节点和 GC 根再递归其边，允许模块、原型及闭包图中存在环。 */
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
    snapshot->cachedStatelessClosureIndex = ZR_CHECKPOINT_NO_INDEX;
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
        checkpoint->objects[index].nodeMapValid = object->nodeMap.isValid;
        if (object->prototype != ZR_NULL) {
            TZrSize prototypeIndex;
            if (!checkpoint_capture_object(checkpoint,
                                           ZR_CAST_RAW_OBJECT_AS_SUPER(object->prototype),
                                           &prototypeIndex)) {
                return ZR_FALSE;
            }
            checkpoint->objects[index].prototypeIndex = prototypeIndex;
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
            checkpoint->objects[index].moduleReserved0 = module->reserved0;
            checkpoint->objects[index].moduleReserved1 = module->reserved1;
            checkpoint->objects[index].moduleMetadataGeneration = module->metadataGeneration;
            checkpoint->objects[index].modulePathHash = module->pathHash;
            checkpoint->objects[index].moduleHasMetadataRuntime = module->hasMetadataRuntime;
            checkpoint->objects[index].proNodeMapValid = module->proNodeMap.isValid;
            checkpoint->objects[index].metadataRuntime = module->metadataRuntime;
            checkpoint->objects[index].metadataRuntime.module = ZR_NULL;
            checkpoint->objects[index].metadataRuntime.metadataFunction = ZR_NULL;
            if (module->moduleName != ZR_NULL) {
                TZrSize moduleNameIndex;
                if (!checkpoint_capture_object(checkpoint,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(module->moduleName), &moduleNameIndex)) {
                    return ZR_FALSE;
                }
                checkpoint->objects[index].moduleNameIndex = moduleNameIndex;
                checkpoint->objects[index].hasModuleName = ZR_TRUE;
            }
            if (module->fullPath != ZR_NULL) {
                TZrSize fullPathIndex;
                if (!checkpoint_capture_object(checkpoint,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(module->fullPath), &fullPathIndex)) {
                    return ZR_FALSE;
                }
                checkpoint->objects[index].fullPathIndex = fullPathIndex;
                checkpoint->objects[index].hasFullPath = ZR_TRUE;
            }
            if (module->metadataRuntime.metadataFunction != ZR_NULL) {
                TZrSize metadataFunctionIndex;
                if (!checkpoint_capture_object(checkpoint,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(module->metadataRuntime.metadataFunction),
                        &metadataFunctionIndex)) {
                    return ZR_FALSE;
                }
                checkpoint->objects[index].metadataFunctionIndex = metadataFunctionIndex;
                checkpoint->objects[index].hasMetadataFunction = ZR_TRUE;
            }
            if (!checkpoint_capture_pairs(checkpoint, &module->proNodeMap,
                                          &capturedPairs, &capturedPairCount)) {
                return ZR_FALSE;
            }
            checkpoint->objects[index].proPairs = capturedPairs;
            checkpoint->objects[index].proPairCount = capturedPairCount;
            if (((module->exportDescriptorLength == 0u) !=
                 (module->exportDescriptors == ZR_NULL)) ||
                module->exportDescriptorLength > (TZrSize)-1 / sizeof(*module->exportDescriptors)) {
                return ZR_FALSE;
            }
            checkpoint->objects[index].descriptorCount = module->exportDescriptorLength;
            if (module->exportDescriptorLength > 0u) {
                checkpoint->objects[index].descriptors =
                        (ZrCheckpointModuleDescriptor *)calloc(module->exportDescriptorLength,
                            sizeof(*checkpoint->objects[index].descriptors));
                if (checkpoint->objects[index].descriptors == ZR_NULL ||
                    module->exportDescriptors == ZR_NULL) {
                    return ZR_FALSE;
                }
                for (capacity = 0u; capacity < module->exportDescriptorLength; capacity++) {
                    const SZrModuleExportDescriptor *source = &module->exportDescriptors[capacity];
                    ZrCheckpointModuleDescriptor *descriptor =
                            &checkpoint->objects[index].descriptors[capacity];
                    descriptor->accessModifier = source->accessModifier;
                    descriptor->exportKind = source->exportKind;
                    descriptor->readiness = source->readiness;
                    descriptor->isReady = source->isReady;
                    if (source->name != ZR_NULL) {
                        TZrSize nameIndex;
                        if (!checkpoint_capture_object(checkpoint,
                                ZR_CAST_RAW_OBJECT_AS_SUPER(source->name), &nameIndex)) {
                            return ZR_FALSE;
                        }
                        checkpoint->objects[index].descriptors[capacity].nameIndex = nameIndex;
                        checkpoint->objects[index].descriptors[capacity].hasName = ZR_TRUE;
                    }
                }
            }
        } else if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
            if (!checkpoint_capture_prototype(checkpoint,
                                              (SZrObjectPrototype *)object,
                                              index)) {
                return ZR_FALSE;
            }
        }
        if (((object->superArrayRawIntCapacity == 0u) !=
             (object->superArrayRawIntData == ZR_NULL)) ||
            object->superArrayRawIntLength > object->superArrayRawIntCapacity ||
            object->superArrayRawIntCapacity > (TZrSize)-1 / sizeof(TZrInt64)) {
            return ZR_FALSE;
        }
        if (object->superArrayRawIntData != ZR_NULL) {
            snapshot = &checkpoint->objects[index];
            snapshot->hasRawIntData = ZR_TRUE;
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
        checkpoint->objects[index].rawIntDirty = object->superArrayRawIntDirty;
    } else if (raw->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        SZrFunction *function = (SZrFunction *)raw;
        checkpoint->objects[index].functionCallBindingGeneration = function->callBindingGeneration;
        checkpoint->objects[index].functionMetadataCodeRegistration = function->metadataCodeRegistration;
        checkpoint->objects[index].functionMetadataTypeLayoutCount = function->metadataTypeLayoutCount;
        checkpoint->objects[index].functionMetadataGcDescriptorCount = function->metadataGcDescriptorCount;
        if ((function->constantValueLength > 0u && function->constantValueList == ZR_NULL) ||
            !checkpoint_capture_function_refs(checkpoint, function, index)) {
            return ZR_FALSE;
        }
    } else if (raw->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        if (!raw->isNative) {
            SZrClosure *closure = (SZrClosure *)raw;
            TZrSize captureCount = closure->closureValueCount;
            checkpoint->objects[index].captureCount = captureCount;
            snapshot = &checkpoint->objects[index];
            if (captureCount > 0u) {
                if (captureCount > (TZrSize)-1 / sizeof(*snapshot->captureIndexes) ||
                    closure->closureValuesExtend == ZR_NULL) {
                    return ZR_FALSE;
                }
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
            if (closure->function != ZR_NULL) {
                TZrSize functionIndex;
                if (!checkpoint_capture_object(checkpoint,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(closure->function), &functionIndex)) {
                    return ZR_FALSE;
                }
                checkpoint->objects[index].functionIndex = functionIndex;
            }
            checkpoint->objects[index].hasFunction = closure->function != ZR_NULL;
        }
    } else if (raw->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
        SZrClosureValue *closureValue = (SZrClosureValue *)raw;
        SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
        ZrCheckpointValue capturedValue;
        if (!ZrCore_ClosureValue_IsClosed(closureValue) || value == ZR_NULL ||
            !checkpoint_capture_value(checkpoint, value, &capturedValue)) {
            return ZR_FALSE;
        }
        checkpoint->objects[index].closureValue = capturedValue;
        checkpoint->objects[index].hasClosureValue = ZR_TRUE;
    }
    *outIndex = index;
    return ZR_TRUE;
}

/* 恢复时重新解析句柄，既保留原对象身份也兼容快照期间的 GC 移动。 */

TZrBool ZrCore_SessionCheckpoint_Create(SZrState *state, SZrSessionCheckpoint **outCheckpoint) {
    SZrSessionCheckpoint *checkpoint;
    SZrGcDomainPauseDiagnostic pauseDiagnostic;
    TZrBool captured;

    if (state == ZR_NULL || state->global == ZR_NULL || outCheckpoint == ZR_NULL ||
        !checkpoint_state_is_clean_create_boundary(state)) {
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
        free(checkpoint->objects[index].descriptors);
        free(checkpoint->objects[index].functionConstants);
        free(checkpoint->objects[index].typedExportMetadataTokens);
        free(checkpoint->objects[index].rawIntData);
        free(checkpoint->objects[index].prototypeInstanceIndexes);
        if (checkpoint->objects[index].prototype != ZR_NULL) {
            free(checkpoint->objects[index].prototype->memberDescriptors);
            free(checkpoint->objects[index].prototype->interfaceDispatchEntries);
            free(checkpoint->objects[index].prototype->managedFields);
            free(checkpoint->objects[index].prototype->keyOffsetPairs);
            free(checkpoint->objects[index].prototype);
        }
    }
    free(checkpoint->objects);
    free(checkpoint);
}
