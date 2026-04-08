set pagination off
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
