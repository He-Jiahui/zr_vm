set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:6445
commands
silent
set $functionSlot = instruction.instruction.operand.operand1[0]
set $parametersCount = instruction.instruction.operand.operand1[1]
set $resultSlot = instruction.instruction.operandExtra
printf "known vm call pc=%ld fn=%p stack=%u frameBytes=%u result=%u functionSlot=%u parameters=%u name=%s\n", programCounter - currentFunction->instructionsList, currentFunction, currentFunction->stackSize, currentFunction->frameByteSize, $resultSlot, $functionSlot, $parametersCount, currentFunction->functionName ? currentFunction->functionName->string : "<anon>"
set $slotValue = execution_inline_frame_get_value_slot(state, currentFunction, base, $functionSlot)
printf "  logical function slot type=%u obj=%p int=%lld\n", $slotValue->type, $slotValue->value.object, $slotValue->value.nativeObject.nativeInt64
set $i = 0
while $i < currentFunction->instructionsLength
  set $ins = currentFunction->instructionsList[$i]
  if $i >= 6 && $i <= 15
    printf "  ins[%u] op=%u e=%u a1=%u b1=%u a0=%u b0=%u c0=%u d0=%u\n", $i, $ins.instruction.operationCode, $ins.instruction.operandExtra, $ins.instruction.operand.operand1[0], $ins.instruction.operand.operand1[1], $ins.instruction.operand.operand0[0], $ins.instruction.operand.operand0[1], $ins.instruction.operand.operand0[2], $ins.instruction.operand.operand0[3]
  end
  set $i = $i + 1
end
continue
end
run
