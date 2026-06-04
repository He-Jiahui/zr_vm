set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
set $targetFunction = 0
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:6445
commands
silent
if programCounter - currentFunction->instructionsList == 8
  set $targetFunction = currentFunction
  printf "target fn=%p captured at pc=8\n", $targetFunction
end
if currentFunction == $targetFunction
  printf "known pc=%ld e=%u a1=%u b1=%u slot0 type=%u obj=%p int=%lld slot2 type=%u obj=%p int=%lld slot4 type=%u obj=%p int=%lld slot5 type=%u obj=%p int=%lld\n", programCounter - currentFunction->instructionsList, instruction.instruction.operandExtra, instruction.instruction.operand.operand1[0], instruction.instruction.operand.operand1[1], execution_inline_frame_get_value_slot(state,currentFunction,base,0)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,0)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,0)->value.nativeObject.nativeInt64, execution_inline_frame_get_value_slot(state,currentFunction,base,2)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,2)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,2)->value.nativeObject.nativeInt64, execution_inline_frame_get_value_slot(state,currentFunction,base,4)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,4)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,4)->value.nativeObject.nativeInt64, execution_inline_frame_get_value_slot(state,currentFunction,base,5)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,5)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,5)->value.nativeObject.nativeInt64
end
continue
end
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:4343
commands
silent
if currentFunction == $targetFunction && programCounter - currentFunction->instructionsList >= 8 && programCounter - currentFunction->instructionsList <= 13
  printf "getstack before pc=%ld e=%u src=%u slot0 type=%u obj=%p int=%lld slot2 type=%u obj=%p int=%lld slot4 type=%u obj=%p int=%lld slot5 type=%u obj=%p int=%lld\n", programCounter - currentFunction->instructionsList, instruction.instruction.operandExtra, instruction.instruction.operand.operand2[0], execution_inline_frame_get_value_slot(state,currentFunction,base,0)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,0)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,0)->value.nativeObject.nativeInt64, execution_inline_frame_get_value_slot(state,currentFunction,base,2)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,2)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,2)->value.nativeObject.nativeInt64, execution_inline_frame_get_value_slot(state,currentFunction,base,4)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,4)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,4)->value.nativeObject.nativeInt64, execution_inline_frame_get_value_slot(state,currentFunction,base,5)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,5)->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,5)->value.nativeObject.nativeInt64
end
continue
end
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:4347
commands
silent
if currentFunction == $targetFunction && programCounter - currentFunction->instructionsList >= 8 && programCounter - currentFunction->instructionsList <= 13
  printf "getstack slow before pc=%ld e=%u src=%u\n", programCounter - currentFunction->instructionsList, instruction.instruction.operandExtra, instruction.instruction.operand.operand2[0]
end
continue
end
run
