set pagination off
set confirm off
set print pretty on
set print elements 128
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/benchmark_gc_fragment_stress.zrp
catch signal SIGABRT
commands
  silent
  printf "\n===== SIGABRT =====\n"
  bt full 25
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
      print *value->value.object
      x/8gx value->value.object
    end
  end
  print object
  if object
    print *object
    x/8gx object
  end
  frame 8
  printf "\n-- frame 8 locals --\n"
  info locals
  frame 9
  printf "\n-- frame 9 locals --\n"
  info locals
  quit 1
end
catch signal SIGSEGV
commands
  silent
  printf "\n===== SIGSEGV =====\n"
  bt full 25
  frame 0
  printf "\n-- frame 0 locals --\n"
  info locals
  frame 1
  printf "\n-- frame 1 locals --\n"
  info locals
  print value
  if value
    print *value
    print value->type
    print value->value.object
    if value->value.object
      print *value->value.object
      x/8gx value->value.object
    end
  end
  frame 2
  printf "\n-- frame 2 locals --\n"
  info locals
  print pair
  if pair
    print *pair
    x/8gx pair
  end
  quit 1
end
run
