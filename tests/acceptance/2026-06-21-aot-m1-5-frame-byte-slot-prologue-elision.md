# AOT M1.5 Frame Byte-Slot Prologue Elision

## Scope
- Continued AOT 07-S4 by narrowing generated frame setup byte-frame requirements to inline-struct slots only.
- Affected layers: frame setup emitter, value SemIR summary emission, source contracts, and focused typed scalar generated-product contract.

## Baseline
- MethodInfo `.registerFrameBytes` had already been narrowed to zero for the focused pure scalar generated function.
- The generated prologue still used the raw frame layout byte size for setup, so pure scalar generated C could emit `zr_aot_frame_byte_size = (TZrSize)0u;` and a byte-slot count local even when no byte-backed inline struct slots existed.
- The value SemIR generated comment still reported the raw frame layout byte size, so the focused pure scalar product could still show `value SemIR lowering frameByteSize=6272`.
- The frame setup source contract already expected an inline-struct-only byte-frame helper, but the implementation had drifted and still used the raw frame layout byte size.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require `backend_aot_c_frame_setup_register_frame_bytes()`.
  - RED source-contract checks require that helper to filter `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT`.
  - RED source-contract checks require byte-slot count emission to be gated by `if (frameByteSize > 0u) {`.
- `tests/parser/test_aot_c_source_contracts.c`
  - RED source-contract checks require `backend_aot_c_value_semir_register_frame_bytes()`.
  - RED source-contract checks require the value SemIR summary to use `valueFrameBytes` instead of `frameLayout->frameByteSize`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks forbid `TZrSize zr_aot_frame_byte_size;`.
  - RED generated-product checks forbid `TZrSize zr_aot_frame_byte_slot_count = 0;`.
  - RED generated-product checks forbid `zr_aot_frame_byte_size = (TZrSize)0u;`.
  - RED generated-product checks require `value SemIR lowering frameByteSize=0`.
  - RED generated-product checks forbid `value SemIR lowering frameByteSize=6272`.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because the conditional byte-slot prologue gate was missing. The first contract run also exposed implementation drift where the frame setup helper named by the contract did not exist.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts failed 19 tests, 1 failure because `backend_aot_c_value_semir_register_frame_bytes(` was missing after the value SemIR summary contract was tightened.
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
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'zr_aot_frame_byte_size|zr_aot_frame_byte_slot_count' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true"`
  - Result: no matches in the focused pure scalar generated C.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -n 'frameByteSize\|registerFrameBytes\|zr_aot_frame_byte_size\|zr_aot_frame_byte_slot_count' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -80 || true"`
  - Result: `.registerFrameBytes = 0u,` and `value SemIR lowering frameByteSize=0`; no byte-frame setup locals.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -o 'frame\.[A-Za-z0-9_]*' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | sort | uniq -c"`
  - Result: remaining focused generated `frame.*` setup fields are still `frame.callInfo`, `frame.function`, `frame.generatedFrameSlotCount`, and `frame.slotBase`.

## Results
- `backend_aot_write_c_frame_setup()` now derives setup byte-frame requirements with `backend_aot_c_frame_setup_register_frame_bytes()`.
- The helper counts only inline-struct frame slots that have an effective type-layout id and positive byte size.
- Pure scalar generated C no longer emits the byte-frame size local, byte-slot count local, or zero byte-frame assignment.
- Value SemIR generated comments now report the same inline-struct-only byte-frame size, so focused pure scalar output records `frameByteSize=0` instead of the raw layout size.
- Inline-struct frame functions keep the byte-slot prologue path when the computed byte-frame requirement is nonzero.

## Modularization Note
- `backend_aot_c_frame_setup.c` remains below the large-file threshold after this slice.
- `backend_aot_c_value_semir.c` remains below the large-file threshold after this slice.
- `tests/parser/test_aot_c_typed_scalar.c` is over 1000 lines. This slice only adds focused generated-product negative assertions; the smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once the 07 checks stabilize.

## Acceptance Decision
- Accepted as a 07-S4 sub-slice.
- 07-S4 remains partial. The byte-frame zero case is no longer emitted for pure scalar generated C, but setup still declares `ZrAotGeneratedFrame frame` and assigns `frame.callInfo`, `frame.function`, `frame.slotBase`, and `frame.generatedFrameSlotCount` for unresolved return, stack, cleanup, and frame-slot fallback boundaries.
