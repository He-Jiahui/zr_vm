set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break ZrCore_Object_GetMemberCachedDescriptorUnchecked
commands
silent
set $hits = $hits + 1
printf "cached_descriptor_get hit #%d\n", $hits
bt 6
if $hits >= 8
  quit
end
continue
end
run
