---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/receiver_call.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/receiver_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/receiver_call.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/receiver_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_activation.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
plan_sources:
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/core/test_type_layout_inline_copy.c
  - tests/parser/test_aot_llvm_symbol_stripping.c
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_canonical_consumers.c
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

Syntax plan 03 M2 extends the same contract to contextual `readonly struct`.
Every instance field is frozen after constructor completion. An ordinary instance
`fn` is normalized to readonly, an explicit `const fn` is equivalent, and a
static method remains receiver-none. The constructor keeps a mutable initializing
receiver so it can populate the destination Place. The readonly capability is
also published on the canonical TypeDef instead of being rediscovered from the
source spelling.

Class property accessors use the same rule as interface property contracts:
getters are readonly and setters are mutable. A readonly view may execute a
getter but setter binding fails at the receiver-capability gate before bytecode
publication.

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

Readonly inline-struct calls do not materialize an object wrapper or defensive
copy. The caller's hidden argument is a frame-layout alias with
`ALIAS | INLINE_RECEIVER_ARGUMENT`; it retains the source Place's layout and
never emits `SET_STACK` or stores a second payload. The callee receiver slot
stores only a `SZrFunctionFrameBorrowedAliasBinding` and is marked
`ALIAS | INDIRECT_ALIAS | BORROWED_ALIAS`. That binding carries the source call
identity, source frame anchor and source stack slot, so Place resolution follows
the original local/field/array alias even after stack relocation instead of
caching a raw address.

Call-window staging deliberately leaves borrowed receiver arguments
unmaterialized. VM pre-call binds the callee alias before ordinary inline
parameter copies; copy and writeback paths skip it. Tail-frame reuse remains
disabled because the source frame must stay live for the borrowed call. Ordinary
locals, `in T`, and `ref readonly T` use the same alias contract. Writable struct
receivers retain the existing value staging and post-call writeback path.

Generated AOT direct calls use the same VM pre-call contract rather than
reimplementing frame initialization. The runtime allocates a scratch call window
after the caller's full frame storage top, not after its dense/generated slot
count. Scalar and ordinary VALUE arguments are staged there; an inline borrowed
receiver is left unmaterialized so `BindAndCopyInlineFrameParametersFromCaller`
resolves it from the caller frame identity. The cached callee size is
`ZrCore_Function_GetFrameStorageSlotCount()`, which includes byte-backed inline
layout storage.

Every allocation boundary protects the call window, destination and caller frame
with stack anchors. After the shared PreCall and its CALL debug hook, the AOT
runtime derives the callee top from the relocated `callInfo->functionBase`.
Failure cleanup first lets core roll back initialized frame ownership, then
discards the remaining staging values. A direct AOT return still closes callee
upvalues before PostCall because it bypasses the interpreter return path. The
root and hard-split AOT runtime mirrors keep this sequence aligned.

Borrowed frame aliases are serialized only in function artifact v1 patch 35 or
newer. The loader rejects the exact borrowed flag combination on older patches,
unknown frame flags, non-inline slot kinds, undersized/misaligned bindings and
bindings outside frame storage. This prevents an older runtime or malformed
artifact from interpreting inline payload bytes as a live caller-frame alias.

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

Syntax plan 03 M2 also publishes exact source unbound method references as
canonical member-access facts. The fact contains the function TypeId with its
hidden receiver effect, resolved SymbolId and declaration range. Schema-v1
callable signature roundtrip retains readonly versus mutable receiver effects.
The publisher requires a source declaration plus fully available parameter and
return types. It revalidates declaration parameter count, rejects variadic
methods, requires every parameter AST type to convert exactly to its recorded
prototype type, and requires a non-generic owner and method. An open generic,
missing parameter type or otherwise incomplete reference stays unavailable
rather than being reconstructed from a type or member name.

## Milestone boundary

M4 covers receiver readonly capability, owner auto-deref, override/interface
variance, artifact/native metadata and two-phase call-scoped loans. M5 owns
caller/function/heap escape, ref return, closure capture and suspension rules.
M6 owns final canonical consumer convergence, including resolved receiver-call
target fact publication. Syntax plan 03 M2 builds on those contracts for
readonly struct execution, property accessors, and exact unbound method-reference
facts without changing their ownership or query boundaries.
