# TODO: 断点绑定 object_super_array.c/hash_set.c 行号；源码变更后核对行号与局部变量。
# 同时观察批量填充和哈希桶扩容，定位 items 缓存容量与 Pair 池容量不同步。
# 运行前由 gdb --args 指定可执行文件及触发批量填充的项目。
set pagination off
set confirm off

break zr_vm_core/src/zr_vm_core/object/object_super_array.c:971 if requiredLength==8 && appendCount==8
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
