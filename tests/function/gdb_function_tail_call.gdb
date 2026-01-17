# GDB 脚本：在 FUNCTION_TAIL_CALL 断言处断点，查看 opA、base、BASE(functionSlot)
# 用法: cd build && gdb -x ../tests/function/gdb_function_tail_call.gdb --args ./bin/zr_vm_named_arguments_test

set pagination off
set print pretty on
set breakpoint pending on

# 在 ZrExecute 内、FUNCTION_TAIL_CALL 的 opA 赋值之后断点（execution.c 约 1469 行）
# 使用绝对路径以便在共享库加载后能解析
break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_core/src/zr_vm_core/execution.c:1469
commands
  silent
  printf "===== FUNCTION_TAIL_CALL 断点 =====\n"
  printf "functionSlot = %u\n", (unsigned)functionSlot
  printf "base = %p\n", base
  printf "BASE(functionSlot) = %p\n", (void*)(base + functionSlot)
  printf "opA = %p\n", opA
  printf "opA->type = %d (15=FUNCTION,17=CLOSURE,0=NULL,5=INT32)\n", opA->type
  printf "opA->value.object = %p\n", opA->value.object
  printf "callInfo->functionBase.valuePointer = %p\n", callInfo->functionBase.valuePointer
  printf "callInfo->previous = %p\n", callInfo->previous
  printf "====================================\n"
  continue
end

run
quit
