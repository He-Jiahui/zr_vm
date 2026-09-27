set pagination off
set confirm off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
# BUG: 从仓库根目录运行时，此根 tests 项目路径不存在；断点脚本需要以归档树中的 aot_eh_tail_gc_stress 样例为输入才有所有权时间线。
set args --execution-mode aot_c --require-aot-path --emit-executed-via ./tests/fixtures/projects/aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp

# 按第一次升级、两个释放点、释放后升级的顺序观察控制块是否符合弱引用契约。
break ZrLibrary_AotRuntime_OwnUpgrade if destinationSlot==7
commands
silent
printf "\n[own-upgrade alias] dst=%u src=%u\n", destinationSlot, sourceSlot
print frame->slotBase[sourceSlot].value
print frame->slotBase[sourceSlot].value.ownershipControl
print frame->slotBase[sourceSlot].value.ownershipControl->strongRefCount
print frame->slotBase[sourceSlot].value.ownershipControl->weakRefs
print frame->slotBase[sourceSlot].value.ownershipControl->object
continue
end

break ZrLibrary_AotRuntime_OwnRelease if destinationSlot==12 || destinationSlot==13
commands
silent
printf "\n[own-release] dst=%u src=%u\n", destinationSlot, sourceSlot
print frame->slotBase[sourceSlot].value
print frame->slotBase[sourceSlot].value.ownershipControl
print frame->slotBase[sourceSlot].value.ownershipControl->strongRefCount
print frame->slotBase[sourceSlot].value.ownershipControl->weakRefs
print frame->slotBase[sourceSlot].value.ownershipControl->object
continue
end

break ZrLibrary_AotRuntime_OwnUpgrade if destinationSlot==14
commands
silent
printf "\n[own-upgrade after] dst=%u src=%u\n", destinationSlot, sourceSlot
print frame->slotBase[6].value
print frame->slotBase[sourceSlot].value
print frame->slotBase[sourceSlot].value.ownershipControl
print frame->slotBase[sourceSlot].value.ownershipControl->strongRefCount
print frame->slotBase[sourceSlot].value.ownershipControl->weakRefs
print frame->slotBase[sourceSlot].value.ownershipControl->object
continue
end

run
bt full
