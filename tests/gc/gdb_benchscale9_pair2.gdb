set pagination off
set confirm off
set print pretty on
file /mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/benchscale_9/benchscale_9.zrp
catch signal SIGABRT
commands
  silent
  printf "\n=== SIGABRT ===\n"
  bt 12
  frame 7
  print fieldName
  print value
  print *value
  print object
  print *object
  print object->cachedHiddenItemsPair
  if object->cachedHiddenItemsPair
    print *object->cachedHiddenItemsPair
    print object->cachedHiddenItemsPair->value
  end
  quit 1
end
catch signal SIGSEGV
commands
  silent
  printf "\n=== SIGSEGV ===\n"
  bt 16
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
