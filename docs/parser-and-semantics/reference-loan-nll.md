---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
plan_sources:
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/parser/test_reference_loan_nll.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_resource_owner_borrow_receiver.c
  - tests/parser/test_property_ref_return.c
  - tests/acceptance/2026-07-20-syntax-02-m3-reference-loan-nll.md
doc_type: module-detail
---

# Reference Loan Liveness And NLL

Syntax plan 02 M3 makes shared and mutable loans a CFG fact rather than a lexical
side effect. `BorrowShared`, `BorrowMut`, and `Reborrow` create stable LoanIds;
the flow result publishes instruction live-in/live-out sets and one region fact
per loan. VM and AOT do not maintain a runtime borrow table.

## Reference-value propagation

Each loan seeds the semantic value named by `createdByValueId`. Copy, move and
conversion preserve its LoanId. A ref value stored in a Place and loaded later
also preserves the LoanId.

Place-backed ref values use a forward CFG reaching-value calculation. The
analysis first discovers Places that can carry a loan, then runs exact kill/gen
on only those Places:

- Store and Initialize replace the current Place loan set with the input value's
  set. Writing a non-ref value therefore kills the previous ref value.
- Load reads the reaching set at that instruction.
- predecessor states are unioned at joins, so a loaded value may conservatively
  represent loans from multiple incoming paths.
- loop back edges participate in the same fixed point.

This two-stage calculation prevents a historical assignment from remaining
attached to every later Load while avoiding a dense all-Place state for Places
that never carry references.

Place-to-result instructions also consume the currently reaching Place loan set.
Consequently, a later load or conversion keeps an owner-derived reference loan
live through its real final use instead of ending it at the intermediate store.

Reference-return property getters reuse that same propagation. `PROPERTY_REF_GET` creates a reference
value tied to its receiver/source Place; `DEREFERENCE` projects the referent Place, and later load or
store instructions consume the same LoanId. A value read therefore does not erase the owner or
receiver region, while `ref propertyAccess` retains reference identity directly.

## NLL and reborrow

The analysis collects every semantic value use, explicit loan access and
`EndLoan`, then computes LoanId liveness backward across CFG successors. A loan
is killed at its creation instruction during backward transfer, which bounds the
region at the origin. The last reachable use, rather than the containing lexical
block, determines the normal end of the region.

Semantic value ids are reusable and are not SSA definitions. Instruction-use construction therefore
masks a loan before the actual `BorrowShared`, `BorrowMut`, `Reserve`, or `Reborrow` instruction that
created it, instead of consulting the current definition attached to a reused ValueId. The mask is
local to instruction uses: global Place propagation and CFG joins keep their complete reaching-loan
sets. This prevents a future property receiver loan from appearing on earlier instructions without
dropping legitimate branch-join facts.

A `Reborrow` derives its direct parent set from the input ref value. A ref loaded
after a CFG join may have multiple possible parents; all remain in the child
ancestor closure and the public region marks the parent as multiple. The
transitive ancestor graph is closed before liveness and a parent cycle rejects
the analysis instead of entering an unbounded traversal.

Every reachable reborrow must have at least one parent. Its instruction Place
and published source Place must be the same as, or a narrower projection of,
each possible parent's source Place. Unknown overlap from a dynamic index or
dereference remains conservative; a reborrow cannot widen `obj.field` back to
`obj` or switch to a disjoint Place.

Parent closure is added to use and live sets, so a child cannot outlive any
possible parent. Conflict checking allows an instruction that explicitly
accesses a loan to pass through that loan and its ancestors only when the root
loan has the required capability. A shared loan authorizes reads and shared
reborrow, while exclusive access and mutable reborrow require a mutable loan.
A live child still suspends or freezes incompatible access through every parent.
When the child reaches its last use, a parent is available again if it remains
live for a later use.

## Conflict matrix

Every Place access is compared with live-in loans through
`ZrParser_PlaceGraph_Overlap()`:

- shared loans coexist with shared loans and reads;
- a mutable loan conflicts with another shared or mutable borrow;
- Store, Initialize, Move and Drop conflict with every overlapping live loan;
- owner `share` construction consumes the Unique source and is classified as an
  exclusive access while an overlapping loan is live;
- a reference-return property keeps its receiver/owner loan live through the dereference result's
  last use, so move, drop, share, and into-GC operations remain conflicting until that point;
- direct reads conflict with an overlapping mutable loan;
- disjoint projections do not conflict;
- unknown overlap is rejected conservatively and remains distinguishable from
  proven overlap in the diagnostic payload.

Access through the active loan or its reborrow ancestry is exempt from the
corresponding source-Place restriction. This is the semantic distinction between
using a reference and bypassing it to access the original Place.

## Published facts and diagnostics

`SZrSemanticFlowResult` owns instruction liveness arrays and loan region facts.
The public query surface answers whether a LoanId is live before or after an
instruction and returns the origin, parent, first live instruction and last use.
Reset and free release every nested liveness array before reanalysis.

Loan-conflict diagnostics contain the conflict instruction/range, related LoanId
and PlaceId, overlap classification, source Place declaration, loan origin and
last-use range. Unknown dynamic-index aliasing therefore reports an unknown
overlap instead of claiming that the indices are equal.

The loan passes consume the reachability facts produced by the base semantic
flow. Instructions in disconnected blocks publish empty liveness and cannot
create diagnostics or contaminate a reachable join.

## Milestone boundary

Syntax 02 M3 establishes canonical loan instructions, Place overlap and owned
CFG ranges without adding a second source passing-mode system. Syntax 02 M4
connects general readonly receivers and two-phase calls. Syntax 04 M3 now reuses
those facts for Unique/Shared owner reborrow, owner receiver access, and
drop/share/move conflicts; see `resource-owner-borrow-receiver.md`. Reference
escape, closure and suspension remain governed by the dedicated escape pass.

Syntax 05 M4 adds property-produced references without adding a parallel lifetime system. Property
and accessor identity select the producer, while this module remains the source of truth for LoanId
liveness, Place overlap and last-use conflicts.
