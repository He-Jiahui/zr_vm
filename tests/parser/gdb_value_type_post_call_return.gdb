set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:141
commands
silent
set $prev = callInfo->previous
set $calleeFn = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo)
set $callerFn = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, $prev)
set $callerBase = $prev->functionBase.valuePointer + 1
set $dest = callInfo->hasReturnDestination ? callInfo->returnDestination : callInfo->functionBase.valuePointer
set $destSlot = $dest - $callerBase
printf "postcall callee=%p caller=%p returnSource=%p dest=%p destSlot=%ld hasDest=%u expected=%u result slot logical type=%u obj=%p int=%lld physical type=%u obj=%p int=%lld\n", $calleeFn, $callerFn, returnSource, $dest, $destSlot, callInfo->hasReturnDestination, callInfo->expectedReturnCount, execution_inline_frame_get_value_slot(state,$callerFn,$callerBase,$destSlot)->type, execution_inline_frame_get_value_slot(state,$callerFn,$callerBase,$destSlot)->value.object, execution_inline_frame_get_value_slot(state,$callerFn,$callerBase,$destSlot)->value.nativeObject.nativeInt64, $dest->value.type, $dest->value.value.object, $dest->value.value.nativeObject.nativeInt64
if $calleeFn
  printf "  callee stack=%u frameBytes=%u pcReturn=%ld name=%s\n", $calleeFn->stackSize, $calleeFn->frameByteSize, callInfo->context.context.programCounter - $calleeFn->instructionsList, $calleeFn->functionName ? $calleeFn->functionName->string : "<anon>"
end
continue
end
run
