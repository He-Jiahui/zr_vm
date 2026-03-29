set pagination off
set print pretty on
set breakpoint pending on
set confirm off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_ffi_test

break main
run

break zr_ffi_symbol_invoke_array

call test_zr_ffi_source_extern_can_bind_and_call_symbol()

printf "symbol=%s\n", symbolData->symbolName
printf "paramCount=%llu\n", (unsigned long long)symbolData->signature->parameterCount
printf "param0kind=%d param1kind=%d returnkind=%d\n", symbolData->signature->parameters[0].type->kind, symbolData->signature->parameters[1].type->kind, symbolData->signature->returnType->kind
bt 8
