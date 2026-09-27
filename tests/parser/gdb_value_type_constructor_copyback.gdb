# 对 value_type_runtime 的构造器接收者回写入口采样，核对元函数名和相邻调用帧；需带符号的 GCC Debug 构建。
# 这是调用完成后的回写诊断；同一单测内其他构造器也可能命中，不表示特定测试场景已失败。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break ZrCore_Function_TryCopyInlineConstructorReceiverBack
commands
silent
set $fn = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, (SZrCallInfo *)callInfo)
printf "copyback fn=%p name=%p meta=%p prev=%p base=%p\n", $fn, $fn ? $fn->functionName : 0, state->global->metaFunctionName[0], callInfo ? callInfo->previous : 0, callInfo ? callInfo->functionBase.valuePointer : 0
if $fn && $fn->functionName && state->global->metaFunctionName[0]
# TODO: %s 读取 stringDataExtend 只适用于短字符串；需核对命中的函数名是否可能走 longString 存储，并改用通用取值入口。
    printf "  nameShortLen=%u metaShortLen=%u nameChars=%s metaChars=%s\n", $fn->functionName->shortStringLength, state->global->metaFunctionName[0]->shortStringLength, $fn->functionName->stringDataExtend, state->global->metaFunctionName[0]->stringDataExtend
end
continue
end
break __assert_fail
run
