set pagination off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:1887
commands
silent
printf "set member slot currentFunction=%p pcOpcode=%u receiverSlot=%u sourceSlot=%u cache=%u\n", currentFunction, instruction->instruction.operationCode, instruction->instruction.operand.operand1[0], instruction->instruction.operandExtra, instruction->instruction.operand.operand1[1]
set $layout = ZrCore_Function_FindFrameSlotLayout(currentFunction, instruction->instruction.operand.operand1[0])
if $layout != 0
  printf "  receiver layout slot=%u kind=%u offset=%u size=%u type=%u param=%u\n", $layout->stackSlot, $layout->slotKind, $layout->byteOffset, $layout->byteSize, $layout->typeLayoutId, $layout->isParameter
else
  printf "  receiver layout null\n"
end
continue
end
run
