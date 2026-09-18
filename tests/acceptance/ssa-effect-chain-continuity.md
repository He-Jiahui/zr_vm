# SSA M1: local observable effect-chain continuity

## Scope

- Plan: `docs/plans/ssa/01-execir-ssa/03-effects-verifier.md`, batch 3.
- Layer: core ExecIR effect verifier; no production executor or AOT emitter changed.
- Two observable operations in the same basic block must connect `effectOut`
  to the following `effectIn`. A pure instruction between them does not reset
  the chain. Different CFG blocks are **not** treated as one linear path.

## Baseline and RED

- On main at `47f4ffe9`, before the production edit, GCC built the focused
  `zr_vm_ssa_effects_verifier_test`; the focused CTest exited 1 with
  `FAIL: an observable effect skipped its preceding token`. The old check
  only rejected decreasing token IDs, so `effectOut=2` followed by
  `effectIn=3` was incorrectly accepted.
- The checkout contains unrelated pre-existing edits. These results cover
  only the named source, test and documentation, not a clean-tree release run.

## Test inventory

- Focused negative: two `CALL`s separated by a `NOP`, with effect tokens
  `1 -> 2`, then `3 -> 4`; assert `EFFECT_TOKEN` on the second call with
  block 1, instruction 3, source 552, expected 2 and actual 3.
- Focused positive: change the second `effectIn` to 2; the same fixture passes.
- Existing focused cases cover malformed ranges, descending memory tokens,
  incorrect PHI predecessors, SSA dominance, and exceptional `INVOKE` results.
- Adjacent consumers: `ssa_core_model`, `ssa_state_maps`,
  `ssa_oracle_projections`, `ssa_pass_manager_scalar`.
- Memory alias partitions, cross-block effect PHIs, exception path joins,
  and actual backend parity remain outside this local slice; M1 remains open.

## Tooling evidence (2026-09-17)

WSL GCC 11.4 and Clang 14, each with its existing `build/ssa-*-debug`
configuration, built the five named test executables and ran:

```text
ctest --test-dir build/ssa-gcc-debug -R 'ssa_(effects_verifier|core_model|state_maps|oracle_projections|pass_manager_scalar)$' --output-on-failure --no-tests=error
ctest --test-dir build/ssa-clang-debug -R 'ssa_(effects_verifier|core_model|state_maps|oracle_projections|pass_manager_scalar)$' --output-on-failure --no-tests=error
```

Both runs reported 5/5 tests passed. Windows MSVC 19.44 through the
VS development environment built those five targets in
`build/ssa-msvc-debug` and ran the same CTest selector: 5/5 passed.
The existing MSVC command line emitted D9025 (`/W3` overridden by `/W4`)
while compiling; no test failed.

For memory/undefined-behavior instrumentation, WSL GCC built the focused
fixture with `-std=c11 -g -O1 -fsanitize=address,undefined
-fno-omit-frame-pointer`, `-Izr_vm_core/include`,
`-Izr_vm_common/include`, and these source files:

```text
tests/parser/test_ssa_effects_verifier.c
zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
zr_vm_core/src/zr_vm_core/execution_contract.c
```

The resulting `/tmp/ssa_effect_chain_asan` exited 0 with
`ssa effects verifier PASS`; no sanitizer diagnostics appeared. This test
creates and frees its own ExecIR module; no ownership transfer, host pointers,
external side effects, or runtime leases are exercised.

## Acceptance decision

Accepted for the **same-block chain continuity slice** only. No M1 or full
SSA-plan acceptance is claimed. The next verifier slice must check CFG edges,
token PHIs and per-region memory versions before proving optimizer reordering
safe across branches.
