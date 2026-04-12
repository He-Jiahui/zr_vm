set pagination off
set confirm off
set print pretty on
set print elements 128
set breakpoint pending on
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/tests/benchmarks/cases/gc_fragment_stress/zr/benchmark_gc_fragment_stress.zrp
break zr_container_get_object_field
commands
  silent
  if fieldName && (strcmp(fieldName, "__zr_entries") == 0 || strcmp(fieldName, "__zr_items") == 0 || strcmp(fieldName, "__zr_source") == 0)
    printf "\nFIELD=%s object=%p\n", fieldName, object
    bt 8
  end
  continue
end
catch signal SIGABRT
commands
  silent
  printf "\n===== SIGABRT =====\n"
  bt full 20
  frame 1
  info locals
  up
  info locals
  quit 1
end
run
