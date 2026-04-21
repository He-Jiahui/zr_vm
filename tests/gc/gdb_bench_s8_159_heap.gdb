set pagination off
set confirm off
set print pretty on
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_s8_159/bench_s8_159.zrp
catch signal SIGABRT
commands
  silent
  printf "\n=== SIGABRT ===\n"
  bt 25
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
catch signal SIGSEGV
commands
  silent
  printf "\n=== SIGSEGV ===\n"
  bt 25
  frame 0
  info locals
  frame 1
  info locals
  quit 1
end
run
