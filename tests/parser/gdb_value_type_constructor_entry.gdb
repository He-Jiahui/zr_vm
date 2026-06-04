set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break ZrCore_Execute if callInfo != 0 && currentFunction != 0 && currentFunction->instructionsLength == 4 && currentFunction->parameterCount == 1
commands
    silent
    printf "constructor execute currentFunction=%p stackSize=%u frameByteSize=%u layoutLength=%u base=%p functionBase=%p functionTop=%p\n", currentFunction, currentFunction->stackSize, currentFunction->frameByteSize, currentFunction->frameSlotLayoutLength, base, callInfo->functionBase.valuePointer, callInfo->functionTop.valuePointer
    set $slot = 0
    while $slot < currentFunction->stackSize
        set $value = execution_inline_frame_get_value_slot(state, currentFunction, base, $slot)
        printf "entry slot %u value@%p type=%u object=%p physical@%p physicalType=%u physicalObject=%p\n", $slot, $value, $value->type, $value->value.object, &base[$slot].value, base[$slot].value.type, base[$slot].value.value.object
        set $slot = $slot + 1
    end
    continue
end
break execution_inline_frame_try_set_member_from_slot if function != 0 && function->instructionsLength == 4 && receiverSlot == 0
commands
    silent
    printf "constructor set_member receiverSlot=%u sourceSlot=%u cacheIndex=%u assigned=%p assignedType=%u assignedObject=%p\n", receiverSlot, sourceSlot, cacheIndex, assignedValue, assignedValue->type, assignedValue->value.object
    set $srcValue = execution_inline_frame_get_value_slot(state, function, frameBase, sourceSlot)
    set $recvValue = execution_inline_frame_get_value_slot(state, function, frameBase, receiverSlot)
    printf "  source logical@%p type=%u object=%p physicalType=%u physicalObject=%p\n", $srcValue, $srcValue->type, $srcValue->value.object, frameBase[sourceSlot].value.type, frameBase[sourceSlot].value.value.object
    printf "  receiver logical@%p type=%u object=%p physicalType=%u physicalObject=%p\n", $recvValue, $recvValue->type, $recvValue->value.object, frameBase[receiverSlot].value.type, frameBase[receiverSlot].value.value.object
    continue
end
break ZrCore_Function_TryCopyInlineFrameReturnValue if callInfo != 0
commands
    silent
    set $callee = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, (SZrCallInfo *)callInfo)
    if $callee != 0 && $callee->instructionsLength == 4 && $callee->parameterCount == 1
        printf "constructor return-copy returnSource=%p returnDestination=%p callee=%p\n", returnSource, returnDestination, $callee
    end
    continue
end
run
