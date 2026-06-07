# AOT Generic Power Meta-Boundary Direct Writes

Date: 2026-06-05

## Scope

This slice removes the generated AOT C helper call for generic `POW` while preserving its current VM meaning: generic `POW` is a meta operation, not primitive numeric exponentiation. Typed numeric power remains covered by `POW_SIGNED`, `POW_UNSIGNED`, and `POW_FLOAT`.

## RED

`test_aot_c_source_lowers_generic_power_meta_boundary_to_direct_c` was added to `tests/parser/test_aot_c_power_contracts.c`.

Initial failure:

```text
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_power_contracts.c:212:test_aot_c_source_lowers_generic_power_meta_boundary_to_direct_c:FAIL: Expected Non-NULL
```

The failure was caused by the missing `backend_aot_c_lowering_generic_power.c` module and the old `POW` function-body route that still emitted `ZrLibrary_AotRuntime_Pow(state, &frame, ...)`.

## Implementation

`backend_aot_c_lowering_generic_power.c` now emits a direct generated-C block for generic `POW`.

The generated C resolves destination/left/right stack slots, checks `ZrCore_Value_GetMeta(state, zr_aot_left, ZR_META_POW)`, and writes `null` directly with `ZrCore_Value_ResetAsNull(zr_aot_destination)` when no power meta function exists. If a real meta power function is present, generated C reports `unsupported AOT generic power meta dispatch` and exits through `ZR_AOT_C_FAIL()` instead of silently calling the runtime helper.

Generated modules now include `zr_vm_core/meta.h` so `SZrMeta` is visible to downstream C compilers.

## Boundary

This is a helper-free generated-C boundary for the no-meta generic `POW` contract. It deliberately does not implement generated meta-call execution. Source-level `**` parser tokenization and LLVM parity remain separate future contracts.

## Validation

WSL GCC:

```text
zr_vm_aot_c_power_contracts_test: 2 tests, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

WSL Clang:

```text
zr_vm_aot_c_power_contracts_test: 2 tests, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 6 tests, 0 failures
```

Windows MSVC:

```text
zr_vm_aot_c_power_contracts_test.exe: 2 tests, 0 failures
zr_vm_aot_c_power_shared_library_smoke_test.exe: 1 test, 0 failures, 1 ignored
```

`git diff --check` reported only existing LF-to-CRLF warnings and no whitespace errors. A source scan found no generated-C `ZrLibrary_AotRuntime_Pow(state, &frame`, `PowSigned(state, &frame`, `PowUnsigned(state, &frame`, or `PowFloat(state, &frame` emission in AOT C backend sources.

## Files

- `tests/parser/test_aot_c_power_contracts.c`
- `tests/parser/test_aot_c_power_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_power.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
