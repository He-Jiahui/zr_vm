set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:1026
commands
silent
if function
    printf "setMember entry fn=%p instr=%u stack=%u frameBytes=%u receiver=%u source=%u frameBase=%p top=%p tail=%p\n", function, function->instructionsLength, function->stackSize, function->frameByteSize, receiverSlot, sourceSlot, frameBase, state->stackTop.valuePointer, state->stackTail.valuePointer
    printf "  layout recv slot=%u kind=%u offset=%u size=%u align=%u type=%u\n", receiverLayout ? receiverLayout->stackSlot : 999, receiverLayout ? receiverLayout->slotKind : 999, receiverLayout ? receiverLayout->byteOffset : 999, receiverLayout ? receiverLayout->byteSize : 999, receiverLayout ? receiverLayout->byteAlign : 999, receiverLayout ? receiverLayout->typeLayoutId : 999
end
continue
end
run
