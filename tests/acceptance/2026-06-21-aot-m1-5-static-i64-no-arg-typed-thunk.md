# AOT M1.5 07-S5 Static I64 No-Arg Typed Thunk

Date: 2026-06-22 05:42:18 +08:00

## Scope

This slice adds the first narrow typed-to-typed direct C call path for static no-argument functions with int64 return metadata returning a constant signed integer.

Covered:

- The AOT C emitter recognizes typed no-argument constant-i64 return functions.
- Generated C emits `zr_aot_typed_i64_fn_N` forward declarations and definitions returning `TZrInt64`.
- Function-body lowering routes eligible static no-arg i64 calls directly to the typed thunk instead of `ZrLibrary_AotRuntime_CallStaticDirect()` or `ZrLibrary_AotRuntime_CallStackValue()`.
- The direct writer assigns the destination scalar local and syncs the current destination stack slot with `ZR_VALUE_FAST_SET` for remaining frame-backed consumers.
- A dedicated shared-library smoke executes `answer(): int { return 42; }`, verifies runtime result `43`, and rejects the old helper call path in generated source.

Out of scope:

- Typed parameter ABI and argument marshaling.
- General typed returns beyond no-arg constant i64.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The new call contract first failed because generated source and backend declarations lacked:

- `backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function)`.
- `backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file, ...)`.
- `backend_aot_c_try_get_i64_constant_return(...)`.
- `backend_aot_write_c_typed_i64_thunk_forward_decls(...)`.
- `backend_aot_write_c_typed_i64_thunks(...)`.
- The generated `zr_aot_typed_i64_fn_N` thunk and `zr_aot_static_i64_no_arg_direct_call` marker.

The new typed direct-call shared-library smoke also failed before implementation because generated C had no typed thunk/direct-call shape.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 8/0.

Validation command:

```sh
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test'
```

## Notes

This is a deliberately small typed direct-call foothold. The stack-slot sync remains because parts of the current generated pipeline still consume frame-backed values after the direct call. Removing that sync belongs to later 07 work that eliminates those consumers.
