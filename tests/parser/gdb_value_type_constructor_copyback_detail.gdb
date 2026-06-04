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
