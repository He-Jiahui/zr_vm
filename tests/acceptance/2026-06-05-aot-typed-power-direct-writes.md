# AOT Typed Power Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for typed power onto direct C expressions:

- `POW_SIGNED`
- `POW_UNSIGNED`
- `POW_FLOAT`

## RED

`test_aot_c_source_lowers_typed_power_to_direct_c` was added to `tests/parser/test_aot_c_power_contracts.c`.

Initial failure:

```text
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_power_contracts.c:184:test_aot_c_source_lowers_typed_power_to_direct_c:FAIL: Expected Non-NULL
```

The failure was caused by the missing `backend_aot_c_lowering_typed_power.c` module. The old typed generated-C route still emitted `ZrLibrary_AotRuntime_PowSigned`, `ZrLibrary_AotRuntime_PowUnsigned`, and `ZrLibrary_AotRuntime_PowFloat`.

An initial source-level generated-library smoke using `**` also failed with the parser diagnostic `Missing expression after '*'`. The backend smoke now hand-builds typed power opcodes directly; source parser support for `**` remains outside this AOT lowering slice.

## Implementation

`backend_aot_c_lowering_typed_power.c` emits direct generated C for the typed power family.

Signed and unsigned integer power validate their typed operands, guard unsupported power domains with `power domain error`, run exponentiation by squaring in generated C, guard overflow to the existing zero-result behavior, and write `ZR_VALUE_TYPE_INT64` or `ZR_VALUE_TYPE_UINT64` destinations with `ZR_VALUE_FAST_SET`.

Float power validates float operands, emits `pow(zr_aot_left_scalar, zr_aot_right_scalar)`, and writes `ZR_VALUE_TYPE_DOUBLE`. The function-body dispatcher routes `POW_SIGNED`, `POW_UNSIGNED`, and `POW_FLOAT` through these direct emitters.

## Boundary

This covers only typed power opcodes in the C backend. Generic `POW`, source-level `**` parser tokenization, and LLVM parity remain future contracts.

## Validation

WSL GCC:

```text
zr_vm_aot_c_power_contracts_test: 1 test, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

WSL Clang:

```text
zr_vm_aot_c_power_contracts_test: 1 test, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

Windows MSVC:

```text
zr_vm_aot_c_power_contracts_test.exe: 1 test, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test.exe: 1 test, 0 failures, 1 ignored
```

`git diff --check` reported only existing LF-to-CRLF warnings and no whitespace errors. A source scan found no generated-C `ZrLibrary_AotRuntime_PowSigned(state, &frame`, `PowUnsigned(state, &frame`, or `PowFloat(state, &frame` emission in AOT C backend sources.

## Files

- `tests/parser/test_aot_c_power_contracts.c`
- `tests/parser/test_aot_c_power_shared_library_smoke.c`
- `tests/CMakeLists.txt`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_power.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
