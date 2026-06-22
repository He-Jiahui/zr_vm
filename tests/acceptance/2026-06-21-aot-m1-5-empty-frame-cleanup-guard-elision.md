# AOT M1.5 Empty Frame Cleanup Guard Elision

## Scope
- Continued AOT 07-S3 by omitting generated frame cleanup guards when no inline-struct frame cleanup can be emitted.
- Affected layers: frame cleanup emitter, function-body emitter, frame setup source contract, and focused typed scalar generated-product contract.

## Baseline
- Focused pure scalar generated C already had `SZrAotMethodInfo.registerFrameBytes = 0u`, but still declared `TZrBool zr_aot_frame_started`, set it to true after frame setup, and emitted an empty `if (zr_aot_frame_started) { }` exit guard.
- `backend_aot_write_c_frame_cleanup()` only emits cleanup code for inline-struct frame slots with valid type layout metadata and nonzero byte size, so pure scalar functions had no cleanup body to protect.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require function body emission to compute `TZrBool needsFrameCleanup`.
  - RED source-contract checks require the decision to call `backend_aot_c_frame_cleanup_would_emit()`.
  - RED source-contract checks require cleanup emission to be gated by `if (needsFrameCleanup)`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks forbid `TZrBool zr_aot_frame_started = ZR_FALSE;`.
  - RED generated-product checks forbid `zr_aot_frame_started = ZR_TRUE;`.
  - RED generated-product checks forbid the empty exit guard `if (zr_aot_frame_started) {`.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because `TZrBool needsFrameCleanup` was missing. Typed scalar generated-product checks also targeted the old frame-started guard strings.
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
  - `grep -n 'zr_aot_frame_started\|if (zr_aot_frame_started)' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true`
  - Result: no matches.
  - Remaining focused generated `frame.*` setup fields are still `frame.callInfo`, `frame.function`, `frame.generatedFrameSlotCount`, and `frame.slotBase`.
- Difference check:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && git diff --check -- tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c"`
  - Result: exited 0 with an existing CRLF/LF notice for `backend_aot_c_function_body.c`.

## Results
- `backend_aot_c_frame_cleanup_would_emit()` now exposes the frame cleanup emitter's inline-struct/drop predicate to function-body emission.
- Function bodies only emit `zr_aot_frame_started` state and its exit guard when cleanup code can actually be generated.
- Focused pure scalar generated C no longer contains the empty frame-started cleanup guard.

## Modularization Note
- `backend_aot_c_frame_cleanup.c` is 65 lines and remains cohesive.
- `backend_aot_c_function_body.c` is 1891 lines. This slice touched the existing setup/exit orchestration only; the smallest follow-up split remains extracting function prologue/epilogue orchestration from the large function-body emitter.
- `tests/parser/test_aot_c_typed_scalar.c` is 1052 lines. This slice only adds three focused generated-product negative assertions; the smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once the 07 checks stabilize.

## Acceptance Decision
- Accepted as the seventh 07-S3 sub-slice.
- 07-S3 remains partial. The cleanup guard is gone for pure scalar generated C, but setup still declares `ZrAotGeneratedFrame frame` and assigns `frame.callInfo`, `frame.function`, `frame.slotBase`, and `frame.generatedFrameSlotCount` for unresolved return, stack, and frame-slot fallback boundaries.
