# AOT CREATE_CLOSURE Materialization Boundary

Date: 2026-06-05

Scope: generated AOT C `CREATE_CLOSURE` lowering for captured closures and unresolved callable constants.

## Contract

- Resolved zero-capture `CREATE_CLOSURE` continues to use the direct generated native-closure path.
- Captured closures and unresolved callable constants no longer emit `ZrLibrary_AotRuntime_CreateClosure` from the generated-C function body.
- Generated C emits `zr_aot_value_unsupported_create_closure_materialization`, records `zr_aot_capture_count`, resolves the source constant and destination slot, checks callable metadata, reports `unsupported AOT CREATE_CLOSURE materialization`, and exits through `ZR_AOT_C_FAIL()`.
- Callable provenance remains valid only for direct zero-capture generated closures; captured and unresolved boundary paths clear it.
- LLVM still keeps its `ZrLibrary_AotRuntime_CreateClosure` route until LLVM parity and real captured-closure materialization are designed.

## RED

The focused contract was added before production code and failed as expected:

```text
Missing source contract text: backend_aot_write_c_unsupported_create_closure_materialization(
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_constant_contracts.c:87:test_aot_c_source_makes_create_closure_explicit_boundary:FAIL:missing required source contract text
2 Tests 1 Failures 0 Ignored
```

## Validation

- GCC `zr_vm_aot_c_constant_contracts_test`: 2 tests, 0 failures.
- GCC nearby regressions: `zr_vm_aot_c_source_contracts_test` 17 tests, `zr_vm_aot_c_global_contracts_test` 1 test, `zr_vm_aot_c_shared_library_smoke_test` 6 tests, all 0 failures.
- Clang same focused set: source contracts 17 tests, constant contracts 2 tests, global contract 1 test, generated shared-library smoke 6 tests, all 0 failures.
- MSVC focused set in `build/codex-msvc-aot-constant-debug`: source contracts 17 tests, constant contracts 2 tests, global contract 1 test, all 0 failures.
- A C-backend source scan found no `ZrLibrary_AotRuntime_CreateClosure` emission in `backend_aot_c_function_body.c` or `backend_aot_c_lowering_values.c`; remaining references are LLVM-only.

## Files

- `tests/parser/test_aot_c_constant_contracts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
