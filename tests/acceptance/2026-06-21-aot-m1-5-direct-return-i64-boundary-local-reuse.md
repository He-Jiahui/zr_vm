# AOT M1.5 Direct Return I64 Boundary Local Reuse

## Scope
- Advanced AOT 07-S2 by tightening the already-local i64 direct-return boundary.
- Affected layers: generated C return lowering, focused typed scalar generated-product tests, and plan/status documentation.

## Baseline
- The previous i64 direct-return local path already wrote the caller result directly from `zr_aot_s23` / `zr_aot_s48` instead of materializing `frame.slotBase[23/48].value`.
- That boundary still reintroduced interpreter-frame lookups inside the return block by declaring `SZrCallInfo *zr_aot_call_info = frame.callInfo`, checking `state == ZR_NULL || frame.function == ZR_NULL`, and branching on `frame.function->functionName` to avoid constructor result writes.

## Test Inventory
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED assertion forbids `/* zr_aot_direct_return_i64_local */` immediately followed by `SZrCallInfo *zr_aot_call_info = frame.callInfo;`.
  - Negative assertions forbid the return-local `state == ZR_NULL || frame.function == ZR_NULL` guard and `frame.function->functionName` constructor branch.
  - Positive assertions still require direct caller result writes from `zr_aot_s23` and `zr_aot_s48`.
  - Runtime path still compiles generated C into a shared library and compares AOT execution against the interpreter result.
- `tests/parser/test_aot_c_source_contracts.c`
  - Regression coverage for existing AOT source contracts.
- `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing return lowering contracts.
- Generated C inspection:
  - Confirms the focused direct-return blocks no longer contain `frame.callInfo`, `frame.function` guard text, constructor-name checks, or `zr_aot_result_slot = frame.slotBase + N`.

## Tooling Evidence
- RED:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && timeout 90s ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: failed 1/1 because generated C still contained `/* zr_aot_direct_return_i64_local */\n        SZrCallInfo *zr_aot_call_info = frame.callInfo;`.
- GREEN focused test:
  - Same command after implementation.
  - Result: 1 test, 0 failures.
- WSL GCC source and return contracts:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar|aot_c_return_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. `ctest -N -R 'return|aot_c_return'` reports 0 registered return-contract CTests in this build.
- Diff hygiene:
  - `git diff --check -- tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/index.md docs/parser-and-semantics/csharp-value-type-semir-aot.md .codex/sessions/20260620-2321-aot-07-12-codegen.md`
  - Result: exit 0, with only existing LF/CRLF warnings from the dirty worktree.

## Results
- `backend_aot_write_c_direct_return_i64_local()` now reuses the function-scope `zr_aot_call_info` value already emitted by the current frame setup instead of redeclaring a return-local pointer from `frame.callInfo`.
- The direct-return local block no longer emits a `frame.function` null guard or constructor-name branch. This is guarded by `backend_aot_c_scalar_locals_can_direct_return_i64_local()`, which rejects constructors before routing to this optimized emitter.
- Generated typed scalar C still writes the caller result boundary value from scalar locals and preserves ownership release, closure close, constructor receiver copy-back, stack top restoration, and `ZR_AOT_C_RETURN(1)`.

## Modularization Note
- `backend_aot_c_lowering_control.c` is 964 lines after this slice, so it is near the project warning threshold but did not gain a new subsystem; this change only tightens an existing direct-return emitter.
- `tests/parser/test_aot_c_typed_scalar.c` is 1025 lines. The slice added three focused assertions to the existing single typed-scalar generated-product fixture. Splitting the test now would mix acceptance coverage with unrelated test reorganization.
- Smallest follow-up boundary: extract generated-C marker/forbidden-token assertions for the 07-S2 typed scalar fixture into a dedicated helper or a narrower scalar-local regression contract once the remaining 07-S2 assertions stabilize.

## Acceptance Decision
- Accepted as a 07-S2 sub-slice.
- Remaining 07-S2 work includes broader prologue/frame setup removal, full boundary marshaling templates, non-i64 direct-return forms, byte-frame narrowing, and any remaining frame-backed writes outside proven scalar-local hot paths.
