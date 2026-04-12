set pagination off
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug/bin/zr_vm_gc_test
break tests/gc/gc_tests.c:1298
run
print snapshot.managedMemoryBytes
print snapshot.regionCount
print snapshot.edenRegionCount
print snapshot.oldRegionCount
print snapshot.pinnedRegionCount
print snapshot.permanentRegionCount
print snapshot.edenLiveBytes
print snapshot.oldLiveBytes
print snapshot.pinnedLiveBytes
print snapshot.permanentLiveBytes
print oldObject->garbageCollectMark.regionKind
print oldObject->garbageCollectMark.storageKind
print oldObject->garbageCollectMark.regionId
print pinnedObject->garbageCollectMark.regionKind
print pinnedObject->garbageCollectMark.regionId
print permanentObject->garbageCollectMark.regionKind
print permanentObject->garbageCollectMark.regionId
print gc->regionCount
set $i = 0
while $i < gc->regionCount
  print gc->regions[$i].kind
  print gc->regions[$i].id
  print gc->regions[$i].liveObjectCount
  print gc->regions[$i].liveBytes
  print gc->regions[$i].usedBytes
  set $i = $i + 1
end
quit
