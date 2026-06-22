# AOT M1.5 RESET_STACK_NULL Scalar-Local Skip

## Scope
- Advanced AOT 07-S2 by removing a focused single-slot `RESET_STACK_NULL` frame write when the slot is covered by a scalar C local and all reachable future paths overwrite or kill the slot before any frame-dependent read.
- Affected layers: AOT C backend function-body routing, scalar-local liveness proof, generated C emitter, and parser/AOT generated-product tests.

## Baseline
- The focused typed scalar generated C still emitted single-slot reset blocks such as `zr_aot_value_exec_reset_stack_null` for `slot 21`, including `SZrTypeValue *zr_aot_destination`, `frame.slotBase[21].value`, and `ZrCore_Value_ResetAsNull`.
- 07-S2 was already partially complete for scalar source/result local-only paths and i64 direct returns, but reset-stack-null frame writes remained a hot typed-function-body frame dependency.

## Test Inventory
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED assertion required `/* zr_aot_reset_stack_null_scalar_local_skip slot=21 */`.
  - Negative assertion forbids the old single-slot `slot 21` frame reset block.
  - Runtime path still compiles generated C into a shared library and compares AOT execution against the interpreter result.
- `tests/parser/test_aot_c_source_contracts.c`
  - Regression coverage for existing AOT source contracts.
- Generated C inspection:
  - Confirms two focused `slot 21` resets are marker-only skip comments.
  - Confirms no generated `frame.slotBase[21].value` single-slot reset remains.

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
  - Focused typed scalar test failed on the missing `/* zr_aot_reset_stack_null_scalar_local_skip slot=21 */` marker.
- GREEN:
  - `backend_aot_c_function_body.c` now routes single-slot `RESET_STACK_NULL` through `backend_aot_c_scalar_locals_reset_can_skip_value_slot()`.
  - `backend_aot_c_scalar_locals.c` now scans the current block suffix and reachable successor blocks to reject future slot reads and accept overwrite/kill paths.
  - `backend_aot_c_lowering_values.c` now emits the marker-only skip comment for proven scalar-local resets.
  - Generated focused C has two `slot 21` skip markers at the branch merge shape and no old `frame.slotBase[21].value` single-slot reset block.
- Non-blocking validation note:
  - One combined focused command timed out before returning output; rerunning the build and executable with an explicit binary timeout produced the passing result above.

## Acceptance Decision
- Accepted as a 07-S2 sub-slice.
- Remaining 07-S2 work includes `RESET_STACK_NULL2`, other reset shapes, prologue/frame setup, boundary local restoration, generic float copy/type checks, and complete typed boundary marshaling.
