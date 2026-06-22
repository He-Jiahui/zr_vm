# AOT M1.5 Fail Macro Function Index Local

## Scope
- Continued AOT 07-S3 by removing the failure macro's dependency on `frame.functionIndex` and `frame.currentInstructionIndex`.
- Affected layers: generated C guard macro, function body prologue, frame setup emitter, source contracts, frame setup contracts, and focused typed scalar generated-product test.

## Baseline
- The previous 07-S3 slice emitted `SZrAotMethodInfo`, but the generated `ZR_AOT_C_FAIL()` macro still read `frame.functionIndex` and `frame.currentInstructionIndex`.
- Frame setup still assigned `frame.functionIndex = zr_aot_context.resolvedFunctionIndex`, keeping one observation-era frame field alive only for diagnostics.

## Test Inventory
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks require `const TZrUInt32 zr_aot_function_index = 0u;`.
  - RED generated-product checks forbid `frame.functionIndex` and `frame.currentInstructionIndex`.
- `tests/parser/test_aot_c_source_contracts.c`
  - RED source-contract checks require `ZR_AOT_C_FAIL()` to report `(unsigned)zr_aot_function_index` and `UINT32_MAX`, not frame fields.
  - Function-body source contract requires the per-function local constant emitted from `entry->flatIndex`.
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks forbid setup assignment to `frame.functionIndex`.

## Tooling Evidence
- RED typed scalar generated product:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && timeout 90s ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: failed 1/1 because generated C lacked `const TZrUInt32 zr_aot_function_index = 0u;`.
- RED source contract:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test"`
  - Result: failed 19 tests / 1 failure because `ZR_AOT_C_FAIL()` still used frame-backed function/index fields.
- RED frame setup contract:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: failed 1/1 because frame setup still emitted `frame.functionIndex = zr_aot_context.resolvedFunctionIndex;`.
- GREEN focused tests:
  - Same commands after implementation.
  - Results: typed scalar 1 test, 0 failures; source contracts 19 tests, 0 failures; frame setup contracts 1 test, 0 failures.
- WSL GCC source and return contracts:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - Focused generated C contains the macro read `(unsigned)zr_aot_function_index` and the function local `const TZrUInt32 zr_aot_function_index = 0u;`.
  - Focused generated C no longer contains `frame.functionIndex`, `frame.currentInstructionIndex`, `frame.lastObservedInstructionIndex`, `frame.lastObservedLine`, `frame.observationMask`, `frame.publishAllInstructions`, or `state->debugHookSignal`.
- Difference check:
  - Scoped `git diff --check -- zr_vm_common/include/zr_vm_common/zr_aot_abi.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c tests/parser/test_aot_c_source_contracts.c tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/index.md docs/parser-and-semantics/csharp-value-type-semir-aot.md .codex/sessions/20260620-2321-aot-07-12-codegen.md tests/acceptance/2026-06-21-aot-m1-5-method-info-skeleton.md` exited 0 with only existing CRLF/LF warnings.

## Results
- `backend_aot_write_c_guard_macro()` now reports generated failures with `zr_aot_function_index` and a fixed `UINT32_MAX` instruction index.
- `backend_aot_write_c_function_body()` emits `const TZrUInt32 zr_aot_function_index = Nu;` per generated function.
- `backend_aot_write_c_frame_setup()` no longer writes `frame.functionIndex`.

## Modularization Note
- `backend_aot_c_emitter.c` is still small enough for this guard macro change to remain local.
- `tests/parser/test_aot_c_typed_scalar.c` is 1039 lines after this slice. This slice adds only three focused generated-product assertions; the smallest follow-up split remains extracting generated-C marker/forbidden-token checks.

## Acceptance Decision
- Accepted as the third 07-S3 sub-slice.
- 07-S3 remains partial. Remaining work includes removing more typed-scalar fat-frame fields, binding MethodInfo descriptors beyond the skeleton, and preserving required boundary/dynamic fallback state.
