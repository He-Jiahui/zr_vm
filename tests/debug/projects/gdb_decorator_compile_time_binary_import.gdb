set pagination off
set print pretty on
set print elements 0
set debuginfod enabled off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/decorator_compile_time_binary/decorator_compile_time_binary.zrp
run
bt
frame 7
info locals
print instruction
print closure
print closure->function
print callInfo
print *callInfo
