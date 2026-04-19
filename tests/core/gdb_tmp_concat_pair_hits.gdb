set pagination off
set breakpoint pending on
set $hits = 0
break ZrCore_String_ConcatPair
commands
silent
set $hits = $hits + 1
printf "concat_pair hit #%d left=%p right=%p leftLen=%llu rightLen=%llu\n", $hits, left, right, (unsigned long long)left->shortStringLength, (unsigned long long)right->shortStringLength
if $hits >= 12
  quit
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
