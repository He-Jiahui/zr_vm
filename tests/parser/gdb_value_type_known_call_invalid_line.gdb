set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:2201
run
bt 8
frame 0
printf "currentFunction=%p instructionsLength=%u stackSize=%u frameByteSize=%u layoutLength=%u pc_index=%ld\n", currentFunction, currentFunction->instructionsLength, currentFunction->stackSize, currentFunction->frameByteSize, currentFunction->frameSlotLayoutLength, programCounter - currentFunction->instructionsList
printf "instruction op=%u extra=%u a1=%u b1=%u a2=%d raw=%llu\n", instruction.instruction.operationCode, instruction.instruction.operandExtra, instruction.instruction.operand.operand1[0], instruction.instruction.operand.operand1[1], instruction.instruction.operand.operand2[0], instruction.value
printf "opA=%p type=%u native=%u gc=%u object=%p nativeInt=%lld nextCallInfo=%p\n", opA, opA->type, opA->isNative, opA->isGarbageCollectable, opA->value.object, opA->value.nativeObject.nativeInt64, nextCallInfo__
set $functionSlot = instruction.instruction.operand.operand1[0]
set $paramCount = instruction.instruction.operand.operand1[1]
set $offset = 0
while $offset <= $paramCount
    set $logicalSlot = $functionSlot + $offset
    set $logical = execution_inline_frame_get_value_slot(state, currentFunction, base, $logicalSlot)
    printf "call offset %u logicalSlot=%u logical@%p type=%u native=%u object=%p nativeInt=%lld physical@%p type=%u native=%u object=%p nativeInt=%lld\n", $offset, $logicalSlot, $logical, $logical->type, $logical->isNative, $logical->value.object, $logical->value.nativeObject.nativeInt64, &base[$logicalSlot].value, base[$logicalSlot].value.type, base[$logicalSlot].value.isNative, base[$logicalSlot].value.value.object, base[$logicalSlot].value.value.nativeObject.nativeInt64
    set $offset = $offset + 1
end
