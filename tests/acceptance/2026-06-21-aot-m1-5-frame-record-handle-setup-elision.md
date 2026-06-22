# AOT M1.5 Frame Record Handle Setup Elision

## Scope
- Continued AOT 07-S3 by removing `frame.recordHandle` from generated C frame setup.
- Affected layers: frame setup emitter, frame setup source contract, and focused typed scalar generated-product contract.

## Baseline
- After the fail-macro local-index slice, focused typed scalar generated C still assigned `frame.recordHandle = zr_aot_context.recordHandle;`.
- The active C backend no longer emits C runtime helper calls that need `frame.recordHandle`; remaining direct generated paths use explicit frame fields such as `frame.function`, `frame.callInfo`, `frame.slotBase`, module state, function tables, and thunk tables.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract check forbids `frame.recordHandle = zr_aot_context.recordHandle;`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product check forbids `frame.recordHandle`.

## Tooling Evidence
- RED frame setup contract:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: failed 1/1 because frame setup source still emitted `frame.recordHandle = zr_aot_context.recordHandle;`.
- RED typed scalar generated product:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: failed 1/1 because generated C still contained `frame.recordHandle`.
- GREEN focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `grep -n 'frame.recordHandle' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c`
  - Result: no matches.
  - Remaining focused generated `frame.*` setup fields are `frame.callInfo`, `frame.function`, `frame.functionCount`, `frame.functionTable`, `frame.functionThunkCount`, `frame.functionThunks`, `frame.generatedFrameSlotCount`, `frame.module`, `frame.moduleExecuted`, and `frame.slotBase`.
- Difference check:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && git diff --check -- tests/parser/test_aot_c_frame_setup_contracts.c tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c"`
  - Result: exited 0.

## Results
- `backend_aot_write_c_frame_setup()` no longer emits `frame.recordHandle = zr_aot_context.recordHandle;`.
- Focused typed scalar generated C no longer contains `frame.recordHandle`.

## Modularization Note
- `backend_aot_c_frame_setup.c` remains small; this slice only removes one setup assignment.
- `tests/parser/test_aot_c_typed_scalar.c` is still over the large-file threshold, but this slice only adds one focused forbidden-token assertion. The smallest follow-up split remains extracting generated-C marker/forbidden-token checks after 07-S3 stabilizes.

## Acceptance Decision
- Accepted as the fourth 07-S3 sub-slice.
- 07-S3 remains partial. Remaining work includes replacing more fat-frame descriptor state, deciding which dynamic/boundary fields still need the legacy frame, and shrinking the typed scalar prologue toward the MethodInfo-only shape.
