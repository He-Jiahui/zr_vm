# 2026-06-28 AOT 07-S2 signed binary scalar-operands consumer alignment

## Scope

- Slice: M1.5 / 07-S2 signed binary scalar-operands consumer alignment.
- Goal: make signed i64 binary arithmetic consume already-written scalar locals when both source operands are proven live, instead of reloading source `SZrTypeValue` data from `frame.slotBase`.
- Trigger: 11-S6H validation exposed a runtime failure in the static numeric no-arg typed direct-call shared-library smoke. The failure was outside the 11-S6H metadata guard/deopt surface, but it blocked the broader AOT 07-12 goal.

## RED

- Command: `./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test`.
- Result: 5 tests, 1 failure.
- Failing test: `test_aot_c_generated_shared_library_executes_static_numeric_call_local_sync_path`.
- Root cause: typed no-arg u64/f64 direct calls and conversions wrote only scalar locals `zr_aot_s7` and `zr_aot_s9`; the following `ADD_SIGNED` still used the stack-value signed arithmetic path and read `frame.slotBase[7]` / `frame.slotBase[9]`, which had not been materialized.

## Implementation

- Added a signed binary scalar-operands fast path guarded by `backend_aot_c_scalar_locals_i64_written_before()` for both operands at the current ExecIR instruction index.
- Threaded `execInstructionIndex` through the signed add/subtract/multiply/divide/modulo lowering entry points.
- Emitted `zr_aot_arith_exec_signed_scalar_operands` in generated C when both operands can be read from `zr_aot_sN`.
- Added generated-product smoke assertions requiring `zr_aot_s7` / `zr_aot_s9` source operands and forbidding source frame reloads for the static numeric no-arg typed direct-call case.
- Added source-contract needles covering the helper and marker.

## GREEN

- WSL gcc:
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_call_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
  - Focused CTest `aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed|aot_c_typed_scalar`: 3/3 passed.
- WSL clang:
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_call_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
  - Focused CTest `aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed|aot_c_typed_scalar`: 3/3 passed.
- Windows MSVC Debug:
  - Build target group passed for source contracts, shared-library smoke, and typed scalar.
  - `zr_vm_aot_c_source_contracts_test.exe`: 21 tests, 0 failures.
  - `zr_vm_aot_c_call_shared_library_smoke_test.exe`: 5 tests, 0 failures, 5 ignored Unix-only tests.
  - `zr_vm_aot_c_typed_scalar_test.exe`: 1 test, 0 failures, 1 ignored Unix-only test.
  - Focused CTest `aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed|aot_c_typed_scalar`: 3/3 passed.

## Files

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `tests/parser/test_aot_c_call_shared_library_smoke.c`
- `tests/parser/test_aot_c_source_contracts.c`
- `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
- `docs/plans/aot/index.md`
- `docs/plans/aot/11-metadata.md`

## Remaining Work

- This slice does not complete all of 07-S2.
- Destination frame writes, generic arithmetic fallback, return/result materialization, and full typed function body removal of `SZrValue` / frame writes remain open.
- This slice does not claim 11-S6 cross-module token resolution or complete ABI drift injection.
