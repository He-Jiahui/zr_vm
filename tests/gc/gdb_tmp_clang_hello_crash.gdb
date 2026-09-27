# 用 clang Debug CLI 跑 hello_world 项目并在异常退出后取得调用栈。
set pagination off
set print elements 0
file /mnt/e/Git/zr_vm/build/codex-wsl-clang-debug/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/tests/fixtures/projects/hello_world/hello_world.zrp
run
bt
quit
