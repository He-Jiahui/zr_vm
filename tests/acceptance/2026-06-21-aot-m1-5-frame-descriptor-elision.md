# AOT M1.5 Frame Descriptor Elision

## Scope
- Continued AOT 07-S4 by omitting `ZrAotGeneratedFrame frame` and the remaining frame descriptor setup assignments for a proven pure scalar generated C function.
- Affected layers: frame descriptor proof, function body prologue emission, frame setup emission, scalar SemIR frame-free probing, scalar stack-copy local-only predicate, source contracts, and focused typed scalar generated-product contract.

## Baseline
- Focused pure scalar generated C already omitted frame-start cleanup state, skip-drop state, byte-frame zero prologue locals, and export-context fields.
- The same generated function still declared `ZrAotGeneratedFrame frame = {0};` and still assigned `frame.function`, `frame.callInfo`, `frame.slotBase`, and `frame.generatedFrameSlotCount` during setup.
- Many non-scalar and unresolved fallback lowerers still read `frame.*`, so descriptor removal needed a conservative body-level proof instead of a global deletion.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require `backend_aot_write_c_frame_setup()` to accept `includeFrameDescriptor`.
  - RED source-contract checks require function body routing through `backend_aot_c_function_body_needs_frame_descriptor()`.
  - RED source-contract checks require the function body to pass `includeFrameDescriptor` to frame setup.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks forbid `ZrAotGeneratedFrame frame = {0};`.
  - RED generated-product checks forbid the focused generated setup assignments to `frame.function`, `frame.callInfo`, `frame.slotBase`, and `frame.generatedFrameSlotCount`.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts while the descriptor proof is introduced.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because `TZrBool includeFrameDescriptor` was missing from the source.
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
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'ZrAotGeneratedFrame frame|frame\\.(function|callInfo|slotBase|generatedFrameSlotCount)|registerFrameBytes|value SemIR lowering frameByteSize' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true"`
  - Result: only `.registerFrameBytes = 0u,` and `value SemIR lowering frameByteSize=0` matched; no frame declaration or descriptor assignment matched.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -o 'frame\\.[A-Za-z0-9_]*' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | sort | uniq -c || true"`
  - Result: no `frame.*` matches in the focused pure scalar generated C.
- Scoped whitespace check:
  - `git diff --check -- tests/parser/test_aot_c_typed_scalar.c tests/parser/test_aot_c_frame_setup_contracts.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_descriptor.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_descriptor.h`
  - Result: exit 0; Git reported existing LF-to-CRLF working-tree notices for touched tracked files.

## Results
- `backend_aot_c_frame_descriptor.*` now owns the conservative frame-free proof for generated C function bodies.
- `backend_aot_write_c_function_body()` emits `ZrAotGeneratedFrame frame = {0};` only when that proof says a frame descriptor is still needed.
- `backend_aot_write_c_frame_setup()` now accepts `includeFrameDescriptor` and emits the four descriptor setup assignments only when true.
- `backend_aot_c_scalar_semir_can_write_frame_free_for_exec_instruction()` probes scalar SemIR emission and rejects candidates that still emit `frame.`.
- `backend_aot_c_scalar_stack_copy_can_use_local_only()` exposes the stricter stack-copy predicate needed by the descriptor proof: both source and destination must have compatible scalar locals.
- Focused pure scalar generated C now has no `ZrAotGeneratedFrame frame` declaration and no `frame.*` references.

## Modularization Note
- `backend_aot_c_frame_descriptor.c` is 210 lines and keeps the new proof out of the already large `backend_aot_c_function_body.c`.
- `backend_aot_c_function_body.c` remains over the large-file threshold. This slice only adds the predicate call and conditional frame declaration there; the proof itself was extracted.
- `tests/parser/test_aot_c_typed_scalar.c` is over 1000 lines. This slice only adds focused generated-product negative assertions; the smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once the 07 checks stabilize.

## Acceptance Decision
- Accepted as a 07-S4 sub-slice.
- 07-S4 remains partial. The focused pure scalar descriptor is eliminated, but generated functions with exports, cleanup, exceptions, inline value frames, unresolved opcodes, or frame-backed fallback lowerers still keep the descriptor path.
