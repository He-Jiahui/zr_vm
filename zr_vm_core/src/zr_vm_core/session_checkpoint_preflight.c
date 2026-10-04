#include "session_checkpoint_private.h"

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

static TZrSize checkpoint_hash_resize_threshold(TZrSize capacity) {
    return (capacity / ZR_HASH_SET_MAX_LOAD_DENOMINATOR) * ZR_HASH_SET_MAX_LOAD_NUMERATOR +
           ((capacity % ZR_HASH_SET_MAX_LOAD_DENOMINATOR) * ZR_HASH_SET_MAX_LOAD_NUMERATOR) /
                   ZR_HASH_SET_MAX_LOAD_DENOMINATOR;
}

static TZrBool checkpoint_hash_capacity(TZrSize elementCount, TZrSize *outCapacity) {
    TZrSize capacity = (TZrSize)1u << ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2;
    const TZrSize maximum = (TZrSize)-1;

    if (outCapacity == ZR_NULL) {
        return ZR_FALSE;
    }
    while (checkpoint_hash_resize_threshold(capacity) < elementCount) {
        if (capacity > maximum / 2u) {
            return ZR_FALSE;
        }
        capacity *= 2u;
    }
    if (capacity > maximum / sizeof(SZrHashKeyValuePair *)) {
        return ZR_FALSE;
    }
    *outCapacity = capacity;
    return ZR_TRUE;
}

