set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:1541
commands
silent
set $hits = $hits + 1
if $hits < 1000
  continue
end
printf "member_set_cached@1541 hit #%d cacheIndex=%u picSlots=%u miss=%llu hit=%llu memberEntry=%u\n", $hits, cacheIndex, entry->picSlotCount, (unsigned long long)entry->runtimeMissCount, (unsigned long long)entry->runtimeHitCount, entry->memberEntryIndex
if entry->picSlotCount > 0
  print entry->picSlots[0].cachedAccessKind
  print entry->picSlots[0].cachedDescriptorIndex
  print entry->picSlots[0].cachedReceiverObject == (SZrObject*)receiverAndResult->value.object
  print entry->picSlots[0].cachedReceiverPair != 0
end
bt 3
if $hits >= 1003
  quit
end
continue
end
run
