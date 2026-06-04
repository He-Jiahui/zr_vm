set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break __assert_fail
run
bt 8
frame 1
printf "currentFunction=%p instructionsLength=%u stackSize=%u frameByteSize=%u layoutLength=%u pc_index=%ld\n", currentFunction, currentFunction->instructionsLength, currentFunction->stackSize, currentFunction->frameByteSize, currentFunction->frameSlotLayoutLength, programCounter - currentFunction->instructionsList
printf "instruction op=%u extra=%u a1=%u b1=%u a2=%d raw=%llu\n", instruction.instruction.operationCode, instruction.instruction.operandExtra, instruction.instruction.operand.operand1[0], instruction.instruction.operand.operand1[1], instruction.instruction.operand.operand2[0], instruction.value
printf "opA=%p type=%u nativeInt=%lld object=%p\n", opA, opA->type, opA->value.nativeObject.nativeInt64, opA->value.object
set $functionSlot = instruction.instruction.operand.operand1[0]
set $layoutValue = execution_inline_frame_get_value_slot(state, currentFunction, base, $functionSlot)
printf "functionSlot=%u layoutValue=%p type=%u nativeInt=%lld object=%p physical=%p physicalType=%u physicalObject=%p\n", $functionSlot, $layoutValue, $layoutValue->type, $layoutValue->value.nativeObject.nativeInt64, $layoutValue->value.object, &base[$functionSlot].value, base[$functionSlot].value.type, base[$functionSlot].value.value.object
set $slot = 0
while $slot < currentFunction->stackSize
    set $value = execution_inline_frame_get_value_slot(state, currentFunction, base, $slot)
    printf "slot %u value@%p type=%u nativeInt=%lld object=%p physical@%p physicalType=%u physicalObject=%p\n", $slot, $value, $value->type, $value->value.nativeObject.nativeInt64, $value->value.object, &base[$slot].value, base[$slot].value.type, base[$slot].value.value.object
    set $slot = $slot + 1
end
set $idx = 0
while $idx < currentFunction->instructionsLength
    printf "insn %u: op=%u extra=%u a1=%u b1=%u a2=%d raw=%llu\n", $idx, currentFunction->instructionsList[$idx].instruction.operationCode, currentFunction->instructionsList[$idx].instruction.operandExtra, currentFunction->instructionsList[$idx].instruction.operand.operand1[0], currentFunction->instructionsList[$idx].instruction.operand.operand1[1], currentFunction->instructionsList[$idx].instruction.operand.operand2[0], currentFunction->instructionsList[$idx].value
    set $idx = $idx + 1
end
