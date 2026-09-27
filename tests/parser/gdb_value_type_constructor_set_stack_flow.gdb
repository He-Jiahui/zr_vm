# 原意是锁定特定构造器函数，并追踪调用、SET_STACK 与 GET_STACK 前的槽布局和值。
# BUG: 下列三个固定行号现分别在 TO_UINT_FLOAT 与动态尾调用分支，不再是所标注的指令入口；采样会误指其他操作。
# TODO: 改断点前还需验证 pc==8 这个旧函数指纹是否仍对应目标构造器。
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
