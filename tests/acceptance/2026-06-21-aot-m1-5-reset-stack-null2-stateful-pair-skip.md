# AOT M1.5 RESET_STACK_NULL2 Stateful Pair Skip

## Scope
- Advanced AOT 07-S2 by proving paired `RESET_STACK_NULL2` scalar-local resets with per-slot live state instead of requiring each slot to prove dead independently.
- Affected layers: AOT C backend function-body routing, scalar-local reset liveness proof, and focused typed scalar generated-product tests.

## Baseline
- The previous `RESET_STACK_NULL2` skip path could remove `slots=5,6`, but straight-line pairs such as `slots=3,4` remained frame-backed because the proof checked each reset target independently.
- The reset proof also asked all scalar consumer predicates whether an instruction read a slot, even when that instruction was not a scalar-local consumer. That made `GET_CONSTANT` look like it read its destination slot before overwriting it.

## Test Inventory
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED assertion requires `/* zr_aot_reset_stack_null2_scalar_local_skip slots=3,4 */`.
  - Negative assertion forbids the old focused `slots=3,4` frame reset2 block.
  - Additional assertion requires `/* zr_aot_reset_stack_null2_scalar_local_skip slots=6,7 */` and forbids the old focused `slots=6,7` frame reset2 block.
  - Runtime path still compiles generated C into a shared library and compares AOT execution against the interpreter result.
- `tests/parser/test_aot_c_source_contracts.c`
  - Regression coverage for existing AOT source contracts.
- Generated C inspection:
  - Confirms all focused reset2 sites are marker-only skip comments.
  - Confirms `zr_aot_value_exec_reset_stack_null2` no longer appears in the focused typed scalar generated C.

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
  - `git diff --check -- tests/parser/test_aot_c_typed_scalar.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
  - Result: exit 0, with only existing LF/CRLF warnings from the dirty worktree.

## Results
- RED:
  - Focused typed scalar test failed on the missing `/* zr_aot_reset_stack_null2_scalar_local_skip slots=3,4 */` marker.
- GREEN:
  - `backend_aot_c_scalar_locals_reset2_can_skip_value_slots()` now tracks the two reset targets with a live bitmask through the current block suffix and successor graph.
  - `backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local()` first verifies the opcode is an actual scalar-local consumer, so `GET_CONSTANT` destination slots are treated as overwrites rather than reads.
  - `backend_aot_c_function_body.c` now routes `RESET_STACK_NULL2` through the paired reset proof instead of two independent single-slot proofs.
  - Generated focused C has marker-only skip comments for `slots=3,4`, `slots=5,6`, `slots=6,7`, `slots=9,10`, and `slots=15,16`, with no old `zr_aot_value_exec_reset_stack_null2` block.

## Modularization Note
- `backend_aot_c_scalar_locals.c` is 2278 lines after this slice. The change stayed in-file because the reset proof depends on existing private scalar-local consumer, overwrite, export, and block-boundary predicates; extracting only the new pair proof now would either duplicate that logic or prematurely widen private APIs.
- Smallest follow-up boundary: extract scalar-local liveness/result/reset proof helpers into a dedicated backend module once the remaining 07-S2 reset/result paths stabilize.

## Acceptance Decision
- Accepted as a 07-S2 sub-slice.
- Remaining 07-S2 work includes prologue/frame setup, boundary local restoration, generic float copy/type checks, complete typed boundary marshaling, and any remaining frame-backed writes outside proven scalar-local hot paths.
