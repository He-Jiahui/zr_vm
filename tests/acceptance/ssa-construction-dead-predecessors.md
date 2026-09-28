---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
tests:
  - tests/parser/test_ssa_construction.c
status: partial
---

# SSA construction with unreachable predecessors

## Scope

This checkpoint covers parser Place promotion when a retained CFG contains an
unreachable predecessor. It follows the reachability and pruned-phi contract
in SSA plan 01.02. The builder keeps source CFG blocks and source maps for
diagnostics; promotion must ignore unreachable predecessors while calculating
frontiers, and must keep a Place in memory form when a required phi would need
an incoming definition from an unreachable block.

## Baseline and RED

The D-drive WSL GCC Debug cache at
`/mnt/d/tmp/zr_vm/close-proxy-core-red` built the current
`test_ssa_construction.c` fixture and reported 11 Unity cases with two
failures. The one-live-plus-dead-predecessor case and the two-live-plus-dead
join case both failed because frontier walking followed the dead predecessor's
invalid immediate dominator. GDB stopped at
`promotion_compute_frontiers` in `exec_ir_ssa_promotion.c:457` with
`ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK`, join block 4, instruction 8. The stack
ran from frontier construction through Place promotion and the public builder.

## Test inventory

- `test_ssa_construction_ignores_dead_predecessor_during_promotion` retains a
  dead predecessor edge and block, verifies its invalid idom and source map,
  expects the reachable store/load to promote without a phi, and runs both
  structure and SSA verification.
- `test_ssa_construction_keeps_mixed_dead_join_in_memory_form` retains two
  reachable branches plus one unreachable incoming edge, checks every edge and
  source map, requires no phi and requires all relevant stores/loads to remain
  in memory form, then runs both structure and SSA verification.
- Existing ordinary diamond, unread-phi pruning, loop-carried phi, and atomic
  missing-definition cases remain in the same builder integration target.

## Tooling evidence

WSL GCC 11.4.0, CMake 3.22.1, and Ninja 1.10.1 used the existing cache on D:

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_construction_test -- -j4
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^ssa_construction$' --output-on-failure --no-tests=error
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_construction_test
```

After the fix, the focused CTest passed 1/1 and the direct executable reported
11 tests, zero failures, and zero ignored. The adjacent selection built the
`ssa_dominator_cfg`, builder, cleanup-state, and Place eligibility/promotion
targets, then ran:

```bash
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --output-on-failure --no-tests=error \
  -R '^(ssa_construction|ssa_dominator_cfg|ssa_builder_cfg|ssa_linear_effects_builder|ssa_cfg_effects_builder|ssa_builder_dominance|ssa_builder_control_edges|ssa_builder_cleanup_dispatch|ssa_cleanup_exception_state|ssa_builder_fact_identity|ssa_builder_iterator_invokes|ssa_place_eligibility|ssa_place_promotion)$'
```

The adjacent selection passed 13/13. The build emitted existing
`-Wmissing-braces` warnings from untouched builder and Place-promotion test
sources. An initial Windows-host CMake/CTest invocation was discarded because
it interpreted the WSL cache as `D:/...`; it ran no valid GCC tests. All
counted build and test commands above ran inside WSL against the D-drive cache.

## Results

Frontier computation now skips only a validated CFG predecessor known to be
unreachable from the freshly computed dominators; invalid IDs and a broken
reachable idom walk still report `INVALID_BLOCK`. After phi placement, a Place
whose required phi would have an unreachable incoming edge is returned to
memory form and its pending phi row is cleared before append/rename. The
single-live-input case still promotes, while the mixed join keeps valid STORE
and LOAD operations. Both outputs preserve CFG/source maps and pass structure
and SSA verification.

## Acceptance decision

Accepted as a focused unreachable-predecessor promotion checkpoint. Full
01.02 acceptance remains open for source-level exceptional/cleanup/suspend
shapes, backend parity, sanitizer coverage, and the full plan validation gate.
