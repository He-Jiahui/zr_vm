# AOT M1.5 Skip-Drop Slot Cleanup Gating

## Scope
- Continued AOT 07-S3 by omitting the generated `zr_aot_skip_drop_slot` local when no inline-struct frame cleanup can run.
- Affected layers: function-body emitter, frame setup source contract, and focused typed scalar generated-product contract.

## Baseline
- Focused pure scalar generated C had already stopped emitting `zr_aot_frame_started` and the empty cleanup guard.
- It still declared `TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;` even though the variable is only consumed by inline-struct cleanup and inline return/drop coordination.
- `backend_aot_c_frame_cleanup_would_emit()` already exposes the same inline-struct/drop predicate that controls cleanup emission, so the skip-drop local can use that predicate.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require function-body emission to compute `TZrBool needsSkipDropSlot`.
  - RED source-contract checks require `needsSkipDropSlot = needsFrameCleanup;`.
  - RED source-contract checks require skip-drop local emission to be gated by `if (needsSkipDropSlot) {`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product check forbids `TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;`.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because `TZrBool needsSkipDropSlot` was missing. The typed scalar generated-product assertion was added in the same red step against the old unconditional skip-drop declaration; the fail-fast `&&` command stopped at the first failing binary.
- GREEN focused tests:
  - Same command after implementation.
  - Result: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -n 'zr_aot_skip_drop_slot\|zr_aot_frame_started\|frame\.' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -40"`
  - Result: no `zr_aot_skip_drop_slot` or `zr_aot_frame_started` matches; remaining focused generated `frame.*` setup fields are still `frame.callInfo`, `frame.function`, `frame.generatedFrameSlotCount`, and `frame.slotBase`.
- Difference check:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && git diff --check -- tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c"`
  - Result: exited 0 with an existing CRLF/LF notice for `backend_aot_c_function_body.c`.

## Results
- `backend_aot_write_c_function_body()` now derives `needsSkipDropSlot` from `needsFrameCleanup`.
- Pure scalar generated C no longer declares an unused skip-drop local.
- Inline-struct cleanup paths keep the skip-drop local because cleanup can still consult it while dropping inline frame slots.

## Modularization Note
- `backend_aot_c_function_body.c` is 1892 lines. This slice only touched the existing setup-local emission cluster; the smallest follow-up split remains extracting generated-function prologue/epilogue orchestration from the large function-body emitter.
- `tests/parser/test_aot_c_typed_scalar.c` is 1053 lines. This slice adds one focused generated-product negative assertion; the smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once the 07 checks stabilize.

## Acceptance Decision
- Accepted as the eighth 07-S3 sub-slice.
- 07-S3 remains partial. The skip-drop local is gone for pure scalar generated C, but setup still declares `ZrAotGeneratedFrame frame` and assigns `frame.callInfo`, `frame.function`, `frame.slotBase`, and `frame.generatedFrameSlotCount` for unresolved return, stack, cleanup, and frame-slot fallback boundaries.
