set pagination off
file /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug/bin/zr_vm_cli
set args /mnt/d/Git/Github/zr_vm_mig/zr_vm/tests/fixtures/projects/decorator_compile_time_import/decorator_compile_time_import.zrp
break execution_raise_vm_runtime_error
run
bt
frame 1
info locals
frame 7
info locals
quit
