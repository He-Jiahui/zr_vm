# 原意是在已知原生函数快速路径单测的旧栈槽断言点观察扩栈后的值。
# BUG: 当前 test_object_call_known_native_fast_path.c:569 是模块描述符 documentation 字段，
# staleCallableSlot/staleReceiverSlot/staleArgumentSlot 均不在该行作用域，断点已不能完成采样。
set pagination off
set confirm off
break /mnt/e/Git/zr_vm/tests/core/test_object_call_known_native_fast_path.c:569
commands
  silent
  printf "staleCallable.type=%d\n", staleCallableSlot->type
  printf "staleCallable.uint=%llu\n", (unsigned long long)staleCallableSlot->value.nativeObject.nativeUInt64
  printf "staleReceiver.type=%d\n", staleReceiverSlot->type
  printf "staleReceiver.ptr=%p\n", staleReceiverSlot->value.object
  printf "staleArgument.type=%d\n", staleArgumentSlot->type
  printf "staleArgument.ptr=%p\n", staleArgumentSlot->value.object
  continue
end
run
