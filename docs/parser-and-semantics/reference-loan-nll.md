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
tests:
  - tests/parser/test_reference_loan_nll.c
  - tests/parser/test_pre_semantic_ir.c
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

## NLL and reborrow

The analysis collects every semantic value use, explicit loan access and
`EndLoan`, then computes LoanId liveness backward across CFG successors. A loan
is killed at its creation instruction during backward transfer, which bounds the
region at the origin. The last reachable use, rather than the containing lexical
block, determines the normal end of the region.

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

M3 consumes canonical loan instructions, Place overlap and owned CFG ranges. It
does not add a second source passing-mode system. M4 connects readonly receivers,
owner auto-deref, two-phase receiver borrowing and remaining call-boundary loan
creation to these facts. M5 owns escape, closure and suspension rejection.
