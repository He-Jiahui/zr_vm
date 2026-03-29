set pagination off
set print pretty on
set breakpoint pending on
set confirm off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_ffi_test

break test_zr_ffi_source_extern_can_bind_and_call_symbol
run

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:267
continue

printf "ffi error code=%d\n", code
bt 16