/* 只用不触发 GC 的原生分配准备集合；提交前所有失败都只释放暂存集合。 */
static TZrBool checkpoint_prepare_map_storage(SZrGlobalState *global,
                                               SZrHashSet *set,
                                               TZrSize pairCount) {
    TZrSize capacity;
    TZrSize bucketBytes;
    TZrSize extraPairs;
    TZrSize poolBytes;
    SZrHashPairPoolBlock *pool;

    if (global == ZR_NULL || set == ZR_NULL || !checkpoint_hash_capacity(pairCount, &capacity)) {
        return ZR_FALSE;
    }
    ZrCore_HashSet_Construct(set);
    bucketBytes = capacity * sizeof(SZrHashKeyValuePair *);
    set->buckets = (SZrHashKeyValuePair **)ZrCore_Memory_RawMallocWithType(
            global, bucketBytes, ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
    if (set->buckets == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(set->buckets, 0, bucketBytes);
    set->bucketSize = bucketBytes;
    set->capacity = capacity;
    set->resizeThreshold = checkpoint_hash_resize_threshold(capacity);

    if (pairCount > 0u) {
        extraPairs = pairCount > ZR_HASH_PAIR_POOL_INLINE_COUNT
                ? pairCount - ZR_HASH_PAIR_POOL_INLINE_COUNT : 0u;
        if (extraPairs > ((TZrSize)-1 - sizeof(SZrHashPairPoolBlock)) /
                              sizeof(SZrHashKeyValuePair)) {
            return ZR_FALSE;
        }
        poolBytes = sizeof(SZrHashPairPoolBlock) + extraPairs * sizeof(SZrHashKeyValuePair);
        pool = (SZrHashPairPoolBlock *)ZrCore_Memory_RawMallocWithType(
                global, poolBytes, ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
        if (pool == ZR_NULL) {
            return ZR_FALSE;
        }
        pool->next = ZR_NULL;
        pool->capacity = pairCount;
        pool->used = 0u;
        set->pairPoolHead = pool;
        set->pairPoolActive = pool;
        set->pairPoolTail = pool;
        set->pairPoolCapacity = pairCount;
    }
    set->isValid = ZR_TRUE;
    return ZR_TRUE;
}

/* 暂存桶已按恢复后地址重算，且取 pair 不分配；此 helper 不可触发 GC。 */
static TZrBool checkpoint_fill_prepared_map(SZrState *state,
                                           const SZrSessionCheckpoint *checkpoint,
                                           SZrHashSet *set,
                                           const ZrCheckpointPair *pairs,
                                           TZrSize pairCount) {
    TZrSize index;

    if (pairCount > 0u && (set == ZR_NULL || !set->isValid ||
                          set->pairPoolCapacity < pairCount)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < pairCount; index++) {
        SZrTypeValue key;
        SZrTypeValue value;
        SZrHashKeyValuePair *pair;
        TZrUInt64 hash;
        TZrSize bucketIndex;

        if (!checkpoint_restore_value(state, checkpoint, &pairs[index].key, &key) ||
            !checkpoint_restore_value(state, checkpoint, &pairs[index].value, &value)) {
            return ZR_FALSE;
        }
        pair = ZrCore_HashSet_TakeReservedPairAssumeAvailable(set);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        pair->key = key;
        pair->value = value;
        hash = ZrCore_Value_GetHash(state, &pair->key);
        bucketIndex = ZR_HASH_MOD(hash, set->capacity);
        pair->next = set->buckets[bucketIndex];
        set->buckets[bucketIndex] = pair;
        set->elementCount++;
    }
    return ZR_TRUE;
}

void checkpoint_release_restore_plan(SZrState *state,
                                             const SZrSessionCheckpoint *checkpoint,
                                             ZrCheckpointRestorePlan *plan) {
    if (state == ZR_NULL || checkpoint == ZR_NULL || plan == ZR_NULL) {
        free(plan);
        return;
    }
    for (TZrSize index = 0u; index < checkpoint->objectCount; index++) {
        ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        ZrCheckpointRestorePlan *prepared = &plan[index];

        ZrCore_HashSet_Deconstruct(state, &prepared->nodeMap);
        ZrCore_HashSet_Deconstruct(state, &prepared->proNodeMap);
        ZrCore_HashSet_Deconstruct(state, &prepared->prototypeKeyOffsetMap);
        if (prepared->descriptors != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                    prepared->descriptors,
                    snapshot->descriptorCount * sizeof(*prepared->descriptors),
                    ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (prepared->rawIntData != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                    prepared->rawIntData,
                    snapshot->rawIntCapacity * sizeof(*prepared->rawIntData),
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (snapshot->prototype != ZR_NULL) {
            if (prepared->prototypeMemberDescriptors != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                        prepared->prototypeMemberDescriptors,
                        snapshot->prototype->memberDescriptorCapacity *
                            sizeof(*prepared->prototypeMemberDescriptors),
                        ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (prepared->prototypeInterfaceDispatchEntries != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global,
                        prepared->prototypeInterfaceDispatchEntries,
                        snapshot->prototype->interfaceDispatchCount *
                            sizeof(*prepared->prototypeInterfaceDispatchEntries));
            }
            if (prepared->prototypeManagedFields != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                        prepared->prototypeManagedFields,
                        snapshot->prototype->managedFieldCapacity *
                            sizeof(*prepared->prototypeManagedFields),
                        ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            for (TZrSize metaIndex = 0u; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
                if (prepared->prototypeMetas[metaIndex] != ZR_NULL) {
                    state->global->allocator(state->global->userAllocationArguments,
                            prepared->prototypeMetas[metaIndex], sizeof(SZrMeta), 0u,
                            ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                }
            }
        }
        if (prepared->prototypeInstances != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, prepared->prototypeInstances,
                    snapshot->prototypeInstanceCount * sizeof(*prepared->prototypeInstances));
        }
        free(prepared->functionConstants);
        free(prepared->closureCaptures);
    }
    free(plan);
}

static TZrBool checkpoint_resolve_plan_object(const ZrCheckpointRestorePlan *plan,
                                              TZrSize objectCount,
                                              TZrSize objectIndex,
                                              SZrRawObject **outObject) {
    if (plan == ZR_NULL || outObject == ZR_NULL || objectIndex >= objectCount ||
        plan[objectIndex].object == ZR_NULL) {
        return ZR_FALSE;
    }
    *outObject = plan[objectIndex].object;
    return ZR_TRUE;
}



static TZrBool checkpoint_live_value_has_no_ownership(const SZrTypeValue *value) {
    return (TZrBool)(value != ZR_NULL &&
                     value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
                     value->ownershipControl == ZR_NULL &&
                     value->ownershipWeakRef == ZR_NULL);
}

/* Deconstructing a replaced map does not release owned pair slots. Refuse before commit. */
static TZrBool checkpoint_live_map_has_only_unowned_values(const SZrHashSet *set) {
    TZrSize activePairCount = 0u;
    TZrSize activePoolPairCount = 0u;
    TZrSize poolCapacity = 0u;
    TZrSize poolUsed = 0u;
    TZrSize poolSteps = 0u;
    const SZrHashPairPoolBlock *block;
    TZrBool activeFound = ZR_FALSE;
    TZrBool tailFound = ZR_FALSE;

    if (set == ZR_NULL) {
        return ZR_FALSE;
    }
    if (set->buckets == ZR_NULL) {
        return (TZrBool)(set->capacity == 0u && set->bucketSize == 0u &&
                         set->elementCount == 0u && !set->isValid &&
                         set->pairPoolHead == ZR_NULL && set->pairPoolActive == ZR_NULL &&
                         set->pairPoolTail == ZR_NULL && set->pairPoolCapacity == 0u &&
                         set->pairPoolUsed == 0u);
    }
    if (!set->isValid || set->capacity == 0u ||
        (set->capacity & (set->capacity - 1u)) != 0u ||
        set->capacity > (TZrSize)-1 / sizeof(*set->buckets) ||
        set->bucketSize != set->capacity * sizeof(*set->buckets) ||
        (set->pairPoolCapacity == 0u &&
         (set->pairPoolHead != ZR_NULL || set->pairPoolActive != ZR_NULL ||
          set->pairPoolTail != ZR_NULL || set->pairPoolUsed != 0u)) ||
        (set->pairPoolCapacity > 0u &&
         (set->pairPoolHead == ZR_NULL || set->pairPoolActive == ZR_NULL ||
          set->pairPoolTail == ZR_NULL))) {
        return ZR_FALSE;
    }
    for (TZrSize bucketIndex = 0u; bucketIndex < set->capacity; bucketIndex++) {
        for (SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
             pair != ZR_NULL; pair = pair->next) {
            TZrBool pairInPool = ZR_FALSE;
            if (!checkpoint_live_value_has_no_ownership(&pair->key) ||
                !checkpoint_live_value_has_no_ownership(&pair->value)) {
                return ZR_FALSE;
            }
            if (!checkpoint_hash_pair_is_valid_pool_member(set, pair, &pairInPool) ||
                activePairCount >= set->elementCount ||
                (pairInPool && activePoolPairCount >= set->pairPoolUsed)) {
                return ZR_FALSE;
            }
            if (pairInPool) {
                activePoolPairCount++;
            }
            activePairCount++;
        }
    }
    if (activePairCount != set->elementCount) {
        return ZR_FALSE;
    }
    /* Removed pool slots remain allocated until Deconstruct; inspect them too. */
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        TZrSize extraPairCapacity = block->capacity > ZR_HASH_PAIR_POOL_INLINE_COUNT
                ? block->capacity - ZR_HASH_PAIR_POOL_INLINE_COUNT : 0u;
        if (poolSteps++ >= set->pairPoolCapacity || block->capacity == 0u ||
            block->used > block->capacity ||
            extraPairCapacity > ((TZrSize)-1 - sizeof(*block)) /
                                        sizeof(SZrHashKeyValuePair) ||
            poolCapacity > (TZrSize)-1 - block->capacity ||
            poolUsed > (TZrSize)-1 - block->used ||
            poolCapacity + block->capacity > set->pairPoolCapacity ||
            poolUsed + block->used > set->pairPoolUsed) {
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
        for (TZrSize pairIndex = 0u; pairIndex < block->used; pairIndex++) {
            const SZrHashKeyValuePair *pair = &block->pairs[pairIndex];
            if (!checkpoint_live_value_has_no_ownership(&pair->key) ||
                !checkpoint_live_value_has_no_ownership(&pair->value)) {
                return ZR_FALSE;
            }
        }
    }
    return (TZrBool)(poolCapacity == set->pairPoolCapacity &&
                     poolUsed == set->pairPoolUsed &&
                     (set->pairPoolCapacity == 0u || (activeFound && tailFound)));
}

static TZrBool checkpoint_stack_pointer_is_in_bounds(const SZrState *state,
                                                      TZrStackValuePointer pointer) {
    TZrStackValuePointer base = state->stackBase.valuePointer;
    TZrStackValuePointer tail = state->stackTail.valuePointer;
    return (TZrBool)(pointer != ZR_NULL && base != ZR_NULL && tail != ZR_NULL &&
                     tail > base && pointer >= base && pointer <= tail);
}

/* ResetThread drops the active link; validate its previous chain before doing so. */
static TZrBool checkpoint_live_callinfo_chain_is_valid(const SZrState *state) {
    const SZrCallInfo *callInfo;
    TZrSize steps = 0u;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_FALSE;
    }
    callInfo = state->callInfoList;
    for (;;) {
        if (!checkpoint_stack_pointer_is_in_bounds(state, callInfo->functionBase.valuePointer) ||
            !checkpoint_stack_pointer_is_in_bounds(state, callInfo->functionTop.valuePointer) ||
            callInfo->functionTop.valuePointer < callInfo->functionBase.valuePointer ||
            (callInfo->hasReturnDestination &&
             (callInfo->returnDestination == ZR_NULL ||
              callInfo->returnDestination < state->stackBase.valuePointer ||
              callInfo->returnDestination >= state->stackTail.valuePointer)) ||
            (callInfo->hasArgumentSourceFrame &&
             !checkpoint_stack_pointer_is_in_bounds(
                     state, callInfo->argumentSourceFrameBase.valuePointer))) {
            return ZR_FALSE;
        }
        if (callInfo == &state->baseCallInfo) {
            return ZR_TRUE;
        }
        if (steps >= state->callInfoListLength || callInfo->previous == ZR_NULL) {
            return ZR_FALSE;
        }
        callInfo = callInfo->previous;
        steps++;
    }
}

/* ResetThread truncates the active stack without visiting each slot. */
static TZrBool checkpoint_live_stack_has_only_unowned_values(const SZrState *state) {
    TZrStackValuePointer slot;
    TZrStackValuePointer base;
    TZrStackValuePointer top;
    TZrStackValuePointer tail;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    base = state->stackBase.valuePointer;
    top = state->stackTop.valuePointer;
    tail = state->stackTail.valuePointer;
    if (base == ZR_NULL || top == ZR_NULL || tail == ZR_NULL ||
        tail <= base || top < base || top > tail ||
        (TZrSize)(tail - base) < 1u + ZR_STACK_NATIVE_CALL_RESERVED_MIN) {
        return ZR_FALSE;
    }
    for (slot = base; slot < top; slot++) {
        if (!checkpoint_live_value_has_no_ownership(&slot->value)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 先分配所有原生暂存，再解引用 GC 句柄；从首次写 live 状态起不再失败或分配。 */
static TZrBool checkpoint_resolve_optional_plan_object(
        const ZrCheckpointRestorePlan *plan,
        TZrSize objectCount,
        TZrSize objectIndex,
        EZrRawObjectType expectedType,
        TZrBool checkInternalType,
        EZrObjectInternalType expectedInternalType,
        SZrRawObject **outObject) {
    if (outObject == ZR_NULL) {
        return ZR_FALSE;
    }
    *outObject = ZR_NULL;
    if (objectIndex == ZR_CHECKPOINT_NO_INDEX) {
        return ZR_TRUE;
    }
    if (!checkpoint_resolve_plan_object(plan, objectCount, objectIndex, outObject) ||
        (*outObject)->type != expectedType ||
        (checkInternalType &&
         ((SZrObject *)*outObject)->internalType != expectedInternalType)) {
        *outObject = ZR_NULL;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool checkpoint_prepare_prototype_state(
        SZrState *state,
        const SZrSessionCheckpoint *checkpoint,
        ZrCheckpointRestorePlan *plan,
        const ZrCheckpointObject *objectSnapshot,
        ZrCheckpointRestorePlan *prepared) {
    const ZrCheckpointPrototype *snapshot = objectSnapshot->prototype;
    SZrObjectPrototype *livePrototype;
    SZrRawObject *resolved;
    TZrSize index;

    if (objectSnapshot->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ||
        snapshot == ZR_NULL || prepared->object == ZR_NULL ||
        prepared->object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    livePrototype = (SZrObjectPrototype *)prepared->object;
    if (livePrototype->super.internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ||
        livePrototype->type != snapshot->type ||
        livePrototype->memberDescriptorCount > livePrototype->memberDescriptorCapacity ||
        livePrototype->managedFieldCount > livePrototype->managedFieldCapacity ||
        livePrototype->memberDescriptorCapacity >
                (TZrSize)-1 / sizeof(*livePrototype->memberDescriptors) ||
        livePrototype->interfaceDispatchCount >
                (TZrSize)-1 / sizeof(*livePrototype->interfaceDispatchEntries) ||
        livePrototype->managedFieldCapacity >
                (TZrSize)-1 / sizeof(*livePrototype->managedFields) ||
        ((livePrototype->memberDescriptorCapacity == 0u) !=
         (livePrototype->memberDescriptors == ZR_NULL)) ||
        (livePrototype->memberDescriptorCount > 0u && livePrototype->memberDescriptors == ZR_NULL) ||
        (livePrototype->memberDescriptorCapacity > 0u && livePrototype->memberDescriptors == ZR_NULL) ||
        ((livePrototype->interfaceDispatchCount == 0u) !=
         (livePrototype->interfaceDispatchEntries == ZR_NULL)) ||
        ((livePrototype->managedFieldCapacity == 0u) !=
         (livePrototype->managedFields == ZR_NULL)) ||
        (livePrototype->managedFieldCount > 0u && livePrototype->managedFields == ZR_NULL) ||
        (livePrototype->managedFieldCapacity > 0u && livePrototype->managedFields == ZR_NULL) ||
        !checkpoint_live_map_has_only_unowned_values(&livePrototype->super.nodeMap)) {
        return ZR_FALSE;
    }
    if (livePrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
        !checkpoint_live_map_has_only_unowned_values(
                &((SZrStructPrototype *)livePrototype)->keyOffsetMap)) {
        return ZR_FALSE;
    }

    if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
            snapshot->nameIndex, ZR_RAW_OBJECT_TYPE_STRING, ZR_FALSE,
            ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
        return ZR_FALSE;
    }
    prepared->prototypeName = (SZrString *)resolved;
    if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
            snapshot->superPrototypeIndex, ZR_RAW_OBJECT_TYPE_OBJECT, ZR_TRUE,
            ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE, &resolved)) {
        return ZR_FALSE;
    }
    prepared->prototypeSuper = (SZrObjectPrototype *)resolved;

    for (index = 0u; index < ZR_META_ENUM_MAX; index++) {
        if (snapshot->hasMeta[index]) {
            SZrMeta *meta = prepared->prototypeMetas[index];
            if (meta == ZR_NULL ||
                !checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                    snapshot->metaFunctionIndexes[index], ZR_RAW_OBJECT_TYPE_FUNCTION,
                    ZR_FALSE, ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
                return ZR_FALSE;
            }
            meta->metaType = snapshot->metaTypes[index];
            meta->function = (SZrFunction *)resolved;
        } else if (prepared->prototypeMetas[index] != ZR_NULL) {
            return ZR_FALSE;
        }
    }

    for (index = 0u; index < snapshot->memberDescriptorCount; index++) {
        const ZrCheckpointMemberDescriptor *source = &snapshot->memberDescriptors[index];
        SZrMemberDescriptor *destination = &prepared->prototypeMemberDescriptors[index];
        SZrRawObject *references[8];
        *destination = source->value;
        for (TZrSize referenceIndex = 0u; referenceIndex < 8u; referenceIndex++) {
            EZrRawObjectType expectedType =
                    referenceIndex == 0u || referenceIndex >= 5u
                    ? ZR_RAW_OBJECT_TYPE_STRING : ZR_RAW_OBJECT_TYPE_FUNCTION;
            if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                    source->objectIndexes[referenceIndex], expectedType, ZR_FALSE,
                    ZR_OBJECT_INTERNAL_TYPE_OBJECT, &references[referenceIndex])) {
                return ZR_FALSE;
            }
        }
        destination->name = (SZrString *)references[0];
        destination->getterFunction = (SZrFunction *)references[1];
        destination->setterFunction = (SZrFunction *)references[2];
        destination->initializerFunction = (SZrFunction *)references[3];
        destination->methodFunction = (SZrFunction *)references[4];
        destination->ownerTypeName = (SZrString *)references[5];
        destination->baseDefinitionOwnerTypeName = (SZrString *)references[6];
        destination->baseDefinitionName = (SZrString *)references[7];
    }
    for (index = 0u; index < snapshot->interfaceDispatchCount; index++) {
        const ZrCheckpointInterfaceDispatchEntry *source =
                &snapshot->interfaceDispatchEntries[index];
        SZrInterfaceDispatchEntry *destination = &prepared->prototypeInterfaceDispatchEntries[index];
        *destination = source->value;
        if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                source->interfacePrototypeIndex, ZR_RAW_OBJECT_TYPE_OBJECT, ZR_TRUE,
                ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE, &resolved)) {
            return ZR_FALSE;
        }
        destination->interfacePrototype = (SZrObjectPrototype *)resolved;
        if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                source->implementationPrototypeIndex, ZR_RAW_OBJECT_TYPE_OBJECT, ZR_TRUE,
                ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE, &resolved)) {
            return ZR_FALSE;
        }
        destination->implementationPrototype = (SZrObjectPrototype *)resolved;
    }

    prepared->prototypeIndexContract = snapshot->indexContract;
    {
        SZrFunction **functions[4] = {
            &prepared->prototypeIndexContract.getByIndexFunction,
            &prepared->prototypeIndexContract.setByIndexFunction,
            &prepared->prototypeIndexContract.containsKeyFunction,
            &prepared->prototypeIndexContract.getLengthFunction };
        for (index = 0u; index < 4u; index++) {
            if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                    snapshot->indexFunctionIndexes[index], ZR_RAW_OBJECT_TYPE_FUNCTION,
                    ZR_FALSE, ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
                return ZR_FALSE;
            }
            *functions[index] = (SZrFunction *)resolved;
        }
    }
    prepared->prototypeIndexContract.getByIndexKnownNativeCallable = ZR_NULL;
    prepared->prototypeIndexContract.setByIndexKnownNativeCallable = ZR_NULL;
    prepared->prototypeIndexContract.getByIndexKnownNativeFunction = ZR_NULL;
    prepared->prototypeIndexContract.setByIndexKnownNativeFunction = ZR_NULL;
    memset(&prepared->prototypeIndexContract.getByIndexKnownNativeDirectDispatch, 0,
           sizeof(prepared->prototypeIndexContract.getByIndexKnownNativeDirectDispatch));
    memset(&prepared->prototypeIndexContract.setByIndexKnownNativeDirectDispatch, 0,
           sizeof(prepared->prototypeIndexContract.setByIndexKnownNativeDirectDispatch));
    prepared->prototypeIterableContract = snapshot->iterableContract;
    if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
            snapshot->iterableFunctionIndex, ZR_RAW_OBJECT_TYPE_FUNCTION, ZR_FALSE,
            ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
        return ZR_FALSE;
    }
    prepared->prototypeIterableContract.iterInitFunction = (SZrFunction *)resolved;
    prepared->prototypeIteratorContract = snapshot->iteratorContract;
    if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
            snapshot->iteratorFunctionIndexes[0], ZR_RAW_OBJECT_TYPE_FUNCTION, ZR_FALSE,
            ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
        return ZR_FALSE;
    }
    prepared->prototypeIteratorContract.moveNextFunction = (SZrFunction *)resolved;
    if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
            snapshot->iteratorFunctionIndexes[1], ZR_RAW_OBJECT_TYPE_FUNCTION, ZR_FALSE,
            ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
        return ZR_FALSE;
    }
    prepared->prototypeIteratorContract.currentFunction = (SZrFunction *)resolved;
    if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
            snapshot->iteratorMemberNameIndex, ZR_RAW_OBJECT_TYPE_STRING, ZR_FALSE,
            ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
        return ZR_FALSE;
    }
    prepared->prototypeIteratorContract.currentMemberName = (SZrString *)resolved;
    for (index = 0u; index < snapshot->managedFieldCount; index++) {
        prepared->prototypeManagedFields[index] = snapshot->managedFields[index].value;
        if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                snapshot->managedFields[index].nameIndex, ZR_RAW_OBJECT_TYPE_STRING,
                ZR_FALSE, ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
            return ZR_FALSE;
        }
        prepared->prototypeManagedFields[index].name = (SZrString *)resolved;
    }
    if (snapshot->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        if (snapshot->keyOffsetMapValid) {
            if (!checkpoint_fill_prepared_map(state, checkpoint,
                    &prepared->prototypeKeyOffsetMap, snapshot->keyOffsetPairs,
                    snapshot->keyOffsetPairCount)) {
                return ZR_FALSE;
            }
        } else if (snapshot->keyOffsetPairCount != 0u) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}


