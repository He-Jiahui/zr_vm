# SSA 01.05: edge-local phi copy scheduling

## Scope

ExecBC and AOTIR projections now own ordered physical-slot phi moves in
addition to their original parallel-copy records. A cycle uses one temporary
slot reused after the cycle has drained. Copies on different predecessor
occurrences carry distinct projected edge IDs; a branching source also splits
phi-bearing edges even when each destination has only one predecessor.
`ZrParser_ExecBcProjection_ExecutePhiMoves` consumes one selected edge through
a slot-copy callback after preflighting every move. Short slot arrays fail
before any callback; callback rejection reports the failing move, while any
rollback of earlier callback side effects remains the backend consumer's
responsibility.

## Red and green evidence

- The initial regression did not compile: neither projection defined
  `phiMoves` or `phiMoveCount`. After scheduling was added, a multi-cycle
  fixture exposed a segmentation fault. GDB showed the loop selected an
  inactive first copy on the second cycle (`phiMoveCount=10674`, six input
  copies); choosing an active copy fixed the repeated temporary write.
- A layout reserving 20 slots initially selected slot 7 for a cycle and
  failed its assertion; the temporary now uses slot 20. `UINT32_MAX` as a
  mapped slot reports `CAPACITY_OVERFLOW` at the projected edge and leaves
  the previous projection and move array intact.
- Two phi destinations reached from a branching source initially shared
  one edge tag; the regression failed until the phi-bearing fanout edges were
  split into distinct synthetic blocks. Review then identified a disjoint
  terminator successor range still bypassing those blocks. The regression
  failed again before both successor ranges were rewritten by occurrence.
- Sparse frame layouts reject aliased physical value slots and unrepresentable
  temporary slot counts. No-reuse packed frames map slot records' logical
  value IDs to their actual physical positions; packed layouts that reuse a
  physical slot report `INVALID_PROJECTION` without replacing the old plan.

The `ssa_oracle_parallel_edges` fixture executes the generated slot moves for
a 3-cycle, a dependent tail, two independent 2-cycles, a self-copy, and two
distinct parallel CFG edges. `ssa_oracle_projections` retains the existing
oracle/projection checks. These fixtures are hand-built ExecIR and have no
runtime effect, callback, or owner lease to balance.
The cycle fixture checks scheduling algebra on a projected graph, while
`test_verified_loop_backedge_phi_swap` also verifies a two-block, external-entry
loop with `STRUCTURE | SSA` before executing the entry and backedge move plans.
The production phi consumer is now exercised for both edges and for missing
callback and undersized-slot failures. A scalar instruction dispatcher and
full oracle/ExecBC event differential remain subsequent 01.05 gates.

## Validation (2026-09-26)

Each environment rebuilt `zr_vm_ssa_oracle_parallel_edges_test` and
`zr_vm_ssa_oracle_projections_test`, then ran CTest with
`-R '^ssa_oracle_(parallel_edges|projections)$' --output-on-failure
--no-tests=error`:

| Environment | Directory | Result |
| --- | --- | --- |
| WSL GCC 11.4 Debug | `build/ssa-gcc-debug` | 2/2 passed |
| WSL Clang 14 Debug | `build/ssa-clang-debug` | 2/2 passed |
| Windows MSVC 19.44 Debug | `build/ssa-msvc-debug` | 2/2 passed |
| WSL GCC ASan/UBSan | `build/ssa-gcc-asan-phase80` | 2/2 passed, no findings |

The GCC Debug `ctest -L ssa --output-on-failure --no-tests=error -j 4`
sweep reported 80/80 passing. Only the two projection targets were rebuilt
for this change; the other label-matched executables were already present in
the build directory.

The copy scheduler runs inside transactional projection construction. It
allocates only the pending-copy workspace and result array, frees both on
failed preparation, and transfers the result into ExecBC or AOTIR ownership
on success. It has no cancellation callback; repeated lowering and output
replacement use the existing projection API. This is not emitted ExecBC,
oracle/ExecBC event comparison, or native C/LLVM execution. Plan 01.05 and
the full M1 gate remain open.
