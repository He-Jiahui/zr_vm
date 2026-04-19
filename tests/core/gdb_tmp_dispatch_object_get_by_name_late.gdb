set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break object_get_own_string_value_by_name_cached_unchecked
commands
silent
set $hits = $hits + 1
if $hits < 200
  continue
end
printf "object_get_by_name_cached hit #%d\n", $hits
bt 6
if $hits >= 205
  quit
end
continue
end
run
