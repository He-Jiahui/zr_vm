set pagination off
set confirm off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args --execution-mode aot_c --require-aot-path --emit-executed-via ./tests/fixtures/projects/aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp

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
