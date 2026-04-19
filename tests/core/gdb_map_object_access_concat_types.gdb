set pagination off
set breakpoint pending on
set $concat_hits = 0
break concat_values_to_destination
commands
silent
set $concat_hits = $concat_hits + 1
printf "concat hit #%d opA=%d opB=%d safe=%d\n", $concat_hits, opA->type, opB->type, safeMode
if $concat_hits >= 5
    quit
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
