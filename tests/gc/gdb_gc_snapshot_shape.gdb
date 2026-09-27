# 检查 GC 快照中的区域数量、存活字节与对象所属区域。
# BUG: 当前断点 gc_tests.c:1298 在 ownership 测试结尾，snapshot/gc/oldObject 等均不在作用域；
# 执行后 print 失败，目标快照测试实际从 test_gc_snapshot_reports_region_pressure_shape 开始。
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
