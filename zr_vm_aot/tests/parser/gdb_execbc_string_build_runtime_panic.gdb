set pagination off
# 在测试 panic 派发处停下，关联 Unity 用例名与触发时的调用栈。
set confirm off
set print thread-events off
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug/lib
file ./build/codex-wsl-current-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test
break zr_tests_runtime_panic_handler_dispatch
run
print Unity.CurrentTestName
bt
frame 1
info locals
up
info locals
quit
