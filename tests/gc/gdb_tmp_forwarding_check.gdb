set pagination off
set confirm off
set print pretty on
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_array_plus_map/bench_array_plus_map.zrp
catch signal SIGABRT
commands
  silent
  frame 7
  printf "\n=== crash object/value forwarding ===\n"
  p value->value.object
  p value->value.object->garbageCollectMark.forwardingAddress
  p value->value.object->garbageCollectMark.status
  p value->value.object->garbageCollectMark.storageKind
  p value->value.object->garbageCollectMark.regionKind
  p value->value.object->garbageCollectMark.generation
  p value->value.object->garbageCollectMark.minorScanEpoch
  p value->value.object->garbageCollectMark.survivalAge
  p value->value.object->garbageCollectMark.promotionReason
  p value->value.object->gcList
  quit 1
end
run
