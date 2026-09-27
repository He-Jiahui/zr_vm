# SSA 01.02: terminator and block successor edge order

## Scope

- Structural verification compares the final terminator's successor count and
  every ordered target with its owning block's successor row. A separate pool
  row is permitted when its contents agree, including repeated targets.
- The direct Oracle and the common ExecBC/AOT projection preflight enforce
  the same contract before execution allocations or projection publication.
  An inconsistent instruction row cannot silently select a different phi
  predecessor edge from the block's ordered adjacency.
- This is a focused 01.02 CFG validation stage, not completion of the SSA
  construction plan or a new serialized edge-ID format.

## RED and Fixtures

- In `ssa_oracle_parallel_edges`, a valid conditional branch has a separate
  instruction successor row `[2, 3]` matching its block's `[2, 3]`. After
  projecting distinct phi-copy edge blocks, the fixture changes only the
  instruction row to `[3, 2]`. Before the fix, GCC reported
  `FAIL: core verifier accepted a reordered terminator successor row`. After
  adding structural verification, GCC reported
  `FAIL: ExecBC projection accepted a reordered edge or replaced published output`.
- Structural verification now reports `INVALID_BLOCK`, block 1, instruction 1,
  expected target 2 and actual target 3. The direct Oracle reports the same
  block/instruction before allocating a result; ExecBC rejects the mutation
  while preserving its previously published phi moves; AOT rejects it without
  publishing moves. The original independent-but-equal rows still pass.
- `ssa_core_model` also truncates a valid two-edge conditional terminator row
  to one slot; its block retains two edges. It checks `INVALID_BLOCK`, block 1,
  instruction 1, expected count 2 and actual count 1.
- `ssa_effects_verifier` temporarily drops a cleanup edge when replacing its
  exceptional branch with a return, so its existing exception-value assertion
  continues testing value reachability on a structurally valid graph.

## Verification (2026-09-27)

- WSL GCC: 7/7 (`ssa_core_model`, `ssa_effects_verifier`,
  `ssa_builder_dominance`, `ssa_builder_control_edges`, `ssa_place_promotion`,
  `ssa_oracle_projections`, `ssa_oracle_parallel_edges`).
- WSL Clang: 5/5 (`ssa_core_model`, `ssa_effects_verifier`,
  `ssa_builder_control_edges`, `ssa_oracle_projections`,
  `ssa_oracle_parallel_edges`).
- Windows MSVC Debug/static: the same 5/5.
- GCC ASan/UBSan with leak detection: 3/3 (`ssa_core_model`,
  `ssa_effects_verifier`, `ssa_oracle_parallel_edges`).

```text
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-gcc.4pVemu -R '^ssa_(core_model|oracle_parallel_edges|oracle_projections|place_promotion|effects_verifier|builder_dominance|builder_control_edges)$' --output-on-failure --no-tests=error
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-clang.kBIWlA -R '^ssa_(core_model|effects_verifier|oracle_parallel_edges|oracle_projections|builder_control_edges)$' --output-on-failure --no-tests=error
ctest --test-dir build/codex-ssa-conversion-msvc-static -R '^ssa_(core_model|effects_verifier|oracle_parallel_edges|oracle_projections|builder_control_edges)$' --output-on-failure --no-tests=error
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir /home/hejiahui/zrvm-ssa-phi-asan -R '^ssa_(core_model|effects_verifier|oracle_parallel_edges)$' --output-on-failure --no-tests=error
```

An expanded GCC run also included `ssa_oracle_resume` (21/22 subtests passed).
Its pre-existing AOT frame-layout move fixture compares the source pointer
after the move (already null) with the destination's owned pointer; that
assertion is unrelated to CFG validation and was not changed in this stage.
The updated source uses existing allocations only: validation itself does not
allocate; on failure Oracle has no values and both projection APIs preserve
their previous output. Cancellation and repeated invocation behavior are not
new entry points here. `exec_ir_projection_common.c` already exceeds 1,000
lines; this small change remains inside its existing CFG preflight. Extracting
`zr_projection_validate` is the next coherent split if that module grows.
