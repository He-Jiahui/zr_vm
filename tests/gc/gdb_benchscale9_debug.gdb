set pagination off
set confirm off
set print pretty on
set print elements 64
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/benchscale_9/benchscale_9.zrp
catch signal SIGABRT
commands
  silent
  printf "\n=== SIGABRT ===\n"
  bt 20
  frame 7
  printf "\n-- frame 7 --\n"
  info locals
  print fieldName
  print value
  if value
    print *value
    print value->type
    print value->value.object
    if value->value.object
      print *value->value.object
    end
  end
  print object
  if object
    print *object
    print object->cachedHiddenItemsPair
    if object->cachedHiddenItemsPair
      print *object->cachedHiddenItemsPair
    end
  end
  quit 1
end
catch signal SIGSEGV
commands
  silent
  printf "\n=== SIGSEGV ===\n"
  bt 20
  frame 0
  info locals
  frame 1
  info locals
  frame 2
  info locals
  frame 3
  info locals
  quit 1
end
run
