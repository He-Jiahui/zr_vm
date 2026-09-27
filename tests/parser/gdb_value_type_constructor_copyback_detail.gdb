# 原意是在构造器回写布局匹配处观察接收者和调用方目标槽的物理值。
# BUG: function.c:2049 现在是泛型参数有效性检查，缺少 callerArgumentStartSlot/receiverLayout/destinationLayout；采样表达式失效。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/function.c:2049
commands
silent
printf "copyback detail argStart=%ld recvKind=%u recvType=%u dstKind=%u dstType=%u calleeSlot0Phys=%u/%p callerDstPhys=%u/%p\n", callerArgumentStartSlot, receiverLayout->slotKind, receiverLayout->typeLayoutId, destinationLayout->slotKind, destinationLayout->typeLayoutId, calleeFrameBase[0].value.type, calleeFrameBase[0].value.value.object, callerFrameBase[callerArgumentStartSlot].value.type, callerFrameBase[callerArgumentStartSlot].value.value.object
continue
end
break __assert_fail
run
