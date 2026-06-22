# AOT M1.5 RESET_STACK_NULL2 Scalar-Local Skip

## Scope
- Advanced AOT 07-S2 by removing focused two-slot `RESET_STACK_NULL2` frame writes when both reset targets are scalar locals and each slot is proven dead before any frame-dependent read.
- Affected layers: AOT C backend function-body routing, scalar-local reset liveness proof, generated C emitter, and parser/AOT generated-product tests.

## Baseline
- The focused typed scalar generated C still emitted `zr_aot_value_exec_reset_stack_null2` blocks with `SZrTypeValue *zr_aot_first`, `SZrTypeValue *zr_aot_second`, `frame.slotBase` bounds checks, and two `ZrCore_Value_ResetAsNull` calls.
- Single-slot `RESET_STACK_NULL` scalar-local skip was already available, but `RESET_STACK_NULL2` still used the frame-backed reset emitter.

## Test Inventory
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED assertion required `/* zr_aot_reset_stack_null2_scalar_local_skip slots=5,6 */`.
  - Negative assertion forbids the old focused `slots=5,6` frame reset2 block.
  - Runtime path still compiles generated C into a shared library and compares AOT execution against the interpreter result.
- `tests/parser/test_aot_c_source_contracts.c`
  - Regression coverage for existing AOT source contracts.
- Generated C inspection:
  - Confirms the two focused `slots=5,6` reset2 sites are marker-only skip comments.
  - Confirms remaining frame-backed reset2 output belongs to other slot pairs that are not proven safe by this focused assertion.

## Tooling Evidence
- WSL GCC/Ninja focused test:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && timeout 90s ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: 1 test, 0 failures.
- WSL GCC source contracts:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test"`
  - Result: 19 tests, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar' --output-on-failure"`
  - Result: 1/1 passed.
- Diff hygiene:
  - `git diff --check`
  - Result: exit 0, with only existing LF/CRLF warnings from the dirty worktree.

## Results
- RED:
  - Focused typed scalar test first failed on the missing `/* zr_aot_reset_stack_null2_scalar_local_skip slots=3,4 */` marker.
  - That target was intentionally revised for this slice because the then-current proof checked each reset target independently and did not yet carry paired live state. A follow-up stateful pair proof later accepted `slots=3,4`.
  - The valid RED target, `slots=5,6`, then covered a pair where both slots are overwritten before later reads.
- GREEN:
  - `backend_aot_c_function_body.c` now routes `RESET_STACK_NULL2` through two calls to `backend_aot_c_scalar_locals_reset_can_skip_value_slot()`.
  - `backend_aot_c_lowering_values.c` now emits `zr_aot_reset_stack_null2_scalar_local_skip` marker-only comments when both slots pass the proof.
  - Generated focused C has two `slots=5,6` skip markers and no old focused `frame.slotBase[5/6]` reset2 block.

## Acceptance Decision
- Accepted as a 07-S2 sub-slice.
- Remaining 07-S2 work includes other reset pairs not yet proven safe, prologue/frame setup, boundary local restoration, generic float copy/type checks, and complete typed boundary marshaling.
