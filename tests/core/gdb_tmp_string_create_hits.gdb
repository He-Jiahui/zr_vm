set pagination off
set breakpoint pending on
set $hits = 0
break ZrCore_String_Create
commands
silent
set $hits = $hits + 1
printf "ZrCore_String_Create hit #%d len=%llu text=", $hits, (unsigned long long)length
x/s string
bt 3
if $hits >= 30
  quit
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
