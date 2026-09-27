# 在 value_type_runtime 中观察不可调用值的错误入口及其调用栈；目标是定位失败调用的实参布局。
# BUG: CallError 的直接调用方是 function_get_meta_call，frame 1 没有下面读取的执行循环局部变量；命中后首个执行帧 printf 报错，后续槽位采样失效。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break ZrCore_Debug_CallError
commands
silent
printf "call error value=%p type=%u obj=%p int=%lld\n", value, value ? value->type : 255, value ? value->value.object : 0, value ? value->value.nativeObject.nativeInt64 : 0
bt 8
frame 1
printf "execute pc=%ld fn=%p stack=%u frameBytes=%u E=%u A1=%u B1=%u opcode=%u\n", programCounter - currentFunction->instructionsList, currentFunction, currentFunction->stackSize, currentFunction->frameByteSize, instruction->instruction.operandExtra, instruction->instruction.operand.operand1[0], instruction->instruction.operand.operand1[1], instruction->instruction.operationCode
set $slot = 0
while $slot < currentFunction->stackSize
  set $logical = execution_inline_frame_get_value_slot(state, currentFunction, base, $slot)
  printf "  slot %u logical type=%u obj=%p int=%lld physical type=%u obj=%p int=%lld\n", $slot, $logical->type, $logical->value.object, $logical->value.nativeObject.nativeInt64, base[$slot].value.type, base[$slot].value.value.object, base[$slot].value.value.nativeObject.nativeInt64
  set $slot = $slot + 1
end
continue
end
run
