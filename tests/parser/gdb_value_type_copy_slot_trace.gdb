# 对 value_type_runtime 的内联值槽拷贝入口采样，比较来源和目标的布局及物理值；C 断言另作停止点。
# TODO: 长度 19 和目标槽 2/3/5 是旧用例的函数指纹，需运行当前带符号目标确认是否仍筛中预期场景。
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
