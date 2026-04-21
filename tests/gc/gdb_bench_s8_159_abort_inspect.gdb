set pagination off
set confirm off
set print pretty on
set print elements 0
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_s8_159/bench_s8_159.zrp
catch signal SIGABRT
commands
  silent
  printf "\n=== SIGABRT ===\n"
  bt 16
  frame 7
  printf "\n--- zr_container_get_object_field_fast ---\n"
  info args
  info locals
  p value
  p *value
  p value->type
  p value->value.object
  p value->value.object->type
  p object
  p object->internalType
  p ((SZrRawObject*)object)->garbageCollectMark.regionKind
  p ((SZrRawObject*)object)->garbageCollectMark.storageKind
  p ((SZrRawObject*)object)->garbageCollectMark.rememberedRegistryIndex
  p object->cachedHiddenItemsPair
  p *object->cachedHiddenItemsPair
  p object->cachedHiddenItemsPair->key
  p object->cachedHiddenItemsPair->value
  p object->cachedHiddenItemsObject
  p object->cachedHiddenItemsObject->internalType
  p object->cachedStringLookupPair
  quit 1
end
run
