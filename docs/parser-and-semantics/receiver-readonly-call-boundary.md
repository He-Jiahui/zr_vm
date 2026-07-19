---
related_code:
  - zr_vm_parser/include/zr_vm_parser/receiver_call.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/receiver_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/receiver_call.h
  - zr_vm_parser/src/zr_vm_parser/receiver_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
plan_sources:
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
tests:
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_compiler_features.c
  - tests/acceptance/2026-07-20-syntax-02-m4-receiver-readonly-call-boundary.md
doc_type: module-detail
---

# Receiver Readonly And Call Boundary

Syntax plan 02 M4 makes method receiver effects part of the canonical member
contract. Instance `fn` requires a writable receiver; `const fn` requires only a
readonly receiver. This rule is shared by source, imported, generic, dynamic and
native calls instead of being reconstructed from member names or bytecode shape.

## Surface and canonical contract

Class, struct and interface method AST nodes preserve
`receiverModifier = default | const`. Top-level `const fn` and `static const fn`
are invalid because neither declaration has an instance receiver. A contextual
`readonly T` type creates a readonly capability view; it is distinct from an
immutable `let` binding and from a `ref readonly T` referent.

`SZrTypeMemberInfo.receiverEffect` is the compiler's canonical member-level
contract:

- static members and constructors use `ZR_CANONICAL_RECEIVER_NONE`;
- ordinary instance methods and setters use `ZR_CANONICAL_RECEIVER_MUTABLE`;
- `const fn` and getters use `ZR_CANONICAL_RECEIVER_READONLY`.

`SZrCompiledMemberInfo.receiverEffect` serializes the same value for imported
modules and interface dispatch. A readonly requirement cannot be implemented or
overridden by a writable receiver, while a writable requirement may use a
readonly implementation because it has fewer effects. Dispatch still exposes
the requirement declared by the selected base/interface contract.

Native instance methods publish the boolean `isReadonlyReceiver` metadata field.
`ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER` is the explicit descriptor flag;
the existing `READONLY_INLINE_VALUE_CONTEXT` flag also implies readonly for
compatibility. Static native methods always normalize to receiver-none.

## Receiver capability analysis

`ZrParser_ReceiverCall_Analyze()` consumes a canonical TypeId, receiver effect,
dispatch kind and Place/addressability facts. `AnalyzeInferred()` is the legacy
projection entry point; it first interns the inferred type and then calls the
same canonical analyzer.

The analyzer applies one matrix to class, struct, interface, override, generic,
dynamic and native dispatch:

- `readonly T`, `ref readonly T` and `Shared<T>` may call readonly members only;
- `ref T` and writable Places may call readonly or writable members;
- a writable struct method requires an addressable Place, writable ref or owner
  auto-deref capability;
- `Unique<T>` can auto-deref to shared or mutable access;
- `Shared<T>` can auto-deref only to shared access;
- `Weak<T>` cannot auto-deref without an explicit upgrade.

Owner behavior is selected from the canonical owner node, not from the source
spelling `Unique` or `Shared`. Copying, returning or inferring a readonly view
preserves `isReadonlyView`; no conversion silently recovers writable capability.

## Two-phase receiver loans

Compiler-generated mutable receiver auto-borrows use a two-phase loan:

1. `ZR_SEMANTIC_IR_RESERVE_BORROW_MUT` is emitted before argument evaluation.
2. Read-only argument evaluation may continue while the loan is reserved.
3. `ZR_SEMANTIC_IR_ACTIVATE_LOAN` runs immediately before the call.
4. `ZR_SEMANTIC_IR_END_LOAN` ends the call-scoped receiver loan.

An explicit mutable ref uses an immediate loan and never enters this path. A
reserved receiver allows shared reads but rejects another reservation, direct
write, move and drop. Once active, it has the ordinary mutable-loan conflict
matrix. Each two-phase region fact records its phase and unique activation
instruction; malformed missing, duplicate or pre-origin activation is rejected.

`ZrParser_SemanticFlow_LoanIsActiveAt()` distinguishes a live reservation from
an active mutable loan. Runtime bytecode does not maintain a borrow table; these
checks remain pre-execution semantic facts.

## Semantic identity boundary

Receiver effect alone is not a call-target identity. Any semantic query or LSP
invalidation consumer that reasons about a receiver call must consume the
resolved target's `SymbolId` and declaration range from a canonical reference
fact/query, together with the selected member contract. It must not infer the
target from the member name.

M4 preserves `receiverEffect` on source and imported members and keeps the
existing `SZrSemanticReferenceFact` identity contract (`symbolId`,
`declarationRange`, resolved state) as the required publication shape. Publishing
the resolved receiver-call target into that fact/query surface is a later
consumer-integration boundary; until it is present, LSP must report unknown or
fall back conservatively rather than guessing by name.

## Milestone boundary

M4 covers receiver readonly capability, owner auto-deref, override/interface
variance, artifact/native metadata and two-phase call-scoped loans. M5 owns
caller/function/heap escape, ref return, closure capture and suspension rules.
M6 owns final canonical consumer convergence, including resolved receiver-call
target fact publication.
