# SSA 01.02: parallel CFG edge reciprocity

## Scope

- Core structural verification and Oracle preflight pair each occurrence of
  an edge in the source successor row with the corresponding occurrence in
  the destination predecessor row, including repeated source/destination IDs.
- An unmatched occurrence reports `INVALID_BLOCK` and the owning block before
  Oracle execution allocates or consumes an unselected phi edge.
- This stage does not introduce a serialized edge-ID field, enforce all
  per-instruction terminator/block successor contracts, or close 01.02's
  exception, cleanup, suspension, and full SSA construction requirements.

## RED and test inventory

- MSVC baseline after adding tests: `ssa_core_model` failed with
  `FAIL: unmatched duplicate predecessor accepted`; `ssa_oracle_parallel_edges`
  failed with `FAIL: oracle accepted a phi edge omitted by source adjacency`.
- `ssa_core_model`: one reciprocal edge and two paired duplicates pass;
  missing one-sided adjacency and surplus duplicates in both directions fail
  with `INVALID_BLOCK` and source/destination identities.
- `ssa_oracle_parallel_edges`: matched duplicate edges choose different phi
  values, even through branch/switch. Missing adjacency fails before execution
  even when only the still-valid first edge would be selected; an invalid
  destination predecessor range fails before any cross-block edge lookup.
- Existing ExecBC and AOT projection tests retain transactional rejection of
  unpaired duplicates and positive parallel phi lowering.

## Verification (2026-09-27)

After rebuilding all five targets in each toolchain, the following selector
passed **5/5** on Windows MSVC 19.44 Debug/static, WSL GCC 11.4, and WSL
Clang 14:

```text
ctest --test-dir build/codex-ssa-conversion-msvc-static -R '^ssa_(core_model|oracle_parallel_edges|effects_verifier|pass_manager_scalar|oracle_projections)$' --output-on-failure --no-tests=error
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-gcc.4pVemu -R '^ssa_(core_model|oracle_parallel_edges|effects_verifier|pass_manager_scalar|oracle_projections)$' --output-on-failure --no-tests=error
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-clang.kBIWlA -R '^ssa_(core_model|oracle_parallel_edges|effects_verifier|pass_manager_scalar|oracle_projections)$' --output-on-failure --no-tests=error
```

The two WSL cache paths are local validation artifacts, not source inputs.
