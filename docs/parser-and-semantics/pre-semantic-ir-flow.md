---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/bound_expression.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/bound_expression.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_value_construct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/bound_expression.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/bound_expression.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_value_construct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
plan_sources:
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_struct_value_init.c
  - tests/acceptance/2026-07-19-syntax-01-m3-pre-semantic-ir.md
doc_type: module-detail
---

# Pre-Execution Semantic IR And Flow Facts

## Purpose

M3 introduces a semantic function that exists before final ExecBC assembly. It gives reads, writes, initialization, moves, copies, drops, borrows, calls, construction, cleanup, properties, and destructuring explicit identities instead of recovering their meaning from execution bytecode.

`SZrSemanticIrFunction` owns its canonical symbol/callable IDs, Place graph, CFG blocks, locals, values, instructions, flat operand storage, regions, cleanup scopes, source map, and loan facts. Instructions use `TypeId`, `PlaceId`, `ValueId`, `LoanId`, and block IDs. VM stack slots remain private to the compiler bridge and are not part of the public semantic instruction contract.

## Instruction Contract

The public opcode set covers constants and conversions; Place construction and projection; load/store/initialize/move/copy/drop; borrow/reborrow/end-loan/dereference; typed, virtual, dynamic, and meta calls; control flow; scope and cleanup; distinct value, aggregate, field, union, GC, and ownership construction; resolved property operations; and evaluate-once destructuring operations.

Value construction, ordinary/meta calls, GC allocation, and ownership construction have different opcodes. Ownership construction additionally records an explicit unique/share/weak/upgrade operation; move, drop, shared borrow, and mutable borrow remain their own opcodes. No generic construct flag or default fallback opcode is used to reinterpret one family as another. Golden formatting is stable and includes instruction ID, opcode name, TypeId, PlaceId, input ValueId, and result ValueId.

`ZrParser_SemanticIr_Validate` rejects dangling Place/Value/Loan/Region/Cleanup references, malformed operand spans, non-sequential instruction/source-map identities, and invalid owned CFG ranges or edges. Empty CFG storage is valid during straight-line compiler emission; once blocks exist, entry/exit IDs, instruction ranges, terminators, and typed edges are checked.

## Compiler Bridge

Every compiler state owns an independent pre-execution semantic function and a private stack-slot bridge. A declared local is registered with the canonical `TypeId` and `SymbolId` already assigned by the semantic context. Parameters, foreach bindings, and compiler-generated locals are materialized on first semantic use so existing compilation paths do not lose Place identity.

For the current lowering surface, local initialization, identifier load, local store, and ownership operations emit semantic instructions first. The bridge then selects `GET_STACK`, `SET_STACK`, or the exact `OWN_*` ExecBC opcode from that emitted semantic instruction; AST callers no longer make a second load/store/move/borrow decision. Both percent directives such as `%borrow(owner)` and point-form operations such as `owner.borrow()` use this path. `%borrow` creates a shared loan, while the exclusive `%loan` path creates a mutable loan; both carry explicit loan facts and regions. `%detach` is a move, `%release` is a drop, and owner transformations use `own.construct` with an explicit operation. Unsupported ownership kinds fail instead of falling through to construction. Script compilation validates the complete pre-execution function before final function assembly, optimization sidecars, and quickening.

Struct value construction follows the same semantic-first rule. The contextual `init TypeRef(...)` syntax produces a dedicated AST node, and `SZrBoundValueConstruct` resolves the canonical constructor plus named/default argument mapping. Lowering emits `VALUE_CONSTRUCT(destinationPlaceId, typeId, constructorId, arguments)` before ExecBC selection. Local, field, fixed-array element, and return construction all pass the final destination Place into this path; ordinary call, GC allocation, and ownership construction remain separate and do not serve as fallback routes.

The private stack-slot bridge is backed by a growable array. Any operation that
materializes another slot can relocate that storage, so lowering code carries
slot contents as value snapshots across materialization and re-resolves a slot
by `stackSlot` immediately before a writeback. This applies to value construction,
contiguous-view/bounds facts, receiver projections, and property-ref load/store;
no semantic instruction may retain an interior slot pointer across a possible
array growth.

The old `SZrSemIrInstruction` table is an execution compatibility projection. `compiler_semir.c` builds it only after final ExecBC assembly. It is not consulted when the compiler chooses local or ownership semantics, and it may legitimately be empty for source programs whose front-end Semantic IR is non-empty.

## Flow Facts

`ZrParser_SemanticFlow_Analyze` runs fixed-point analyses over the function CFG. Each reachable block owns separate entry and exit state for every Place:

- initialization: uninitialized, initialized, or maybe initialized;
- availability: available, moved, maybe moved, or dropped;
- borrowing: a conservative shared-loan set plus an optional mutable loan;
- escape: local, function, caller, heap/static, or unknown;
- reachability: recorded independently on the block facts.

Unreachable predecessors do not participate in joins. Initialization and availability join conservatively, and escape takes the widest bound. A store restores an assignable moved Place to initialized/available. Shared/mutable borrowing is then replaced by the backward CFG liveness result: ref values propagate through values and Place Store/Load, Store performs kill/gen for overwritten ref slots, reborrow keeps its parent live, and each block receives the exact live loan set at its entry and exit. Place access conflicts use projection overlap and end after the final possible use instead of the lexical block. The diagnostic pass reports uninitialized or maybe-uninitialized use, use after move/drop, maybe-moved use, NLL loan conflicts, and escape violations with instruction, Place, loan, block, overlap, origin, declaration, last-use, and source-range identities. See `reference-loan-nll.md` for the loan algorithm and M3 boundary.

`FIELD_INITIALIZE` requires a field-projected destination. Flow analysis marks the field initialized and records the matching parent-field bit without claiming that sibling fields are initialized. The resulting bitmap is the semantic source for partial-constructor cleanup; joins preserve the independent field facts rather than collapsing the whole aggregate to initialized.

## Boundaries

This graph remains compilation-session data. Canonical public contracts and hashes enter `.zrs`, `.zri`, and `.zro`, while local Place, block, loan, field-initialization bitmap, and flow state do not become `.zro` ABI by default. VM, AOT, LSP, reflection, and debug consume canonical projections where their contract requires them. Execution-side optimization and quickening continue to consume assembled execution structures rather than redefining front-end semantics. Owner/resource field teardown and the complete cross-function exception protocol remain Syntax 04 promotion gates; struct M1 only publishes the generic layout maps and the partial-initialization mechanism they will consume.

## Verification

`test_pre_semantic_ir.c` fixes the complete opcode-family golden, destination-bearing `VALUE_CONSTRUCT`, field-projected `FIELD_INITIALIZE`, parent cleanup bitmap behavior, a source-level local initialize/load/store golden, explicit ownership-operation and shared-loan lowering, structural validation before execution-sidecar construction, CFG join negatives for definite assignment, move availability, loan conflicts, and caller escape, plus store-after-move and NLL replacement of compatibility borrow states. `test_struct_value_init.c` covers contextual parsing, qualified/generic TypeRef targets, named/default binding, constructor isolation, destination-first local/field/array lowering, runtime constructor aliases, and partial unwind. `test_reference_loan_nll.c` covers last-use release, shared/mutable conflicts, ref-slot overwrite, branch/loop liveness, dynamic-index unknown overlap, nested reborrow, and move/drop rejection. The compiler integration and ownership suites protect existing ExecBC behavior while the new semantic source is introduced.
