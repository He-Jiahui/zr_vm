# 原意是在构造器接收者回写时比对调用方实参槽与返回目标槽的布局。
# BUG: function.c:2021 现在属于泛型调用包装器参数声明，不在回写函数内；断点命中后 callInfo 等局部量不可用。
# TODO: 迁移到当前回写入口后仍需按 hasArgumentSourceFrame/argumentSourceStartSlot 计算实参源槽，不能一律用相邻帧指针差。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/function.c:2021
commands
silent
set $calleeFn = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, (SZrCallInfo *)callInfo)
set $callerFn = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo->previous)
set $callerBase = callInfo->previous->functionBase.valuePointer + 1
set $argSlot = (callInfo->functionBase.valuePointer + 1) - $callerBase
set $dest = callInfo->hasReturnDestination ? callInfo->returnDestination : 0
set $destSlot = $dest ? $dest - $callerBase : -1
printf "copyback callee=%p caller=%p argSlot=%ld hasDest=%u destSlot=%ld\n", $calleeFn, $callerFn, $argSlot, callInfo->hasReturnDestination, $destSlot
if $callerFn && $argSlot >= 0
  set $argLayout = ZrCore_Function_FindFrameSlotLayout($callerFn, $argSlot)
  if $argLayout
    printf "  arg layout kind=%u typeLayout=%u\n", $argLayout->slotKind, $argLayout->typeLayoutId
  end
end
if $callerFn && $destSlot >= 0
  set $destLayout = ZrCore_Function_FindFrameSlotLayout($callerFn, $destSlot)
  if $destLayout
    printf "  dest layout kind=%u typeLayout=%u\n", $destLayout->slotKind, $destLayout->typeLayoutId
  else
    printf "  dest layout <none>\n"
  end
end
continue
end
run
