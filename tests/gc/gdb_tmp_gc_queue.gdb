set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_array_plus_map/bench_array_plus_map.zrp
catch signal SIGABRT
commands
  silent
  frame 7
  printf "\n=== collector queue state at crash ===\n"
  p state->global->garbageCollector->gcRunningStatus
  p state->global->garbageCollector->collectionPhase
  p state->global->garbageCollector->waitToScanObjectList
  p state->global->garbageCollector->waitToScanAgainObjectList
  p state->global->garbageCollector->gcStatus
  quit 1
end
run
