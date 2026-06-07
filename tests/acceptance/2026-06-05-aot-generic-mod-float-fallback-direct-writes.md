# AOT Generic MOD Float Fallback Direct Writes

Date: 2026-06-05

## Scope

This slice removes the remaining generated AOT C unsupported boundary for numeric generic `MOD` when either operand is a float. Generic `MOD` still accepts only numeric `SZrTypeValue` operands in this direct primitive subset; nonnumeric, string, object, dynamic, and meta cases remain explicit unsupported boundaries.

## RED

`test_aot_c_source_lowers_generic_numeric_mod_float_fallback_to_direct_c` was added in `tests/parser/test_aot_c_generic_numeric_contracts.c` instead of extending the already large source-contract file.

Initial failure:

```text
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_generic_numeric_contracts.c:87:test_aot_c_source_lowers_generic_numeric_mod_float_fallback_to_direct_c:FAIL:missing required source contract text
```

The failure was caused by `backend_aot_write_c_direct_mod()` passing `ZR_NULL` for its float expression and `ZR_FALSE` for `supportsFloat`.

## Implementation

`backend_aot_write_c_direct_mod()` now passes `fmod(zr_aot_left_float, zr_aot_right_float)` into the shared generic numeric binary writer and enables the float arm.

Generated C extracts signed, unsigned, or float numeric operands into `TZrFloat64` when either runtime operand tag is float, checks the existing `modulo by zero` guard against `zr_aot_right_float`, and writes the result as `ZR_VALUE_TYPE_DOUBLE`.

## Boundary

This covers numeric generic `MOD` float fallback in the C backend only. Generic `MOD` for bool-as-numeric helper compatibility, nonnumeric/meta dispatch, source quickening policy, and LLVM parity remain separate contracts.

## Validation

WSL GCC:

```text
zr_vm_aot_c_generic_numeric_contracts_test: 1 test, 0 failures
zr_vm_aot_c_generic_numeric_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_float_contracts_test: 1 test, 0 failures
zr_vm_aot_c_float_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_power_contracts_test: 2 tests, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

WSL Clang:

```text
zr_vm_aot_c_generic_numeric_contracts_test: 1 test, 0 failures
zr_vm_aot_c_generic_numeric_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_float_contracts_test: 1 test, 0 failures
zr_vm_aot_c_float_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_power_contracts_test: 2 tests, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

Windows MSVC:

```text
zr_vm_aot_c_generic_numeric_contracts_test.exe: 1 test, 0 failures
zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe: 1 test, 0 failures, 1 ignored
```

A source scan found no generated-C `ZrLibrary_AotRuntime_Mod(state, &frame` emission in AOT C backend sources and confirmed the generic `MOD` route calls `backend_aot_write_c_direct_mod(...)`.

## Files

- `tests/parser/test_aot_c_generic_numeric_contracts.c`
- `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`
- `tests/CMakeLists.txt`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
