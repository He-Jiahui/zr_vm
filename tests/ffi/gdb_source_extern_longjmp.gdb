# 比较 longjmp 前后的调用帧与局部变量，辅助定位异常恢复路径。
set pagination off
set print pretty on
set breakpoint pending on
set confirm off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_ffi_test

break test_zr_ffi_source_extern_can_bind_and_call_symbol
run

break __longjmp
continue

bt 20
frame 1
info locals
frame 2
info locals