TZrBool checkpoint_state_is_clean_create_boundary(const SZrState *state) {
    TZrStackValuePointer base;
    TZrStackValuePointer top;
    TZrStackValuePointer tail;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    base = state->stackBase.valuePointer;
    top = state->stackTop.valuePointer;
    tail = state->stackTail.valuePointer;
    if (base == ZR_NULL || top == ZR_NULL || tail == ZR_NULL || tail <= base ||
        top < base || top > tail ||
        (TZrSize)(tail - base) < 1u + ZR_STACK_NATIVE_CALL_RESERVED_MIN ||
        top != base + 1) {
        return ZR_FALSE;
    }
    return (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE &&
        state->callInfoList == &state->baseCallInfo &&
        checkpoint_live_callinfo_chain_is_valid(state) &&
        state->executionBudget == ZR_NULL && state->nestedNativeCalls == 0u &&
        state->nestedNativeCallYieldFlag == 0u &&
        state->aotGcRootFrameDepth == 0u && state->aotGcRootFrameStack == ZR_NULL &&
        state->stackClosureValueList == ZR_NULL &&
        state->toBeClosedValueList.valuePointer == base &&
        state->exceptionRecoverPoint == ZR_NULL &&
        state->exceptionHandlerStackLength == 0u &&
        state->pendingControl.kind == ZR_VM_PENDING_CONTROL_NONE &&
        state->pendingControl.callInfo == ZR_NULL &&
        state->pendingControl.targetInstructionOffset == 0u &&
        state->pendingControl.valueSlot == 0u && !state->pendingControl.hasValue &&
        state->pendingControl.value.type == ZR_VALUE_TYPE_NULL &&
        checkpoint_live_value_has_no_ownership(&state->pendingControl.value) &&
        !state->hasCurrentException && state->currentException.type == ZR_VALUE_TYPE_NULL &&
        state->currentExceptionStatus == ZR_THREAD_STATUS_FINE &&
        checkpoint_live_value_has_no_ownership(&state->currentException) &&
        base->value.type == ZR_VALUE_TYPE_NULL &&
        checkpoint_live_value_has_no_ownership(&base->value));
}

