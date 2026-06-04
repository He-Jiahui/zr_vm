set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:6445
commands
silent
if programCounter - currentFunction->instructionsList == 8
  printf "target function=%p stack=%u frameBytes=%u locals=%u layouts=%u constants=%u\n", currentFunction, currentFunction->stackSize, currentFunction->frameByteSize, currentFunction->localVariableLength, currentFunction->frameSlotLayoutLength, currentFunction->constantValueLength
  set $i = 0
  while $i < currentFunction->localVariableLength
    set $local = currentFunction->localVariableList[$i]
    printf "  local[%u] slot=%u start=%ld end=%ld name=%p\n", $i, $local.stackSlot, $local.offsetActivate, $local.offsetDead, $local.name
    set $i = $i + 1
  end
  set $i = 0
  while $i < currentFunction->frameSlotLayoutLength
    set $layout = currentFunction->frameSlotLayouts[$i]
    printf "  layout[%u] slot=%u kind=%u offset=%u size=%u align=%u typeLayout=%u\n", $i, $layout.stackSlot, $layout.slotKind, $layout.byteOffset, $layout.byteSize, $layout.byteAlign, $layout.typeLayoutId
    set $i = $i + 1
  end
  set $i = 0
  while $i < currentFunction->constantValueLength
    set $value = currentFunction->constantValueList[$i]
    printf "  const[%u] type=%u native=%u obj=%p int=%lld\n", $i, $value.type, $value.isNative, $value.value.object, $value.value.nativeObject.nativeInt64
    set $i = $i + 1
  end
end
continue
end
run
