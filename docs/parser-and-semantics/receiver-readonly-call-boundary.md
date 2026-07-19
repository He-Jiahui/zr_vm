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
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_activation.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
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

The packed v34 `SZrCompiledMemberInfo` layout remains exactly 31 `TZrUInt32`
words and has a compile-time size assertion. Callable receiver effect projects
through its existing `isConst` bit; import reconstruction applies that bit only
to instance methods and non-constructor meta methods. Fields and other member
kinds never acquire a receiver effect from field constness. A readonly
requirement cannot be implemented or overridden by a writable receiver, while a
writable requirement may use a readonly implementation because it has fewer
effects. Dispatch still exposes the requirement declared by the selected
base/interface contract.

Native instance methods publish the boolean `isReadonlyReceiver` metadata field.
`ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER` is the explicit descriptor flag;
runtime-only flags such as `READONLY_INLINE_VALUE_CONTEXT` do not imply a
semantic receiver contract. Static native methods and constructors always
normalize to receiver-none. The builtin `IArrayLike` and container Array/Map
get-item descriptors explicitly carry the readonly receiver flag; corresponding
set-item descriptors remain mutable.

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
instruction. Structural validation rejects missing or duplicate activation;
forward CFG dataflow rejects activation whose reservation is not available on
every incoming path.

`ZrParser_SemanticFlow_LoanIsActiveAt()` distinguishes a live reservation from
an active mutable loan using definite-active instruction facts rather than
comparing globally assigned instruction IDs. Must-active facts answer public
queries, may-active facts conservatively drive conflict checks at joins and loop
headers, and available-reservation facts validate activation origins. Runtime
bytecode does not maintain a borrow table; these checks remain pre-execution
semantic facts.

## Compiler integration and Place identity

The source compiler runs structural validation and semantic-flow analysis over
its pre-Semantic-IR before publishing executable output. M4 builds an
execution-order entry/exit CFG for the call-scoped sidecar; later milestones may
replace it with full statement CFG lowering without changing the loan APIs.
Compiler state records every generated receiver LoanId, so the publication gate
promotes conflicts involving either immediate shared or two-phase mutable
receiver loans without promoting unrelated legacy sidecar diagnostics.

Receiver loan identity is independent of ABI staging slots. Direct local roots
use canonical local Places, and each bound field in a member chain emits a field
projection keyed by the resolved member identity. Thus
`holder.buffer.push(holder.buffer.mutate())` aliases the same projected Place and
is rejected, while executable argument/receiver staging remains unchanged. The
legal `buffer.push(buffer.read())` path is compiled and executed in acceptance
tests. A readonly outer call such as `buffer.observe(buffer.mutate())` is also
rejected because its shared receiver loan remains live during argument
evaluation.

## Semantic identity boundary

Receiver effect alone is not a call-target identity. Any semantic query or LSP
invalidation consumer that reasons about a receiver call must consume the
resolved target's `SymbolId` and declaration range from a canonical reference
fact/query, together with the selected member contract. It must not infer the
target from the member name.

M4 preserves `receiverEffect` on source and imported members and keeps the
existing `SZrSemanticReferenceFact` identity contract (`symbolId`,
`declarationRange`, resolved state) as the required publication shape.
Publishing the resolved receiver-call target into that canonical fact/query
surface is a later M6 consumer-integration boundary. Until it is present, LSP
must report unknown or fall back conservatively rather than guessing by member
name.

## Milestone boundary

M4 covers receiver readonly capability, owner auto-deref, override/interface
variance, artifact/native metadata and two-phase call-scoped loans. M5 owns
caller/function/heap escape, ref return, closure capture and suspension rules.
M6 owns final canonical consumer convergence, including resolved receiver-call
target fact publication.
