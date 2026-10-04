---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_intervals.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.h
  - tests/parser/test_ssa_branch_range_null.c
  - tests/cmake/ssa-branch-range-null-tests.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.h
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
tests:
  - tests/parser/test_ssa_branch_range_null.c
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
  - tests/parser/test_ssa_branch_loop_ranges.c
doc_type: module-detail
---

# ExecIR branch range and null analysis

`exec_ir_branch_facts.h` exposes a read-only analysis producer for the reachable
acyclic portion and a bounded single natural loop shape of an ExecIR function. It produces signed 64-bit intervals and
explicit null states at block entries and successor occurrences. It does not
rewrite the graph or delete checks. The 02.02 loop and consumer gates remain open.

## Representation and witness contract

Type tokens are opaque identities. Integer-looking or null-looking tokens do
not prove signed representation or nullability. `signedValues` provides explicit
upstream representation witnesses for SSA values. External entry facts can also
provide signed representation, interval bounds and explicit null states. Entry
facts require external SSA definitions, reject duplicate values and reversed
intervals, and reject NULL that contradicts explicit NONNULL metadata.

The producer owns copies of all witness data. Caller witness arrays only need
to remain alive during Analyze. The upstream proof producer changes revision
or generation when its evidence changes. A local signed domain witness never
makes a definition available at an earlier location.

No-pool CONSTANT layoutId becomes an exact immediate interval only when its
result has an explicit signed representation witness. Pool-backed constants
remain UNKNOWN: their type IDs, flags and bits have no assumed format here.
Explicit value NONNULL metadata is propagated; NULLABLE means UNKNOWN.
No branch condition is treated as a null comparison, and no nil COMPARE is added.

## Transfer and joins

COPY and MOVE propagate existing SSA facts. Checked ADD/SUB derive complete
signed i64 bounds only when both operands and the result have explicit domain
witnesses, both input intervals are complete, and every endpoint calculation
is representable. Flags, effect tokens and memory tokens block this inference.
Possible endpoint overflow or poisoned input produces `overflowed=true` with
no bounds; it does not mark an edge unreachable. Missing domain/bounds remain
UNKNOWN. `overflowed=false` alone is never a nonoverflow proof. Conversions,
other arithmetic and unknown results do not derive interval bounds. Existing
immutable SSA facts survive calls; no facts about mutable memory are produced.

For COMPARE selectors 0 through 5 (equal, less, less/equal, greater,
greater/equal, unequal), available nonpoisoned signed intervals refine both
operands on true and false successor occurrences. Equality intersects intervals;
ordered comparisons project endpoint constraints using snapshots of both inputs.
Swapped operands reverse direction. Strict comparisons at INT64_MIN/MAX become
unreachable without overflowing. Unequal comparisons retain singleton endpoint
exclusions; interior holes remain the original interval. Compare flags, effect
tokens and memory tokens block refinement. See the detailed
[arithmetic and constraints contract](../instruction-generation/execir-branch-arithmetic-constraints.md).

Block entry joins take the convex hull over reachable incoming edges. A bound
survives only if every path has that bound. Null states survive only when paths
agree. PHI operands are evaluated at their predecessor edge environments;
unreachable edges do not contribute. Parallel true and false edges remain
separate occurrences, including when they target the same block. A local value
missing on an incoming path has no queryable fact at the joined entry.

Reachable cycles first attempt the bounded four-block single natural loop
described in [the loop module](ssa-branch-loop-range-analysis.md). The helper
stages complete environments, widens/narrows the header, and checks an additional
post-fixpoint before publication. Unsupported shapes or unproven convergence
retain the successful all-UNKNOWN fallback and set `cyclicFallback`. No signed
or null proof is exported for fallback environments. An unreachable cycle does
not suppress proofs in the reachable DAG. Current loop runtime GREEN is pending.

## Ownership, validity and errors

Initialize a result before Analyze and free it when finished. Analyze retires
the prior result even when rebuilding fails, builds into temporary owned memory,
and publishes only a successful result. Array products are checked before
allocation. Failure uses existing diagnostic kinds and IR/source sites.

The module must own the exact function pointer; a matching ID is insufficient.
Storage counts/capacities and products are checked before reads. The Core
STRUCTURE and SSA verifier validates the graph before dataflow. The producer
also checks edge multiplicity and occurrence ownership.

The module and function are borrowed and must remain alive while the result is
used. Before consuming a query as proof, call `BranchFactsIsCurrent` with current
revision and generation. It revalidates binding/structure and compares an IR
and constant-pool fingerprint. Unreachable or unavailable locations return NULL;
available UNKNOWN values return an explicit fact with no bounds/null conclusion.
Raw struct padding in the fingerprint may cause conservative cache misses.

Throwing terminator results are unavailable on exceptional successors;
suspension result availability is left UNKNOWN/unavailable in this subset.
The current 53-case fixture has no dedicated call, throwing, suspension or
allocation-failure cases. Those paths are not runtime accepted by this matrix.
The `overflowed` field records input evidence; its default false value is not
an independently established proof that deferred arithmetic cannot overflow.

## Evidence and remaining gates

Offline Lua `lua/src/lvm.c` distinguishes integer/float comparisons and
`lua/testes/math.lua` checks integer extrema. Offline JDK
`lua/jdk/src/hotspot/share/opto/castnode.cpp` pins constraint facts to control
and widens ranges at loop-related boundaries. This producer keeps facts scoped
to CFG occurrences; its finite loop scope and remaining general-loop gates are
documented separately in the loop module.

The unit matrix covers predicates, swapped operands, integer endpoints,
opaque formats, explicit null metadata, witness ownership, invalid IR,
stale/rebuilt results, PHI joins, parallel edges, nested/contradictory paths,
reachable/unreachable cycles and a 128-block DAG. Actual execution evidence is
recorded in [the scoped acceptance note](ssa-branch-facts-scoped-acceptance.md):
Root native Windows Clang/LLD run and repeat each passed 53 cases. UBSan covered
the producer and fixture, with 19 current plain Core objects reused.
`tests/cmake/ssa-branch-range-null-tests.cmake` defines the standalone target
`zr_vm_ssa_branch_range_null_test` and CTest `ssa_branch_range_null`; its parent
include and CMake configure/build acceptance remain separate integration work.
Runtime differential, pooled-format
proof production, general loop widening/narrowing, consumer check elimination, proof
remarks and allocation fault injection remain separate acceptance gates.
