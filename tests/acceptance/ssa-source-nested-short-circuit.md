# Nested Source Short-Circuit CFG

## Scope

- 01.02 source-owned CFG preflight now accepts `&&`/`||` trees whose leaves are existing linear expressions. It still rejects unsupported leaves.
- Affects parser source CFG production and ExecIR builder inputs; no ExecIR opcode, verifier rule, or fallback path changes.

## Baseline

- MSVC pre-change: the new nested while test failed at `preSemanticIrCfgActive` (`Expected TRUE Was FALSE`); the five previous while tests passed. The existing preflight accepted only a single logical node with linear operands.
- The existing `for` short-circuit producer shares the same preflight. Unrelated repository dirty changes were not included.

## Test Inventory

- `test_nested_while_conditions_keep_both_rhs_branches`: inner and outer branch edges, loop backedge, strict ExecIR build.
- `test_nested_rhs_runs_only_after_its_own_left_branch`: nested-left, nested-right and mixed inner-OR/outer-AND forms, skipped and executed assignment store counts under the direct Oracle.
- `test_nested_for_condition_keeps_source_cfg`: nested-right condition and step-to-header backedge, strict build.
- `test_nested_unmodeled_rhs_remains_analysis_only`: unsupported comparison leaf refuses source CFG and strict builder returns `UNSUPPORTED`.
- Existing single-level `&&`/`||`, unsupported source and slot-reuse cases are retained. No new ownership, suspension, or exception behavior is claimed.

## Tooling Evidence

- MSVC 19.44.35228.0, `build/codex-ssa-conversion-msvc-static`: build each focused test with `Invoke-VsDevCommand.ps1 cmake --build ... --target zr_vm_ssa_source_while_short_circuit_test` and `... --target zr_vm_ssa_source_for_short_circuit_test --parallel 4`; execute each binary directly. Initial RED 6 tests/1 failure, after the recursive preflight 8/8 while and 8/8 for pass.
- GCC 11.4.0, `/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu`: configure with Ninja, Debug, `BUILD_TESTS=ON`, `BUILD_CLI=OFF` and `BUILD_LANGUAGE_SERVER_EXTENSION=OFF`; build four focused targets with `cmake --build ... --target zr_vm_ssa_source_while_short_circuit_test zr_vm_ssa_source_for_short_circuit_test zr_vm_ssa_construction_test zr_vm_pre_semantic_ir_test -j 8` (861/861); `ctest --test-dir ... -R '^(ssa_source_(while|for)_short_circuit|ssa_construction)$' --output-on-failure --no-tests=error` passed 3/3; direct `bin/zr_vm_pre_semantic_ir_test` passed 101/101.
- Clang 14.0.0, `/home/hejiahui/zrvm-ssa-nested-clang.kBIWlA`: configure Ninja Debug with static libraries on, shared libraries off, `BUILD_NETWORK_LIB=OFF`, `BUILD_DEBUG_LIB=OFF`, `BUILD_THREAD_LIB=OFF`, `BUILD_LANGUAGE_SERVER=OFF`, `BUILD_RUST_BINDING=OFF`, `BUILD_CLI=OFF`, `BUILD_TESTS=ON`; the same four-target build completed 861/861. The same focused CTest regex passed 3/3; direct `bin/zr_vm_pre_semantic_ir_test` passed 101/101. `CMAKE_CXX_COMPILER` was unused with those optional components disabled.
- MSVC adjacent `ssa_construction` and both source short-circuit CTests passed 3/3; direct `zr_vm_pre_semantic_ir_test.exe` passed 101/101.
- The test initially expected an extra exit block after an explicit return; corrected to the actual eight-block graph before judging production behavior.
- A WSL build directory under the Windows mount spent nearly ten minutes generating; `/tmp` did not survive between WSL calls. Reconfigured on the persistent WSL home filesystem instead. These are validation-environment failures, not source test failures.
- Full repository CTest, sanitizer and other source-expression contexts were not run for this focused loop CFG slice; no cross-backend parity is claimed.

## Results

- Focused MSVC, GCC, and Clang source CFG/build/oracle tests passed. Unsupported leaves remain analysis-only. The original negative golden emits expected diagnostic text yet exits with 101/101 passing tests.

## Acceptance Decision

- Accepted for nested source `while`/`for` short-circuit conditions and their direct Oracle coverage. Other source-expression contexts, the whole 01.02 plan, and M1 parity remain open.
