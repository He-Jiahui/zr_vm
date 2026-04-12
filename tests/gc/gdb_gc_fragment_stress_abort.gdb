set pagination off
set confirm off
set print pretty on
set print elements 128
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/tests/benchmarks/cases/gc_fragment_stress/zr/benchmark_gc_fragment_stress.zrp
catch signal SIGABRT
commands
  silent
  printf "\n===== SIGABRT =====\n"
  bt 20
  frame 7
  printf "\n-- frame 7 locals --\n"
  info locals
  print fieldName
  print value
  if value
    print *value
    print value->type
    print value->value.object
    if value->value.object
      print value->value.object->type
      x/8gx value->value.object
    end
  end
  print object
  if object
    print *object
    print object->cachedHiddenItemsPair
    print object->cachedHiddenItemsObject
  end
  quit 1
end
run
