set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break execution_inline_frame_try_get_member_to_slot if function != 0 && function->instructionsLength == 19 && function->instructionsList[16].instruction.operationCode == 39 && receiverSlot == 2 && destinationSlot == 6
run
bt 6
printf "receiverSlot=%u cacheIndex=%u destinationSlot=%u result=%p\n", receiverSlot, cacheIndex, destinationSlot, result
printf "function=%p instructionIndex14 op=%u extra=%u a1=%u b1=%u\n", function, function->instructionsList[14].instruction.operationCode, function->instructionsList[14].instruction.operandExtra, function->instructionsList[14].instruction.operand.operand1[0], function->instructionsList[14].instruction.operand.operand1[1]
printf "cacheLength=%u memberEntryLength=%u\n", function->callSiteCacheLength, function->memberEntryLength
set $ci = 0
while $ci < function->callSiteCacheLength
    printf "cache %u: kind=%u instructionIndex=%u memberEntryIndex=%u argCount=%u picCount=%u runtimeHit=%u runtimeMiss=%u\n", $ci, function->callSiteCaches[$ci].kind, function->callSiteCaches[$ci].instructionIndex, function->callSiteCaches[$ci].memberEntryIndex, function->callSiteCaches[$ci].argumentCount, function->callSiteCaches[$ci].picSlotCount, function->callSiteCaches[$ci].runtimeHitCount, function->callSiteCaches[$ci].runtimeMissCount
    if function->callSiteCaches[$ci].memberEntryIndex < function->memberEntryLength && function->memberEntries[function->callSiteCaches[$ci].memberEntryIndex].symbol != 0
        set $memberString = function->memberEntries[function->callSiteCaches[$ci].memberEntryIndex].symbol
        printf "  memberSymbol=%p shortLen=%u bytes=%c%c%c%c%c%c%c%c\n", $memberString, $memberString->shortStringLength, $memberString->stringDataExtend[0], $memberString->stringDataExtend[1], $memberString->stringDataExtend[2], $memberString->stringDataExtend[3], $memberString->stringDataExtend[4], $memberString->stringDataExtend[5], $memberString->stringDataExtend[6], $memberString->stringDataExtend[7]
    end
    if function->callSiteCaches[$ci].picSlotCount > 0
        printf "  pic0 access=%u hot=%u cachedName=%p receiverObject=%p pair=%p prototype=%p desc=%u\n", function->callSiteCaches[$ci].picSlots[0].cachedAccessKind, function->callSiteCaches[$ci].picSlots[0].cachedHotFieldKind, function->callSiteCaches[$ci].picSlots[0].cachedMemberName, function->callSiteCaches[$ci].picSlots[0].cachedReceiverObject, function->callSiteCaches[$ci].picSlots[0].cachedReceiverPair, function->callSiteCaches[$ci].picSlots[0].cachedReceiverPrototype, function->callSiteCaches[$ci].picSlots[0].cachedDescriptorIndex
    end
    set $ci = $ci + 1
end
set $receiverLayout = ZrCore_Function_FindFrameSlotLayout(function, receiverSlot)
set $destinationLayout = ZrCore_Function_FindFrameSlotLayout(function, destinationSlot)
printf "receiverLayout=%p kind=%u typeLayoutId=%u offset=%u size=%u\n", $receiverLayout, $receiverLayout->slotKind, $receiverLayout->typeLayoutId, $receiverLayout->byteOffset, $receiverLayout->byteSize
printf "destinationLayout=%p kind=%u typeLayoutId=%u offset=%u size=%u\n", $destinationLayout, $destinationLayout->slotKind, $destinationLayout->typeLayoutId, $destinationLayout->byteOffset, $destinationLayout->byteSize
set $receiverValue = execution_inline_frame_get_value_slot(state, function, frameBase, receiverSlot)
set $destinationValue = execution_inline_frame_get_value_slot(state, function, frameBase, destinationSlot)
printf "receiverValue@%p type=%u object=%p physicalType=%u physicalObject=%p\n", $receiverValue, $receiverValue->type, $receiverValue->value.object, frameBase[receiverSlot].value.type, frameBase[receiverSlot].value.value.object
printf "destinationBefore@%p type=%u object=%p physicalType=%u physicalObject=%p\n", $destinationValue, $destinationValue->type, $destinationValue->value.object, frameBase[destinationSlot].value.type, frameBase[destinationSlot].value.value.object
set $fn = function
set $fb = frameBase
set $dst = destinationSlot
set $dest = result
tbreak execution_inline_frame_load_value_slot_field
continue
printf "fieldLayout=%p valueType=%u isValueSlot=%u isPrimitivePod=%u typeLayoutId=%u offset? size=%u fieldAddress=%p\n", fieldLayout, fieldLayout->valueType, fieldLayout->isValueSlot, fieldLayout->isPrimitivePod, fieldLayout->typeLayoutId, fieldLayout->byteSize, fieldAddress
printf "fieldValue type=%u object=%p nativeInt=%lld\n", ((SZrTypeValue *)fieldAddress)->type, ((SZrTypeValue *)fieldAddress)->value.object, ((SZrTypeValue *)fieldAddress)->value.nativeObject.nativeInt64
finish
printf "loadReturn=%u destinationAfter@%p type=%u object=%p physicalType=%u physicalObject=%p\n", $rax, $dest, ((SZrTypeValue *)$dest)->type, ((SZrTypeValue *)$dest)->value.object, $fb[$dst].value.type, $fb[$dst].value.value.object
