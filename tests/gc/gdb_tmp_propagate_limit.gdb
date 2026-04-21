set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_array_plus_map/bench_array_plus_map.zrp
break gc_mark.c:906
commands
  silent
  printf "\n=== propagate limit hit ===\n"
  bt 6
  p iterationCount
  p maxIterations
  p global->garbageCollector->waitToScanObjectList
  p global->garbageCollector->waitToScanAgainObjectList
  quit 2
end
run
