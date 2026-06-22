# AOT M1.5 Frame Export Context Gating

## Scope
- Continued AOT 07-S3 by making module/export descriptor fields conditional in generated C frame setup.
- Affected layers: frame setup signature, frame setup emitter, function-body setup call site, frame setup source contract, and focused typed scalar generated-product contract.

## Baseline
- After `frame.recordHandle` removal, focused typed scalar generated C still assigned export-only frame fields:
  `frame.module`, `frame.moduleExecuted`, `frame.functionTable`, `frame.functionCount`,
  `frame.functionThunks`, and `frame.functionThunkCount`.
- Source inspection showed those fields are read by generated export publication paths, not by the focused non-export typed scalar hot path.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require the frame setup helper to accept `TZrBool includeExportContext`.
  - RED source-contract checks require export-context assignment strings to sit behind `if (includeExportContext)`.
  - RED function-body source-contract checks require the call site to pass `publishExports`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks forbid `frame.module`, `frame.moduleExecuted`, `frame.functionTable`,
    `frame.functionCount`, `frame.functionThunks`, and `frame.functionThunkCount`.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because the helper did not yet accept `TZrBool includeExportContext`. The generated-product assertion also targeted the old non-conditional export field setup.
- GREEN focused tests:
  - Same command after implementation.
  - Result: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `grep -nE 'frame\.(module|moduleExecuted|functionTable|functionCount|functionThunks|functionThunkCount|recordHandle)' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c`
  - Result: no matches.
  - Remaining focused generated `frame.*` setup fields are `frame.callInfo`, `frame.function`, `frame.generatedFrameSlotCount`, and `frame.slotBase`.
- Difference check:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && git diff --check -- tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c"`
  - Result: exited 0 with an existing CRLF/LF notice for `backend_aot_c_function_body.c`.

## Results
- `backend_aot_write_c_frame_setup()` now accepts `includeExportContext`.
- `backend_aot_write_c_function_body()` passes the existing `publishExports` predicate into frame setup.
- Generated export descriptor fields are emitted only when the function can publish module exports.
- Focused non-export typed scalar generated C no longer contains export-context frame fields.

## Modularization Note
- `backend_aot_c_frame_setup.c` remains small. `backend_aot_c_function_body.c` remains large; this slice touched only the existing frame-setup call site and did not add a new responsibility.
- `tests/parser/test_aot_c_typed_scalar.c` remains over the large-file threshold. This slice only adds focused forbidden-token assertions; the smallest follow-up split remains extracting generated-C marker/forbidden-token checks.

## Acceptance Decision
- Accepted as the fifth 07-S3 sub-slice.
- 07-S3 remains partial. Remaining focused prologue fields are `frame.callInfo`, `frame.function`, `frame.slotBase`, and `frame.generatedFrameSlotCount`; these still support return, cleanup, stack, and frame-slot fallback paths until later 07 slices replace those boundaries.
