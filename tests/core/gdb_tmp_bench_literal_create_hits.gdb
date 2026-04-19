set pagination off
set breakpoint pending on
set $hits = 0
break ZrCore_String_Create
commands
silent
if (((length == 2) && ((string[0] == 'a') || (string[0] == 'b') || (string[0] == 'c') || (string[0] == 'd'))) || ((length == 5) && (string[0] == '_')))
  set $hits = $hits + 1
  printf "bench-literal create hit #%d len=%llu text=", $hits, (unsigned long long)length
  x/s string
  bt 4
  if $hits >= 20
    quit
  end
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
