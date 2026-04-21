set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_array_plus_map/bench_array_plus_map.zrp
catch signal SIGABRT
commands
  silent
  frame 7
  printf "\n=== crash owner mark state ===\n"
  p object
  p ((SZrRawObject*)object)->garbageCollectMark.status
  p ((SZrRawObject*)object)->garbageCollectMark.generation
  p ((SZrRawObject*)object)->garbageCollectMark.minorScanEpoch
  p ((SZrRawObject*)object)->garbageCollectMark.forwardingAddress
  p ((SZrRawObject*)object)->garbageCollectMark.rememberedRegistryIndex
  quit 1
end
run
