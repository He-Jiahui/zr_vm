set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break execution_member_get_cached
commands
silent
set $hits = $hits + 1
if $hits < 1000
  continue
end
printf "member_get_cached hit #%d cacheIndex=%u receiver=%p result=%p\n", $hits, cacheIndex, receiver, result
print entry
if entry != 0
  print entry->picSlotCount
  print entry->runtimeHitCount
  print entry->runtimeMissCount
end
bt 4
if $hits >= 1003
  quit
end
continue
end
run
