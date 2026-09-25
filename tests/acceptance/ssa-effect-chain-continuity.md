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

## Region-local memory token slice (2026-09-25)

The verifier now accepts the tagged token form
`ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region, version)`. A managed-heap chain and an
independent native/FFI chain may advance in the same block without a false
function-wide ordering failure. A tagged token with a region absent from the
opcode schema is rejected, while a store's tagged version can be consumed by
the following load in that same region. Untagged tokens retain the legacy
global ordering contract for compatibility with existing artifacts.

GCC 11.4 Debug, Clang 14 Debug, and GCC ASan/UBSan all rebuilt and ran
`ssa_effects_verifier`; each reported `ssa effects verifier PASS` and the
corresponding CTest passed. This evidence covers region-local validation only;
Producer-side token generation and cross-block region PHIs remain open.

## Cross-block effect-token merge slice

The verifier now accepts an explicit CFG effect merge without changing the
value-SSA PHI pool. `SZrExecIrBlock.effectPhiResult` names the merged effect
token, while `effectPhiIncomings` reuses the existing edge-ordered
`SZrExecIrPhiIncoming` storage; each `value` is interpreted as an effect-token
ID for that range. Every incoming predecessor must match the block's
predecessor edge occurrence and its terminal observable `effectOut`. The merge
result must be newer than every incoming token, and the first observable
instruction in the join block must consume it. Distinct predecessor chains
without this metadata are rejected with `ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN`.

This is intentionally a verifier-side contract. Producer-side effect-token
allocation remains follow-up work.

## Cross-block memory-token merge slice

Tagged memory regions now have the same explicit CFG join contract. A block may
set one `memoryPhiResult[region]` and an edge-ordered `memoryPhiIncomings[region]`
range for each region. The verifier requires each incoming to match the
predecessor's terminal tagged version, requires the merged version to advance,
and requires the first tagged memory consumer to use the merged token. A
diamond with heap versions 2 and 3 therefore needs heap version 4 at the join;
stale inputs, a stale consumer, or a missing phi are rejected with the memory
token diagnostic. Untagged memory tokens retain their legacy global ordering.

## Parallel-edge PHI follow-up (2026-09-18)

Before the fix, the new `ssa_effects_verifier` case exited 1 on MSVC with
`FAIL: effect verifier rejected two distinct incoming CFG edge occurrences`.
The existing count and exact positional checks already require one incoming
per edge, including repeated source-block IDs. The effect phase now accepts
those parallel occurrences rather than rejecting duplicate predecessor IDs;
its foreign and out-of-order predecessor negatives remain in the same test.

MSVC 19.44 rebuilt `zr_vm_ssa_effects_verifier_test` and
`zr_vm_ssa_oracle_parallel_edges_test` under `D:/zr-ssa-verify-871bc234`;
both standalone executables exited 0. WSL GCC 11.4 rebuilt the focused effect
verifier with ASan/UBSan and ran
`/mnt/d/zr-ssa-verify-871bc234/ssa_effects_gcc_asan`: exit 0, no sanitizer
report. Producer-side token generation and full M1 acceptance remain open.
