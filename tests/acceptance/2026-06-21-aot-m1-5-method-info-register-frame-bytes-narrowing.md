# AOT M1.5 MethodInfo Register Frame Bytes Narrowing

## Scope
- Continued AOT 07-S3 by narrowing generated `SZrAotMethodInfo.registerFrameBytes` to the byte range required by inline-struct frame slots.
- Affected layers: C backend method-info emitter, frame setup source contract, and focused typed scalar generated-product contract.

## Baseline
- The MethodInfo skeleton slice emitted one static method-info descriptor per generated function, but copied the full generated frame layout byte size into `.registerFrameBytes`.
- The focused pure scalar generated C therefore recorded `.registerFrameBytes = 6272u,` even though scalar locals are C locals and do not need a MethodInfo byte frame.
- Frame setup and legacy boundary code still allocate/setup the current frame layout; this slice only narrows the readonly MethodInfo descriptor.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require the MethodInfo emitter to compute register-frame bytes through `backend_aot_c_method_info_register_frame_bytes()`.
  - RED source-contract checks require that helper to filter for `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks require the focused pure scalar method info to contain `.registerFrameBytes = 0u,`.
  - RED generated-product checks forbid the old `.registerFrameBytes = 6272u,`.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because the MethodInfo register-frame-byte helper was missing; typed scalar failed 1/1 because generated C still lacked `.registerFrameBytes = 0u,`.
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
  - `grep -n 'registerFrameBytes' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -5`
  - Result: `915:    .registerFrameBytes = 0u,`.
  - Remaining focused generated `frame.*` setup fields are still `frame.callInfo`, `frame.function`, `frame.generatedFrameSlotCount`, and `frame.slotBase`.
- Difference check:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && git diff --check -- tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c"`
  - Result: exited 0 with an existing CRLF/LF notice for `backend_aot_c_emitter.c`.

## Results
- `backend_aot_c_emitter.c` now computes MethodInfo `.registerFrameBytes` from inline-struct frame slots only.
- Pure scalar slots no longer contribute to the MethodInfo byte frame requirement.
- Focused pure scalar generated C now emits `.registerFrameBytes = 0u,`.
- The generated prologue still contains the current frame setup and still records the old `frameByteSize=6272` observation comment; later 07-S3/07-S4 slices own prologue collapse.

## Modularization Note
- `backend_aot_c_emitter.c` is 348 lines after this slice and remains below the large-file threshold.
- `tests/parser/test_aot_c_frame_setup_contracts.c` is 269 lines and remains focused.
- `tests/parser/test_aot_c_typed_scalar.c` is 1049 lines. This slice only adds two focused MethodInfo generated-product assertions; the smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once the 07 checks stabilize.

## Acceptance Decision
- Accepted as the sixth 07-S3 sub-slice.
- 07-S3 remains partial. MethodInfo now reports scalar-only byte-frame size as zero, but generated frame setup still has `frame.callInfo`, `frame.function`, `frame.slotBase`, and `frame.generatedFrameSlotCount` for unresolved return, cleanup, stack, and frame-slot fallback boundaries.
