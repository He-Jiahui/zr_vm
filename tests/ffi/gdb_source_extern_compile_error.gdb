set pagination off
set print pretty on
set breakpoint pending on
set confirm off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_ffi_test

break test_zr_ffi_source_extern_can_bind_and_call_symbol
run

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_parser/src/zr_vm_parser/compiler.c:524
continue

printf "compiler error: %s\n", msg
printf "location line=%u column=%u\n", location.line, location.column
bt 12
