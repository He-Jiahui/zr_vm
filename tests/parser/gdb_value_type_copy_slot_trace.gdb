set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break execution_inline_frame_try_copy_stack_slot
commands
silent
if function && function->instructionsLength == 19 && (destinationSlot == 2 || destinationSlot == 3 || destinationSlot == 5)
    printf "copySlot dst=%u src=%u dstKind=%u srcKind=%u dstPhys=%u/%p srcPhys=%u/%p\n", destinationSlot, sourceSlot, ZrCore_Function_FindFrameSlotLayout(function, destinationSlot) ? ZrCore_Function_FindFrameSlotLayout(function, destinationSlot)->slotKind : 255, ZrCore_Function_FindFrameSlotLayout(function, sourceSlot) ? ZrCore_Function_FindFrameSlotLayout(function, sourceSlot)->slotKind : 255, frameBase[destinationSlot].value.type, frameBase[destinationSlot].value.value.object, frameBase[sourceSlot].value.type, frameBase[sourceSlot].value.value.object
end
continue
end
break __assert_fail
run