/* Rust 项目会话在无 live Value 时进入；暂停同域 mutator 后捕获全局根与对象图。 */

TZrBool checkpoint_prepare_restore_plan(
        SZrState *state,
        const SZrSessionCheckpoint *checkpoint,
        ZrCheckpointRestorePlan **outPlan,
        ZrCheckpointRestoreRoots *outRoots,
        TZrBool *outNeedsReset) {

    ZrCheckpointRestorePlan *plan = ZR_NULL;
    ZrCheckpointRestoreRoots roots;
    TZrSize index;

    if (outPlan == ZR_NULL || outRoots == ZR_NULL || outNeedsReset == ZR_NULL) {
        return ZR_FALSE;
    }
    *outPlan = ZR_NULL;
    *outNeedsReset = ZR_FALSE;

    if (state == ZR_NULL || checkpoint == ZR_NULL || checkpoint->state != state) {
        return ZR_FALSE;
    }
    memset(&roots, 0, sizeof(roots));
    if (checkpoint->objectCount > (TZrSize)-1 / sizeof(*plan)) {
        return ZR_FALSE;
    }
    if (checkpoint->objectCount > 0u) {
        plan = (ZrCheckpointRestorePlan *)calloc(checkpoint->objectCount, sizeof(*plan));
        if (plan == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    /* 这一段仅申请 storage；使用 RawMallocWithType，不允许分配失败时隐式跑 GC。 */
    for (index = 0u; index < checkpoint->objectCount; index++) {
        const ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        ZrCheckpointRestorePlan *prepared = &plan[index];
        TZrSize allocationBytes;

        ZrCore_HashSet_Construct(&prepared->nodeMap);
        ZrCore_HashSet_Construct(&prepared->proNodeMap);
        ZrCore_HashSet_Construct(&prepared->prototypeKeyOffsetMap);
        if ((snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT ||
             snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) && snapshot->nodeMapValid &&
            !checkpoint_prepare_map_storage(state->global, &prepared->nodeMap, snapshot->pairCount)) {
            goto failure;
        }
        if (snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT || snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            if (snapshot->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE && snapshot->proNodeMapValid &&
                !checkpoint_prepare_map_storage(state->global, &prepared->proNodeMap,
                                                snapshot->proPairCount)) {
                goto failure;
            }
            if (snapshot->descriptorCount > 0u) {
                if (snapshot->descriptorCount > (TZrSize)-1 / sizeof(*prepared->descriptors)) {
                    goto failure;
                }
                allocationBytes = snapshot->descriptorCount * sizeof(*prepared->descriptors);
                prepared->descriptors = (SZrModuleExportDescriptor *)
                        ZrCore_Memory_RawMallocWithType(state->global, allocationBytes,
                                                        ZR_MEMORY_NATIVE_TYPE_OBJECT);
                if (prepared->descriptors == ZR_NULL) {
                    goto failure;
                }
            }
            if (snapshot->hasRawIntData && snapshot->rawIntCapacity > 0u) {
                if (snapshot->rawIntCapacity > (TZrSize)-1 / sizeof(*prepared->rawIntData)) {
                    goto failure;
                }
                allocationBytes = snapshot->rawIntCapacity * sizeof(*prepared->rawIntData);
                prepared->rawIntData = (TZrInt64 *)ZrCore_Memory_RawMallocWithType(
                        state->global, allocationBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (prepared->rawIntData == ZR_NULL) {
                    goto failure;
                }
            }
            if (snapshot->prototype != ZR_NULL) {
                const ZrCheckpointPrototype *prototype = snapshot->prototype;
                if (prototype->memberDescriptorCapacity > 0u) {
                    if (prototype->memberDescriptorCapacity >
                        (TZrSize)-1 / sizeof(*prepared->prototypeMemberDescriptors)) {
                        goto failure;
                    }
                    allocationBytes = prototype->memberDescriptorCapacity *
                                      sizeof(*prepared->prototypeMemberDescriptors);
                    prepared->prototypeMemberDescriptors = (SZrMemberDescriptor *)
                            ZrCore_Memory_RawMallocWithType(state->global, allocationBytes,
                                                           ZR_MEMORY_NATIVE_TYPE_OBJECT);
                    if (prepared->prototypeMemberDescriptors == ZR_NULL) {
                        goto failure;
                    }
                }
                if (prototype->interfaceDispatchCount > 0u) {
                    if (prototype->interfaceDispatchCount >
                        (TZrSize)-1 / sizeof(*prepared->prototypeInterfaceDispatchEntries)) {
                        goto failure;
                    }
                    allocationBytes = prototype->interfaceDispatchCount *
                                      sizeof(*prepared->prototypeInterfaceDispatchEntries);
                    prepared->prototypeInterfaceDispatchEntries = (SZrInterfaceDispatchEntry *)
                            ZrCore_Memory_RawMalloc(state->global, allocationBytes);
                    if (prepared->prototypeInterfaceDispatchEntries == ZR_NULL) {
                        goto failure;
                    }
                }
                if (prototype->managedFieldCapacity > 0u) {
                    if (prototype->managedFieldCapacity >
                        (TZrSize)-1 / sizeof(*prepared->prototypeManagedFields)) {
                        goto failure;
                    }
                    allocationBytes = prototype->managedFieldCapacity *
                                      sizeof(*prepared->prototypeManagedFields);
                    prepared->prototypeManagedFields = (SZrManagedFieldInfo *)
                            ZrCore_Memory_RawMallocWithType(state->global, allocationBytes,
                                                           ZR_MEMORY_NATIVE_TYPE_OBJECT);
                    if (prepared->prototypeManagedFields == ZR_NULL) {
                        goto failure;
                    }
                }
                for (TZrSize metaIndex = 0u; metaIndex < ZR_META_ENUM_MAX; metaIndex++) {
                    if (prototype->hasMeta[metaIndex]) {
                        prepared->prototypeMetas[metaIndex] = (SZrMeta *)
                                ZrCore_Memory_RawMallocWithType(state->global, sizeof(SZrMeta),
                                                               ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                        if (prepared->prototypeMetas[metaIndex] == ZR_NULL) {
                            goto failure;
                        }
                    }
                }
                if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
                    prototype->keyOffsetMapValid &&
                    !checkpoint_prepare_map_storage(state->global,
                            &prepared->prototypeKeyOffsetMap, prototype->keyOffsetPairCount)) {
                    goto failure;
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            if (snapshot->functionConstantCount > 0u) {
                if (snapshot->functionConstantCount > (TZrSize)-1 / sizeof(*prepared->functionConstants)) {
                    goto failure;
                }
                prepared->functionConstants = (SZrTypeValue *)calloc(
                        snapshot->functionConstantCount, sizeof(*prepared->functionConstants));
                if (prepared->functionConstants == ZR_NULL) {
                    goto failure;
                }
            }
            if (snapshot->prototypeInstanceCount > 0u) {
                if (snapshot->prototypeInstanceCount >
                    (TZrSize)-1 / sizeof(*prepared->prototypeInstances)) {
                    goto failure;
                }
                allocationBytes = snapshot->prototypeInstanceCount *
                                  sizeof(*prepared->prototypeInstances);
                prepared->prototypeInstances = (SZrObjectPrototype **)ZrCore_Memory_RawMalloc(
                        state->global, allocationBytes);
                if (prepared->prototypeInstances == ZR_NULL) {
                    goto failure;
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE && snapshot->captureCount > 0u) {
            if (snapshot->captureCount > (TZrSize)-1 / sizeof(*prepared->closureCaptures)) {
                goto failure;
            }
            prepared->closureCaptures = (SZrRawObject **)calloc(
                    snapshot->captureCount, sizeof(*prepared->closureCaptures));
            if (prepared->closureCaptures == ZR_NULL) {
                goto failure;
            }
        }
    }

    for (index = 0u; index < checkpoint->objectCount; index++) {
        const ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        if (!ZrCore_GcRootHandle_Resolve(state, &snapshot->handle, &plan[index].object) ||
            plan[index].object->type != snapshot->type) {
            goto failure;
        }
        if (snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT ||
            snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            SZrObject *object = (SZrObject *)plan[index].object;
            if (object->internalType != snapshot->internalType ||
                ((object->superArrayRawIntCapacity == 0u) !=
                 (object->superArrayRawIntData == ZR_NULL)) ||
                object->superArrayRawIntLength > object->superArrayRawIntCapacity ||
                object->superArrayRawIntCapacity > (TZrSize)-1 / sizeof(TZrInt64)) {
                goto failure;
            }
            if (snapshot->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                SZrObjectModule *module = (SZrObjectModule *)object;
                if (((module->exportDescriptorLength == 0u) !=
                     (module->exportDescriptors == ZR_NULL)) ||
                    module->exportDescriptorLength >
                            (TZrSize)-1 / sizeof(*module->exportDescriptors)) {
                    goto failure;
                }
            }
        }
    }

    if (!checkpoint_live_value_has_no_ownership(&state->global->loadedModulesRegistry) ||
        !checkpoint_live_value_has_no_ownership(&state->global->nullValue) ||
        !checkpoint_live_value_has_no_ownership(&state->global->zrObject) ||
        !checkpoint_live_value_has_no_ownership(&state->global->unhandledExceptionHandler)) {
        goto failure;
    }
    for (index = 0u; index < checkpoint->objectCount; index++) {
        const ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        SZrRawObject *raw = plan[index].object;
        if (snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT ||
            snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            SZrObject *object = (SZrObject *)raw;
            if (!checkpoint_live_map_has_only_unowned_values(&object->nodeMap) ||
                (snapshot->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE &&
                 !checkpoint_live_map_has_only_unowned_values(
                         &((SZrObjectModule *)object)->proNodeMap))) {
                goto failure;
            }
        }
    }

    /* 句柄解析放在最后一次分配之后；以下所有指针在提交前都不会因 GC 移动。 */
    /* All native storage is reserved; resolve the retained cache graph before commit. */
    for (index = 0u; index < checkpoint->objectCount; index++) {
        const ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        ZrCheckpointRestorePlan *prepared = &plan[index];
        SZrRawObject *resolved;
        if (snapshot->prototype != ZR_NULL &&
            !checkpoint_prepare_prototype_state(state, checkpoint, plan, snapshot, prepared)) {
            goto failure;
        }
        if (snapshot->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            SZrFunction *function = (SZrFunction *)prepared->object;
            if ((function->prototypeInstancesLength == 0u) !=
                    (function->prototypeInstances == ZR_NULL) ||
                function->prototypeInstancesLength >
                        (TZrSize)-1 / sizeof(*function->prototypeInstances) ||
                (snapshot->prototypeInstanceCount > 0u &&
                 snapshot->prototypeInstanceIndexes == ZR_NULL) ||
                (snapshot->prototypeInstanceCount > 0u &&
                 prepared->prototypeInstances == ZR_NULL) ||
                !checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                    snapshot->hasCachedStatelessClosure
                        ? snapshot->cachedStatelessClosureIndex : ZR_CHECKPOINT_NO_INDEX,
                    ZR_RAW_OBJECT_TYPE_CLOSURE, ZR_FALSE,
                    ZR_OBJECT_INTERNAL_TYPE_OBJECT, &resolved)) {
                goto failure;
            }
            prepared->cachedStatelessClosure = (SZrClosure *)resolved;
            for (TZrSize instanceIndex = 0u;
                 instanceIndex < snapshot->prototypeInstanceCount;
                 instanceIndex++) {
                if (!checkpoint_resolve_optional_plan_object(plan, checkpoint->objectCount,
                        snapshot->prototypeInstanceIndexes[instanceIndex],
                        ZR_RAW_OBJECT_TYPE_OBJECT, ZR_TRUE,
                        ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE, &resolved)) {
                    goto failure;
                }
                prepared->prototypeInstances[instanceIndex] = (SZrObjectPrototype *)resolved;
            }
        }
    }

    if (!checkpoint_restore_value(state, checkpoint, &checkpoint->loadedModulesRegistry,
                                  &roots.loadedModulesRegistry) ||
        !checkpoint_restore_value(state, checkpoint, &checkpoint->nullValue, &roots.nullValue) ||
        !checkpoint_restore_value(state, checkpoint, &checkpoint->zrObject, &roots.zrObject)) {
        goto failure;
    }
    if (checkpoint->hasUnhandledExceptionHandler) {
        if (!checkpoint_restore_value(state, checkpoint, &checkpoint->unhandledExceptionHandler,
                                      &roots.unhandledExceptionHandler)) {
            goto failure;
        }
    } else {
        ZrCore_Value_ResetAsNull(&roots.unhandledExceptionHandler);
    }
    if (checkpoint->hasErrorPrototype &&
        !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                checkpoint->errorPrototypeIndex, (SZrRawObject **)&roots.errorPrototype)) {
        goto failure;
    }
    if (checkpoint->hasStackFramePrototype &&
        !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                checkpoint->stackFramePrototypeIndex, (SZrRawObject **)&roots.stackFramePrototype)) {
        goto failure;
    }
    for (index = 0u; index < ZR_VALUE_TYPE_ENUM_MAX; index++) {
        if (checkpoint->hasBasicTypePrototype[index] &&
            !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                    checkpoint->basicTypePrototypeIndexes[index],
                    (SZrRawObject **)&roots.basicTypePrototypes[index])) {
            goto failure;
        }
    }
    for (index = 0u; index < ZR_META_ENUM_MAX; index++) {
        if (checkpoint->hasMetaFunctionName[index] &&
            !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                    checkpoint->metaFunctionNameIndexes[index],
                    (SZrRawObject **)&roots.metaFunctionNames[index])) {
            goto failure;
        }
    }
    for (index = 0u; index < checkpoint->objectCount; index++) {
        const ZrCheckpointObject *snapshot = &checkpoint->objects[index];
        ZrCheckpointRestorePlan *prepared = &plan[index];

        if (snapshot->type == ZR_RAW_OBJECT_TYPE_OBJECT ||
            snapshot->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            SZrObject *object = (SZrObject *)prepared->object;
            if (snapshot->hasPrototype &&
                !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                        snapshot->prototypeIndex, (SZrRawObject **)&prepared->prototype)) {
                goto failure;
            }
            if (snapshot->nodeMapValid &&
                !checkpoint_fill_prepared_map(state, checkpoint, &prepared->nodeMap,
                                              snapshot->pairs, snapshot->pairCount)) {
                goto failure;
            }
            if (snapshot->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                if (snapshot->proNodeMapValid &&
                    !checkpoint_fill_prepared_map(state, checkpoint, &prepared->proNodeMap,
                                                  snapshot->proPairs, snapshot->proPairCount)) {
                    goto failure;
                }
                if (snapshot->hasModuleName &&
                    !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                            snapshot->moduleNameIndex, (SZrRawObject **)&prepared->moduleName)) {
                    goto failure;
                }
                if (snapshot->hasFullPath &&
                    !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                            snapshot->fullPathIndex, (SZrRawObject **)&prepared->fullPath)) {
                    goto failure;
                }
                if (snapshot->hasMetadataFunction &&
                    !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                            snapshot->metadataFunctionIndex,
                            (SZrRawObject **)&prepared->metadataFunction)) {
                    goto failure;
                }
                if (snapshot->descriptorCount > 0u && prepared->descriptors == ZR_NULL) {
                    goto failure;
                }
                for (TZrSize descriptorIndex = 0u;
                     descriptorIndex < snapshot->descriptorCount; descriptorIndex++) {
                    const ZrCheckpointModuleDescriptor *saved = &snapshot->descriptors[descriptorIndex];
                    SZrModuleExportDescriptor *restored = &prepared->descriptors[descriptorIndex];
                    restored->name = ZR_NULL;
                    restored->accessModifier = saved->accessModifier;
                    restored->exportKind = saved->exportKind;
                    restored->readiness = saved->readiness;
                    restored->isReady = saved->isReady;
                    if (saved->hasName &&
                        !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                            saved->nameIndex, (SZrRawObject **)&restored->name)) {
                        goto failure;
                    }
                }
            }
            if (object->superArrayRawIntData != ZR_NULL &&
                object->superArrayRawIntCapacity > (TZrSize)-1 / sizeof(TZrInt64)) {
                goto failure;
            }
            if (snapshot->hasRawIntData) {
                if (snapshot->rawIntLength > snapshot->rawIntCapacity ||
                    (snapshot->rawIntCapacity > 0u &&
                     (snapshot->rawIntData == ZR_NULL || prepared->rawIntData == ZR_NULL))) {
                    goto failure;
                }
                if (snapshot->rawIntCapacity > 0u) {
                    memcpy(prepared->rawIntData, snapshot->rawIntData,
                           snapshot->rawIntCapacity * sizeof(*prepared->rawIntData));
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            SZrFunction *function = (SZrFunction *)prepared->object;
            if (function->constantValueLength != snapshot->functionConstantCount ||
                (snapshot->functionConstantCount > 0u &&
                 (function->constantValueList == ZR_NULL || prepared->functionConstants == ZR_NULL)) ||
                function->typedExportedSymbolLength != snapshot->typedExportMetadataTokenCount ||
                (snapshot->typedExportMetadataTokenCount > 0u && function->typedExportedSymbols == ZR_NULL)) {
                goto failure;
            }
            if (snapshot->hasDecoratorMetadata) {
                if (!function->hasDecoratorMetadata ||
                    !checkpoint_live_value_has_no_ownership(&function->decoratorMetadataValue) ||
                    !checkpoint_restore_value(state, checkpoint, &snapshot->decoratorMetadata,
                                              &prepared->decoratorMetadata)) {
                    goto failure;
                }
            } else if (function->hasDecoratorMetadata &&
                       !checkpoint_live_value_has_no_ownership(&function->decoratorMetadataValue)) {
                goto failure;
            }
            for (TZrSize constantIndex = 0u;
                 constantIndex < snapshot->functionConstantCount; constantIndex++) {
                if (function->constantValueList[constantIndex].ownershipKind !=
                            ZR_OWNERSHIP_VALUE_KIND_NONE ||
                    function->constantValueList[constantIndex].ownershipControl != ZR_NULL ||
                    function->constantValueList[constantIndex].ownershipWeakRef != ZR_NULL ||
                    !checkpoint_restore_value(state, checkpoint,
                        &snapshot->functionConstants[constantIndex],
                        &prepared->functionConstants[constantIndex])) {
                    goto failure;
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
            SZrClosure *closure = (SZrClosure *)prepared->object;
            if (!closure->super.isNative) {
                if (closure->closureValueCount != snapshot->captureCount ||
                    (snapshot->captureCount > 0u &&
                     (closure->closureValuesExtend == ZR_NULL || prepared->closureCaptures == ZR_NULL))) {
                    goto failure;
                }
                if (snapshot->hasFunction &&
                    !checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                            snapshot->functionIndex, (SZrRawObject **)&prepared->closureFunction)) {
                    goto failure;
                }
                for (TZrSize captureIndex = 0u; captureIndex < snapshot->captureCount; captureIndex++) {
                    if (!checkpoint_resolve_plan_object(plan, checkpoint->objectCount,
                            snapshot->captureIndexes[captureIndex],
                            &prepared->closureCaptures[captureIndex])) {
                        goto failure;
                    }
                }
            }
        } else if (snapshot->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE && snapshot->hasClosureValue) {
            SZrClosureValue *closureValue = (SZrClosureValue *)prepared->object;
            SZrTypeValue *currentValue = ZrCore_ClosureValue_GetValue(closureValue);
            if (!ZrCore_ClosureValue_IsClosed(closureValue) || currentValue == ZR_NULL ||
                currentValue->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
                currentValue->ownershipControl != ZR_NULL || currentValue->ownershipWeakRef != ZR_NULL ||
                !checkpoint_restore_value(state, checkpoint, &snapshot->closureValue,
                                          &prepared->closureValue)) {
                goto failure;
            }
        }
    }

    /*
     * ResetThread can run pending-value and exception-handler cleanup. Reject those
     * live scopes before resetting; the empty to-be-closed list uses stackBase as
     * its sentinel. With cleanup scopes empty, reset has no guest callback,
     * allocation, GC, or non-FINE result. Keep it after all fallible preparation
     * so a preparation failure preserves the live thread.
     */
    if (state->exceptionRecoverPoint != ZR_NULL ||
        state->exceptionHandlerStackLength != 0u ||
        state->stackBase.valuePointer == ZR_NULL ||
        state->toBeClosedValueList.valuePointer != state->stackBase.valuePointer ||
        state->aotGcRootFrameDepth != 0u ||
        state->aotGcRootFrameStack != ZR_NULL ||
        !checkpoint_live_callinfo_chain_is_valid(state) ||
        state->pendingControl.kind != ZR_VM_PENDING_CONTROL_NONE ||
        state->pendingControl.callInfo != ZR_NULL || state->pendingControl.hasValue ||
        state->pendingControl.value.type != ZR_VALUE_TYPE_NULL ||
        !checkpoint_live_value_has_no_ownership(&state->pendingControl.value) ||
        !checkpoint_live_value_has_no_ownership(&state->currentException) ||
        !checkpoint_live_stack_has_only_unowned_values(state)) {
        goto failure;
    }

    /* Every commit barrier is void and can grow the remembered registry.  A
     * conservative reservation keeps that allocation in preparation, while a
     * failure still leaves the live thread and graph untouched.  Keep this as
     * the final fallible preparation step: all earlier validation and pointer
     * resolution must have succeeded before the live collector shape changes. */
    if (state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        goto failure;
    }
    {
        SZrGarbageCollector *collector = state->global->garbageCollector;
        TZrSize requiredCapacity;
        TZrSize plannedCapacity;

        if (collector->rememberedObjectCount > (TZrSize)-1 - checkpoint->objectCount) {
            goto failure;
        }
        requiredCapacity = collector->rememberedObjectCount + checkpoint->objectCount;
        /* gc_object.c keeps these growth constants private to its
         * implementation unit.  Keep the preflight arithmetic aligned with
         * those current values rather than referring to unavailable macros. */
        plannedCapacity = collector->rememberedObjectCapacity > 0u
                ? collector->rememberedObjectCapacity
                : (TZrSize)8;
        while (plannedCapacity < requiredCapacity) {
            if (plannedCapacity > (TZrSize)-1 / (TZrSize)2) {
                goto failure;
            }
            plannedCapacity *= (TZrSize)2;
        }
        if (plannedCapacity > (TZrSize)-1 / sizeof(SZrRawObject *) ||
            !garbage_collector_ensure_remembered_registry_capacity(
                state->global, requiredCapacity)) {
            goto failure;
        }
    }

    *outNeedsReset = (state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList != &state->baseCallInfo || state->hasCurrentException ||
        state->stackTop.valuePointer != state->stackBase.valuePointer + 1);
    *outPlan = plan;
    *outRoots = roots;
    return ZR_TRUE;

failure:
    checkpoint_release_restore_plan(state, checkpoint, plan);
    return ZR_FALSE;
}
