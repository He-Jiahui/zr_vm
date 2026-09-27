# SSA 01.02: one terminator per nonempty block

## Scope and RED

- A one-block, single-`RETURN` function is a valid structural and projection
  fixture. Appending a second `RETURN` to the same block produced RED failures
  under WSL GCC: `structure accepted a terminator before the end of its block`
  and `oracle executed before checking for an early terminator`.
- Structure verification now checks every instruction before the final one
  and reports `MISSING_TERMINATOR` at block 1, instruction 1, with expected
  final instruction 2 and actual early terminator 1. Oracle preflight checks
  the same shape before execution state allocation; ExecBC/AOT projection
  preflight rejects it before replacing a previously published instruction
  array. The valid one-return baseline still projects successfully.
- This stage enforces the nonempty-block uniqueness rule, not completion of
  all SSA 01.02 source CFG and exception construction requirements.

## Verification (2026-09-27)

- WSL GCC: `ssa_core_model`, `ssa_effects_verifier`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, `ssa_place_promotion`, `ssa_oracle_resume`,
  `ssa_oracle_projections`, and `ssa_oracle_parallel_edges` pass 8/8.
- WSL Clang and Windows MSVC Debug/static: core model, effects verifier,
  oracle resume, oracle projections and oracle parallel edges each pass 5/5.
- GCC ASan/UBSan with leak detection: core model, effects verifier, oracle
  resume and oracle parallel edges pass 4/4.

```text
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-gcc.4pVemu -R '^ssa_(core_model|effects_verifier|builder_dominance|builder_control_edges|place_promotion|oracle_resume|oracle_projections|oracle_parallel_edges)$' --output-on-failure --no-tests=error
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-clang.kBIWlA -R '^ssa_(core_model|effects_verifier|oracle_resume|oracle_projections|oracle_parallel_edges)$' --output-on-failure --no-tests=error
ctest --test-dir build/codex-ssa-conversion-msvc-static -R '^ssa_(core_model|effects_verifier|oracle_resume|oracle_projections|oracle_parallel_edges)$' --output-on-failure --no-tests=error
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir /home/hejiahui/zrvm-ssa-phi-asan -R '^ssa_(core_model|effects_verifier|oracle_resume|oracle_parallel_edges)$' --output-on-failure --no-tests=error
```

The validator changes allocate nothing and do not change public entry points.
The projection source file is already over 1,000 lines; the additional check
remains within its existing graph preflight, with extraction of
`zr_projection_validate` reserved for a future coherent module split.
