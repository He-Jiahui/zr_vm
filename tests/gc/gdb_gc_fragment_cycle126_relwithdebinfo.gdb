set pagination off
set print pretty on
set print elements 0
set confirm off
handle SIGSEGV stop print pass
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-relwithdebinfo-ci-make/bin/zr_vm_cli
set args /tmp/zr_gc_cycle126/benchmark_gc_fragment_stress.zrp
run
bt
frame 0
info locals
frame 1
info locals
frame 2
info locals
quit
