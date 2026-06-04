set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
set $targetFunction = 0
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:6445
commands
silent
if programCounter - currentFunction->instructionsList == 8
  set $targetFunction = currentFunction
  printf "target fn=%p\n", $targetFunction
end
continue
end
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:4353
commands
silent
if currentFunction == $targetFunction
  set $dst = instruction.instruction.operandExtra
  set $src = instruction.instruction.operand.operand2[0]
  set $dstv = execution_inline_frame_get_value_slot(state,currentFunction,base,$dst)
  set $srcv = execution_inline_frame_get_value_slot(state,currentFunction,base,$src)
  set $dl = ZrCore_Function_FindFrameSlotLayout(currentFunction,$dst)
  set $sl = ZrCore_Function_FindFrameSlotLayout(currentFunction,$src)
  printf "setstack pc=%ld dst=%u src=%u dstType=%u srcType=%u\n", programCounter - currentFunction->instructionsList, $dst, $src, $dstv->type, $srcv->type
  if $dl
    printf "  dst layout kind=%u typeLayout=%u\n", $dl->slotKind, $dl->typeLayoutId
  end
  if $sl
    printf "  src layout kind=%u typeLayout=%u\n", $sl->slotKind, $sl->typeLayoutId
  end
end
continue
end
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:4343
commands
silent
if currentFunction == $targetFunction
  set $dst = instruction.instruction.operandExtra
  set $src = instruction.instruction.operand.operand2[0]
  set $dstv = execution_inline_frame_get_value_slot(state,currentFunction,base,$dst)
  set $srcv = execution_inline_frame_get_value_slot(state,currentFunction,base,$src)
  set $dl = ZrCore_Function_FindFrameSlotLayout(currentFunction,$dst)
  set $sl = ZrCore_Function_FindFrameSlotLayout(currentFunction,$src)
  printf "getstack pc=%ld dst=%u src=%u dstType=%u srcType=%u\n", programCounter - currentFunction->instructionsList, $dst, $src, $dstv->type, $srcv->type
  if $dl
    printf "  dst layout kind=%u typeLayout=%u\n", $dl->slotKind, $dl->typeLayoutId
  end
  if $sl
    printf "  src layout kind=%u typeLayout=%u\n", $sl->slotKind, $sl->typeLayoutId
  end
end
continue
end
run
