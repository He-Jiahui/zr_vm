set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break execution_member_try_cached_instance_field_object_get
commands
silent
set $hits = $hits + 1
printf "object_get_fallback hit #%d descriptor_index=%u receiver=%p member=%p\n", $hits, slot->cachedDescriptorIndex, object, memberName
bt 5
if $hits >= 5
  quit
end
continue
end
run
