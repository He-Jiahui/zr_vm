set pagination off
set confirm off

break zr_vm_core/src/zr_vm_core/object_super_array.c:971 if requiredLength==8 && appendCount==8
commands
    silent
    printf "ensure items=%p cap=%zu threshold=%zu elem=%zu pairPoolCap=%zu pairPoolUsed=%zu cachedCap=%lld required=%zu append=%zu\n", itemsObject, itemsObject->nodeMap.capacity, itemsObject->nodeMap.resizeThreshold, itemsObject->nodeMap.elementCount, itemsObject->nodeMap.pairPoolCapacity, itemsObject->nodeMap.pairPoolUsed, receiverObject->cachedCapacityPair ? receiverObject->cachedCapacityPair->value.value.nativeObject.nativeInt64 : -1LL, requiredLength, appendCount
    continue
end

break zr_vm_core/src/zr_vm_core/hash_set.c:196 if newCapacity>=8
commands
    silent
    printf "grow set=%p old=%zu new=%zu elem=%zu threshold=%zu pairPoolCap=%zu pairPoolUsed=%zu\n", set, set->capacity, newCapacity, set->elementCount, set->resizeThreshold, set->pairPoolCapacity, set->pairPoolUsed
    continue
end

run
