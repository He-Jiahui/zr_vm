set pagination off
set confirm off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args --execution-mode aot_c --require-aot-path --emit-executed-via ./tests/fixtures/projects/aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp
break ZrLibrary_AotRuntime_OwnUpgrade if destinationSlot==14
commands
silent
printf "hit own upgrade destination=%u source=%u\n", destinationSlot, sourceSlot
print frame->slotBase[6].value
print frame->slotBase[15].value
print frame->slotBase[15].value.ownershipControl
print frame->slotBase[15].value.ownershipControl->strongRefCount
print frame->slotBase[15].value.ownershipControl->object
continue
end
run
bt full
