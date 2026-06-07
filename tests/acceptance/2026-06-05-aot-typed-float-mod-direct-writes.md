# AOT Typed Float Mod Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for typed float modulo onto direct C math:

- `MOD_FLOAT`

## RED

`test_aot_c_source_lowers_typed_float_mod_to_direct_c` was added to `tests/parser/test_aot_c_float_contracts.c`.

Initial failure:

```text
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_float_contracts.c:142:test_aot_c_source_lowers_typed_float_mod_to_direct_c:FAIL: Expected Non-NULL
```

The old generated-C path emitted `ZrLibrary_AotRuntime_ModFloat(state, &frame, ...)`.

## Implementation

`backend_aot_c_lowering_typed_float_mod.c` now emits `zr_aot_arith_exec_float_mod`, validates float operands, reads `nativeDouble`, checks `zr_aot_right_scalar == 0.0`, reports `modulo by zero`, and writes a `ZR_VALUE_TYPE_DOUBLE` result with `ZR_VALUE_FAST_SET(... fmod(...) ...)`.

Generated modules include `<math.h>`. The `MOD_FLOAT` function-body route calls `backend_aot_write_c_direct_mod_float`.

## Boundary

This covers only typed `MOD_FLOAT` in the C backend. Generic `MOD` float fallback semantics, power, and LLVM parity remain future contracts. Linux generated shared-library smoke links `-lm` for `fmod`.

## Validation

WSL GCC:

```text
zr_vm_aot_c_float_contracts_test: 1 test, 0 failures
zr_vm_aot_c_float_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

WSL Clang:

```text
zr_vm_aot_c_float_contracts_test: 1 test, 0 failures
zr_vm_aot_c_float_shared_library_smoke_test: 1 test, 0 failures
```

Windows MSVC:

```text
zr_vm_aot_c_float_contracts_test.exe: 1 test, 0 failures
```

`git diff --check` reported only existing LF-to-CRLF warnings and no whitespace errors. A source scan found no `ZrLibrary_AotRuntime_ModFloat(state, &frame` generated-C emission in AOT C backend sources.

## Files

- `tests/parser/test_aot_c_float_contracts.c`
- `tests/parser/test_aot_c_float_shared_library_smoke.c`
- `tests/CMakeLists.txt`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_float_mod.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
