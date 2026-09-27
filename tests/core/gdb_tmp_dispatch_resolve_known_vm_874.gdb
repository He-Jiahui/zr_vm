# 原意是在 dispatch_loops 后段采样已知 VM 成员调用 PIC 的 receiver 与函数缓存。
# BUG: 当前 execution_member_access.c:874 位于缓存 receiver-pair 取值函数的 result 断言；
# cacheIndex、entry、receiver 不在此帧，原打印表达式与调用链已不符。
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
