set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break ZrCore_Function_TryCopyInlineConstructorReceiverBack
commands
silent
set $fn = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, (SZrCallInfo *)callInfo)
printf "copyback fn=%p name=%p meta=%p prev=%p base=%p\n", $fn, $fn ? $fn->functionName : 0, state->global->metaFunctionName[0], callInfo ? callInfo->previous : 0, callInfo ? callInfo->functionBase.valuePointer : 0
if $fn && $fn->functionName && state->global->metaFunctionName[0]
    printf "  nameShortLen=%u metaShortLen=%u nameChars=%s metaChars=%s\n", $fn->functionName->shortStringLength, state->global->metaFunctionName[0]->shortStringLength, $fn->functionName->stringDataExtend, state->global->metaFunctionName[0]->stringDataExtend
end
continue
end
break __assert_fail
run
