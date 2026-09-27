# 原意是在 SET_MEMBER 处理接收者时查其内联帧槽布局，核对操作数和缓存索引。
# BUG: execution_dispatch.c:1887 当前在缓存辅助函数边界，非成员赋值入口；currentFunction/instruction 不在此作用域。
set pagination off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:1887
commands
silent
# BUG: 执行循环的 instruction 是 TZrInstruction 值而非指针；迁到正确 SET_MEMBER 停点后，下面的 -> 表达式仍会报错。
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
