set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:874
commands
silent
set $hits = $hits + 1
if $hits < 500
  continue
end
printf "resolve_known_vm@874 hit #%d cacheIndex=%u picSlots=%u argCount=%u\n", $hits, cacheIndex, entry->picSlotCount, entry->argumentCount
if entry->picSlotCount > 0
  print entry->picSlots[0].cachedFunction != 0
  print entry->picSlots[0].cachedReceiverObject == (SZrObject*)receiver->value.object
  print entry->picSlots[0].cachedReceiverPrototype == ((SZrObject*)receiver->value.object)->prototype
  print entry->picSlots[0].cachedFunction != 0 ? entry->picSlots[0].cachedFunction->closureValueLength : 999
end
bt 3
if $hits >= 503
  quit
end
continue
end
run
