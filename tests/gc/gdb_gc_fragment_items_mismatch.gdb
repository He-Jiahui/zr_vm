# 定位 __zr_items 字段值类型与原始对象类型不一致的调用路径。
# BUG: 条件断点现落在 module.c:293 的 zr_container_make_field_key，作用域没有 value；
# 运行到该处时条件求值失败，无法捕获预期失配。现行目标实现见 module.c:291 和 :1397。
set pagination off
set confirm off
set print pretty on
set print elements 128
set breakpoint pending on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args dumps/gc_stress_probe/benchmark_gc_stress_probe.zrp
cd /mnt/e/Git/zr_vm

break /mnt/e/Git/zr_vm/zr_vm_lib_container/src/zr_vm_lib_container/module.c:293 if fieldName && strcmp(fieldName, "__zr_items") == 0 && value != 0 && (value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != 0 && value->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT && value->value.object->type != ZR_RAW_OBJECT_TYPE_ARRAY
commands
  silent
  printf "\n===== __zr_items mismatch =====\n"
  printf "object=%p fieldName=%s value=%p\n", object, fieldName, value
  printf "object.super.type=%d internalType=%d storageKind=%d regionKind=%d status=%d generation=%d memberVersion=%u\n", object->super.type, object->internalType, object->super.garbageCollectMark.storageKind, object->super.garbageCollectMark.regionKind, object->super.garbageCollectMark.status, object->super.garbageCollectMark.generation, object->memberVersion
  printf "value.type=%d isGc=%d raw=%p raw.type=%d raw.storageKind=%d raw.regionKind=%d raw.status=%d raw.generation=%d\n", value->type, value->isGarbageCollectable, value->value.object, value->value.object->type, value->value.object->garbageCollectMark.storageKind, value->value.object->garbageCollectMark.regionKind, value->value.object->garbageCollectMark.status, value->value.object->garbageCollectMark.generation
  bt 12
  frame 0
  up
  info args
  info locals
  quit 0
end

run
