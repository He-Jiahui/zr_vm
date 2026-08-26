---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/parser/test_resource_owner_borrow_receiver.c
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/parser/test_pre_semantic_ir.c
doc_type: module-detail
---

# Resource Owner Borrow And Receiver

## Purpose

Syntax 04 M3 connects `Unique<T>` and `Shared<T>` to the canonical reference-loan
checker. Owner values can participate in readonly calls and `in T` passing
without being consumed, while mutable calls remain exclusive. The compiler
publishes the same Place, LoanId, receiver effect, and reference provenance facts
used by ordinary references; runtime Borrow/Loan state is not a correctness gate.

## Capability Contract

The ownership shell determines the available auto-deref capability:

- `Unique<T>` supplies readonly and writable receiver access.
- `Shared<T>` supplies readonly receiver access only.
- `Weak<T>` supplies guarded readonly target access. One chain-level guard wakes
  a hidden `Shared<T>` owner; direct absence throws `NullReferenceError`, while
  optional absence returns null or a void no-op and skips the complete suffix.
- Explicit `wake(weak)` remains the ownership intrinsic for materializing a
  nullable `Shared<T>` value outside a target-access chain.
- An `in T` parameter may receive a compatible `Unique<T>` or `Shared<T>` as a
  shared reborrow when the canonical inner type matches.

The final call-compatibility check and overload scoring share one mode-aware
predicate. It accepts only passing mode `in`, an unqualified parameter type, and
a canonical owner shell with the same inner contract. Value, `out`, `ref`,
generic, and name-only matches are not widened.

## Receiver And Argument Lowering

Direct owner member calls retain the owner source Place and stage a borrowed
receiver. Readonly methods create shared receiver access. Writable methods on a
Unique receiver create restricted mutable access; the same call on Shared is
rejected before execution. Weak direct and optional access consume a canonical
`ReceiverGuardFact`, evaluate the receiver once, and materialize at most one
hidden Shared owner for the dominated postfix suffix. Direct access requires a
live target and throws the named null-reference error on expiry; optional access
branches around getters, indexes, nested calls, and argument evaluation.

The hidden owner survives allocation, GC pressure, and nested execution. It is
released on normal success, null short circuit, throw unwind, and scope cleanup.
A method/property ref or other ref-like result tied to that temporary wake owner
cannot escape the guarded chain.

Passing a direct owner local to `in T` uses the same shared reborrow path. This
keeps the source available after the call and avoids publishing a canonical
move. Nested readonly uses can coexist. A writable nested use conflicts while a
shared receiver loan is live, and a later move is legal once the final borrowed
use has ended.

The runtime projection still emits the existing ownership instruction family so
VM and AOT retain compatible frame behavior. Those instructions mirror an
already accepted compile-time fact; they do not maintain an independent borrow
table.

## Loan Liveness And Owner Consumption

Reference-bearing Places contribute their reaching LoanIds to later
place-to-result instructions. A later load therefore extends the originating
loan to its true last use. While that loan is live, `drop`, Unique move, and
`share` of the overlapping owner Place are exclusive accesses and are rejected.
The source operations `share`, `degrade`, `wake`, `intoGc`, and `drop` are
represented only by `OwnershipIntrinsicFact`; consuming operations carry a
canonical PlaceId and conflict analysis classifies that fact as consumption of
the source owner. No member spelling or legacy construct kind selects ownership
semantics.

If there is no later reference use, non-lexical liveness contracts the loan and
the subsequent owner move is accepted. A canonical Unique value passed by value
is lowered as a move, invalidates the source semantic value, and does not get
misclassified as a second `own` construction.

## Reference Result Provenance

When a direct source member declares a reference return, the result inherits the
receiver Place and escape bound. Assigning that result to a local transfers the
loan through the conversion/result temporary instead of ending the receiver loan
at the call. The escape pass then rejects returning that reference beyond the
owner borrow.

This proof is intentionally narrow: the declared owner inner type must resolve
to a direct source class or struct TypeDef, and a direct method with the same
spelling must expose reference access. If overloads share that spelling, any
reference-return declaration is treated conservatively as receiver-tied until a
later resolved-overload identity is available. External descriptors, inherited
or chained receiver expressions, and unavailable source declarations remain
unknown rather than being widened from arbitrary source text.

## Test Coverage

`test_resource_owner_borrow_receiver.c` covers Unique/Shared `in` reborrow,
readonly and writable receivers, guarded Weak reads, two-phase nested calls,
active reference conflicts with drop/share/move, last-use NLL, reference-return
escape (including method/property ref-like results from a temporary Weak wake),
and the absence of runtime Loan correctness dependencies.
`test_ownership_intrinsic_member_separation.c` covers dedicated intrinsic facts,
same-name member dispatch, direct/optional Weak access, single receiver
evaluation, skipped getters/indexes/arguments, hidden-owner cleanup, named
errors, and GC pressure. The lower-level `test_pre_semantic_ir.c` cases freeze
Place-to-result liveness, owner consumption conflicts, and the no-later-use
boundary.

## Milestone Boundary

M3 completes compile-time owner reborrow and direct receiver enforcement. It does
not add a runtime borrow table. Syntax 04 M4 consumes its canonical Place/LoanId
facts for `intoGc(owner)`: the operation is an exclusive owner
consumption, is rejected while an overlapping loan is live, and leaves the source
Place moved.

M4 also adds `GcDomain`, canonical `Gc<T>` / `GcBox<T>` bridge kinds, explicit
ownership roots, and cross-domain write rejection. It does not widen the M3
receiver proof or infer a bridge from member spelling. Multi-mutator handoff and
cross-domain transport remain M5/M6 work.

The 2026-08-26 ownership/object-member convergence extends the original M3
receiver boundary with fact-driven Weak guards without restoring an ownership
member-call mode. Production parser C/H has no removed percent-prefixed source
branches, and the removed ownership-member selector has no references. The
current focused GCC runner passes 35/35; stable GCC/Clang/MSVC full-graph replay
remains the final acceptance gate.
