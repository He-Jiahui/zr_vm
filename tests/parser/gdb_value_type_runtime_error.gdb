set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break execution_raise_vm_runtime_error
run
printf "format=%s\n", format
bt 6
frame 1
printf "currentFunction=%p instructionsLength=%u stackSize=%u frameByteSize=%u layoutLength=%u pc_index=%ld\n", currentFunction, currentFunction->instructionsLength, currentFunction->stackSize, currentFunction->frameByteSize, currentFunction->frameSlotLayoutLength, programCounter - currentFunction->instructionsList
set $idx = 0
while $idx < currentFunction->instructionsLength
    printf "insn %u: op=%u extra=%u a1=%u b1=%u a2=%d raw=%llu\n", $idx, currentFunction->instructionsList[$idx].instruction.operationCode, currentFunction->instructionsList[$idx].instruction.operandExtra, currentFunction->instructionsList[$idx].instruction.operand.operand1[0], currentFunction->instructionsList[$idx].instruction.operand.operand1[1], currentFunction->instructionsList[$idx].instruction.operand.operand2[0], currentFunction->instructionsList[$idx].value
    set $idx = $idx + 1
end
set $layout = 0
while $layout < currentFunction->frameSlotLayoutLength
    printf "layout %u: slot=%u off=%u size=%u kind=%u type=%u param=%u\n", $layout, currentFunction->frameSlotLayouts[$layout].stackSlot, currentFunction->frameSlotLayouts[$layout].byteOffset, currentFunction->frameSlotLayouts[$layout].byteSize, currentFunction->frameSlotLayouts[$layout].slotKind, currentFunction->frameSlotLayouts[$layout].typeLayoutId, currentFunction->frameSlotLayouts[$layout].isParameter
    set $layout = $layout + 1
end
set $slot = 0
while $slot < currentFunction->stackSize
    set $value = execution_inline_frame_get_value_slot(state, currentFunction, base, $slot)
    printf "slot %u value@%p type=%u nativeInt=%lld object=%p\n", $slot, $value, $value->type, $value->value.nativeObject.nativeInt64, $value->value.object
    set $slot = $slot + 1
end
set $bytes = (unsigned char *)base
printf "slot0 inline bytes:"
set $byte = 0
while $byte < 80
    printf " %02x", $bytes[$byte]
    set $byte = $byte + 1
end
printf "\n"
