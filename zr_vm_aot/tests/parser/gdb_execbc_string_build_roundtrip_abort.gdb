set pagination off
# 针对字符串构建回环的进程中止，保留故障现场与上层调用者局部变量。
set confirm off
set print thread-events off
handle SIGABRT stop print nopass
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug/lib
file ./build/codex-wsl-current-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test
run
bt
frame 1
info locals
up
info locals
quit
