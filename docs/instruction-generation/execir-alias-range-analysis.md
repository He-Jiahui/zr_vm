---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_alias.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
  - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
tests:
  - tests/parser/test_ssa_gvn_range.c
  - tests/acceptance/ssa-gvn-conversion-result-type.md
  - tests/acceptance/ssa-bounds-proof-mutable-length.md
  - tests/acceptance/ssa-alias-zero-generation.md
  - tests/acceptance/2026-10-02-ssa-gvn-move-consumption.md
doc_type: implementation-note
status: implemented
---

# ExecIR alias facts

Alias classification is parser-owned and uses stable base/projection IDs; it
never stores runtime pointers.  The lattice is `must-alias`, `disjoint`,
`may-alias`, and `unknown`.

Identical stable base and projection identities produce `must-alias`. Distinct
allocation or stack bases are `disjoint` only while both are stable and
unescaped. Distinct parameter or external-handle bases remain `unknown`
because callers and FFI may alias them. Distinct projections are disjoint only
when lowering supplies an explicit layout proof (`projectionDisjoint`) for the
same layout; projection IDs alone are not evidence.

Unknown writes, missing (`generation == 0`) or mismatched generation identities,
invalid/unknown bases, or escaped external locations force `unknown`. A stable
base, layout, and projection still cannot prove `must-alias` or `disjoint`
without a matching nonzero generation on both locations. Consumers must retain
loads and guards whenever the query is not a positive proof. The two-location
query has no active analysis-generation argument, so callers must separately
check freshness against their current analysis state; two matching but stale
nonzero generations cannot be detected by this query alone.

Range and shape facts are generation-scoped. A bounds proof requires known
lower and upper bounds for both index and length, matching generations, no
arithmetic overflow, non-negative bounds, and `index.upper < length.lower`.
Missing lower bounds, an inverted index or length interval, a mutable length
fact on either operand, overflow, or mismatched generations return false, even
when callers bypass the fact container and query the proof function directly.
The direct query cannot distinguish two equally stale generations from the
active generation; callers must validate fact freshness against their current
analysis container before using a positive proof.
The proof-only bounds-elision API then returns false without reporting an
execution error; callers must retain the check. Shape facts require nonzero
type/layout/shape identities and are
discarded on generation invalidation. Edge-sensitive range propagation remains
a subsequent stage; the current GVN boundary is described below.

## Conservative GVN and guard use

GVN folds syntactically identical pure scalar operations when the candidate
precedes the use in the same basic block or its block dominates the use's block.
It rejects memory/effect-token users, `LOAD`, calls, allocation, barriers, drop,
throwing operations, GC, suspension, and all terminators. `MOVE` is also excluded:
it consumes its source even when instruction flags and memory/effect tokens are
empty. A second move of the same source must still encounter the consumed value;
replacing it with a copy of the first result would incorrectly initialize its
result. The focused regression checks the source's initialized-to-moved
transition and the second result's uninitialized state before and after repeated
GVN runs using Core owner-state analysis. Ordinary scalar `COPY` remains a
candidate, with an explicit positive reuse regression.

A duplicate is rewritten to `COPY` of the dominating result while retaining
its original result ID; this avoids assuming that physical instruction order
proves dominance for arbitrary later uses. Invalid function storage and
block instruction ranges return a structured diagnostic before any rewrite.
Malformed operand/result ranges are not reusable candidates. An allocation
failure during a later rewrite can
leave earlier successful rewrites in place, so callers requiring atomicity
must use the pass manager's transactional function clone.

The key also compares the result Value's canonical type. In particular,
`CONVERT` may omit the instruction `typeToken`: direct execution and ExecBC
projection then select its result Value type. Identical input operands with
different implicit target types cannot be commoned, even if every instruction
field matches. Equally typed repeats in the same block may still become
`COPY`. Missing or undersized Value storage is rejected before the pass reads
the result type. Dominating pure-value reuse and sibling-block rejection are
covered; edge-sensitive range propagation and memory/guard proof elimination
remain 02.02 work. General ownership availability across intervening operations
requires further analysis; excluding `MOVE` only establishes the consumption
boundary covered by this regression.

The bounds-elision API is proof-only: it does not delete an instruction and
returns false for incomplete range facts. Callers retain the original guard on
false and emit their own missed/blocked remark with the relevant source ID.
