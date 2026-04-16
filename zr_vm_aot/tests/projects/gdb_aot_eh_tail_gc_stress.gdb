set pagination off
set confirm off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args --execution-mode aot_c --require-aot-path --emit-executed-via ./tests/fixtures/projects/aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp
run
bt full
frame 0
info locals
