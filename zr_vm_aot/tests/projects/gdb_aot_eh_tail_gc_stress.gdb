set pagination off
set confirm off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
# BUG: 项目样例实际位于 zr_vm_aot/tests/fixtures/projects；从仓库根目录启动时此根 tests 路径不存在，CLI 不能进入待调试的 AOT 场景。
set args --execution-mode aot_c --require-aot-path --emit-executed-via ./tests/fixtures/projects/aot_eh_tail_gc_stress/aot_eh_tail_gc_stress.zrp
run
bt full
frame 0
info locals
