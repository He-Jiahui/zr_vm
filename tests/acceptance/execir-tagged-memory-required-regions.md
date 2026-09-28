# ExecIR Tagged Memory Required Regions

## Scope

- Require fully tagged memory input and output ranges to cover every memory region declared by the opcode schema for that direction.
- Preserve the untagged compatibility path and existing per-region version ordering.
- Affected layers: core ExecIR effect verification and parser verifier/builder tests.

## Baseline

- Current-source RED: the valid two-region CALL passed, but after setting `memoryIn.count` to 1 the verifier accepted the missing native-FFI region. The test exited with `FAIL: tagged call missing a declared memory input region accepted`.
- The first test build exposed a fixture mistake: DROP's `MAY_DROP` property is schema metadata, not a dynamic instruction flag. Removing the nonexistent flag fixed the test compile before the semantic RED run.
- The adjacent `ssa_construction` CTest includes two separately prepared dead-predecessor RED cases. They remain outside this fix.

## Test Inventory

- `tests/parser/test_ssa_effects_verifier.c`: accepts a complete tagged CALL with managed-heap and native-FFI input/output tokens; independently omits one input and one output region; checks `MEMORY_TOKEN` plus function, block, instruction, and source identity. Existing untagged-token and region-local ordering cases remain in the same suite.
- Adjacent CTest coverage: `ssa_core_model`, `ssa_construction`, `ssa_builder_cfg`, `ssa_builder_dominance`, `ssa_builder_control_edges`, `ssa_builder_cleanup_dispatch`, `ssa_builder_fact_identity`, and `ssa_builder_iterator_invokes`.
- Boundary coverage includes independent read/write masks, a multi-region opcode, precise source diagnostics, and the legacy untagged path.

## Tooling Evidence

- WSL GCC 11.4.0, CMake 3.22.1, Ninja 1.10.1, and CTest 3.22.1.
- Build directory: `/mnt/d/tmp/zr_vm/close-proxy-core-red`. Generated build outputs remained on D:.
- Focused RED build: `cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_ssa_effects_verifier_test -j 8` (exit 0 after correcting the test fixture).
- Focused RED run: `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_effects_verifier_test` (exit 1 with the expected missing-input-region acceptance failure).
- Focused GREEN build: the same target command after the verifier change (exit 0).
- Focused GREEN run: the same executable (exit 0, `ssa effects verifier PASS`).
- Adjacent build: `cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_ssa_core_model_test zr_vm_ssa_construction_test zr_vm_ssa_builder_cfg_test zr_vm_ssa_builder_dominance_test zr_vm_ssa_builder_control_edges_test zr_vm_ssa_builder_cleanup_dispatch_test zr_vm_ssa_builder_fact_identity_test zr_vm_ssa_builder_iterator_invokes_test -j 8` (exit 0; unchanged `exec_ir_build.c` emitted missing-braces warnings).
- Adjacent CTest: `ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R '^(ssa_effects_verifier|ssa_core_model|ssa_construction|ssa_builder_cfg|ssa_builder_dominance|ssa_builder_control_edges|ssa_builder_cleanup_dispatch|ssa_builder_fact_identity|ssa_builder_iterator_invokes)$' --output-on-failure --no-tests=error`.

## Results

- The focused verifier CTest and direct Unity executable pass after the fix.
- Eight of nine selected adjacent CTests pass: effects verifier, core model, and all six builder suites pass.
- `ssa_construction` reports 9/11 Unity tests passing. Its failures are `test_ssa_construction_ignores_dead_predecessor_during_promotion` and `test_ssa_construction_keeps_mixed_dead_join_in_memory_form`, the separately prepared dead-predecessor RED cases in the active SSA plan. This change did not modify their source or behavior.
- A test-tree search found no remaining single-region tagged CALL positive fixture; the existing tagged CALL fixture uses both required regions.

## Acceptance Decision

- Accepted for the tagged required-region verifier fix: both directional omission cases now fail with the expected diagnostic identity, while the focused suite and eight adjacent verifier/builder CTests pass.
- The two prepared `ssa_construction` dead-predecessor RED cases remain outside this commit.
