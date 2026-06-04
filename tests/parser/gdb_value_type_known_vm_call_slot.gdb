set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:6455
commands
silent
set $functionSlot = instruction.instruction.operand.operand1[0]
set $parametersCount = instruction.instruction.operand.operand1[1]
set $resultSlot = instruction.instruction.operandExtra
printf "known vm call pc=%ld fn=%p stack=%u frameBytes=%u result=%u functionSlot=%u parameters=%u\n", programCounter - currentFunction->instructionsList, currentFunction, currentFunction->stackSize, currentFunction->frameByteSize, $resultSlot, $functionSlot, $parametersCount
set $logical = execution_inline_frame_get_value_slot(state, currentFunction, base, $functionSlot)
printf "  logical function slot %u value@%p type=%u obj=%p int=%lld\n", $functionSlot, $logical, $logical->type, $logical->value.object, $logical->value.nativeObject.nativeInt64
printf "  physical function slot %u type=%u obj=%p int=%lld\n", $functionSlot, base[$functionSlot].value.type, base[$functionSlot].value.value.object, base[$functionSlot].value.value.nativeObject.nativeInt64
set $layout = ZrCore_Function_FindFrameSlotLayout(currentFunction, $functionSlot)
if $layout
printf "  layout kind=%u byteOffset=%u byteSize=%u typeLayout=%u\n", $layout->slotKind, $layout->byteOffset, $layout->byteSize, $layout->typeLayoutId
else
printf "  layout <none>\n"
end
continue
end
run
