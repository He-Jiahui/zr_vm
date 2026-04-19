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
