---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_intervals.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - tests/parser/test_ssa_branch_loop_ranges.c
  - tests/cmake/ssa-branch-loop-ranges-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
  - .codex/plans/20261004-ssa-branch-loop-widening-narrowing.md
tests:
  - tests/parser/test_ssa_branch_loop_ranges.c
  - tests/parser/test_ssa_branch_range_null.c
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
doc_type: module-detail
---

# Bounded natural loop range analysis

This private module extends the existing BranchFacts Analyze API. It is called
only when the reachable graph has a cycle; the DAG path remains unchanged.
Current direct-workspace native Windows evidence accepts the finite 11-case
loop fixture, the original 53-case foundation and the 40-case arithmetic
regression. Acceptance is limited to these targets; the whole V27/V28 receipts
retain their recorded failures. See the scoped evidence below.

## Supported input

Core STRUCTURE|SSA, storage and occurrence validation precede the private call.
The module must own the exact function pointer. The matched function has four
distinct blocks and six instructions: entry BRANCH; header COMPARE and conditional
branch; body ADD/SUB and BRANCH to header; exit RETURN of the induction value.
The true successor is the body, the false successor the exit. Block roles are
derived from actual CFG relationships, not IDs or array order. The single header
PHI has precisely entry initial and body update incoming occurrences. Extra
entries, parallel occurrences, nesting, extra instructions, effects, memory,
flags, binding/deopt/layout metadata, block effect/memory PHI side fields,
nonterminator successor ranges, or exceptional boundaries are unsupported.
Core already rejects nonzero matchTypeToken on the matched opcodes.

The compare is ordered LT/LE/GT/GE and uses induction and invariant limit;
swapped compare operands are handled by the existing refinement hook. ADD may
have induction on either side; SUB must be induction minus step. Initial, step
and limit are distinct external SSA values, each with an available, unpoisoned,
complete signed singleton entry fact. PHI and update results require independent
signed representation witnesses. Type tokens, constant pool bits and advisory
induction metadata do not establish representation. Missing evidence uses the
legacy successful UNKNOWN fallback.

## Equations, widening and narrowing

Scratch reachability distinguishes bottom edges from reachable UNKNOWN values.
For each candidate header H, the helper reconstructs entry, header, both branch
occurrences, body/backedge and exit environments. The candidate header is the
join of entry and reachable backedge; its PHI projects initial and update values
from the corresponding environments. A bottom backedge contributes nothing;
a reachable unavailable value participates in the ordinary conservative join.
Local update and condition values are never made available at earlier entries.

Only the header widens. An outward-moving lower or upper endpoint is removed;
availability and domain can only weaken and overflow poison remains sticky.
Both branch occurrences are recomputed on every sweep, so an initially bottom
exit can become reachable after widening. Losing an already propagated edge
during widening rejects the attempt and falls back.

After widening stabilizes, F(H) must be covered by H. Narrowing restores bounds
from the complete candidate equations; it cannot create availability/domain or
clear poison. Each narrowing iteration checks coverage first, and a final fresh
evaluation checks closure again before publishing. Coverage compares availability,
signed domain, poison, null state and endpoint inclusion. A candidate UNKNOWN or
poison cannot justify a finite unpoisoned fact. All published downstream vectors
and reachability come from the same final F(H), rather than stale earlier sweeps.
Closure proves all iterations, not a bounded execution prefix. Exit reachability
expresses possible execution and does not prove termination.

For initial0, step1, limit10 and LT, widening loses the header upper bound, then
the true edge caps induction at9. Checked update gives [1,10], so entry/backedge
join restores header [0,10]; the false edge gives exit10. With step2, an exit can
overshoot the limit; no rule substitutes the limit as the exit value. Zero-trip
loops exclude the body before transfer. Zero step or opposite direction never
receives an assumed eventual-exit proof.

## Arithmetic, budgets and ownership

The module reuses existing entry, transfer, refinement and join hooks; it has no
second arithmetic evaluator. ADD/SUB require representable endpoints before host
arithmetic. Possible overflow produces poison with no bounds, including inclusive
MAX guards. Narrowing cannot erase that poison. A dead body is not transferred.

Named private limits allow at most 4096 scratch fact cells, 65536 work units, and
8 widening and 8 narrowing sweeps. Four entries, four exits, four edges and two
header vectors require 14 value vectors. Products are checked before allocation.
The existing producer allocates its separately checked result arrays before this
attempt; this cap does not bound that unchanged allocation. Each complete
evaluation charges 32 value visits per value plus 16 fixed instruction/edge visits;
this allowance covers reconstruction and the associated coverage/update passes.
The finite shape and cell limit bound initialization and matching too. Budget
exhaustion or failed closure discards all scratch facts and uses legacy fallback.
There is no elapsed-time correctness criterion and no new public options API.

One owned scratch allocation borrows the result's witness context. Hooks operate
on scratch arrays and retain no pointers. CONVERGED copies complete entries,
exits, edges and reachability to the already owned result. UNSUPPORTED/UNPROVEN
leave those arrays unchanged for the original fallback. OOM/product overflow
report existing diagnostics; Analyze's prior-result retirement and failure
cleanup remain unchanged. IR, PHI rows and effects are never rewritten.

## Validation and remaining scope

[Scoped evidence](ssa-branch-loop-ranges-scoped-acceptance.md) records historical
11-case RED and current direct-workspace target acceptance. Root V27 completed
loop11 and foundation53 with all 22 project C TUs UBSan instrumented; V28 completed
arithmetic40 using the current fixture and audited V27 support objects. The
independent adoption reports check actual dependency pins and link participation.
The V26 build typo `phiIncoming` was corrected to `phiRange` before those runs.
Each focused target needs the private loop TU in addition to its fixture,
BranchFacts producer and 19 Core support TUs. The three standalone fragments
include it. The normal parser module recursively discovers source files through
CommonMacros. Fresh formal CMake acceptance and repeat runs remain open here.

This finite shape does not close 02.02 or the full 47-leaf milestone. General loops, nested/multiple backedges,
production witness creation, mutable-memory invalidation, proof remarks, null
and bounds check consumers, configurable budgets, allocation fault injection and
backend differential acceptance remain open. No check is deleted by this module.
