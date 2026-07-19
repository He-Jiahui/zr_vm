---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
plan_sources:
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_pre_semantic_ir.c
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

The old `SZrSemIrInstruction` table is an execution compatibility projection. `compiler_semir.c` builds it only after final ExecBC assembly. It is not consulted when the compiler chooses local or ownership semantics, and it may legitimately be empty for source programs whose front-end Semantic IR is non-empty.

## Flow Facts

`ZrParser_SemanticFlow_Analyze` runs a fixed-point worklist over the function CFG. Each reachable block owns separate entry and exit state for every Place:

- initialization: uninitialized, initialized, or maybe initialized;
- availability: available, moved, maybe moved, or dropped;
- borrowing: a conservative shared-loan set plus an optional mutable loan;
- escape: local, function, caller, heap/static, or unknown;
- reachability: recorded independently on the block facts.

Unreachable predecessors do not participate in joins. Initialization and availability join conservatively, shared loans are unioned, incompatible mutable loans become a multiple-loan conflict state, and escape takes the widest bound. A store restores an assignable moved Place to initialized/available, reads reject an active mutable loan, and initialize/store/move/drop reject conflicting shared or mutable loans. Ending one concrete loan never clears a multiple-loan join. The diagnostic pass reports uninitialized or maybe-uninitialized use, use after move/drop, maybe-moved use, loan conflicts, and escape violations with instruction, Place, loan, block, and source-range identities.

## Boundaries

This graph remains compilation-session data. M4 decides which canonical public contracts and hashes enter `.zrs`, `.zri`, and `.zro`; local Place, block, loan, and flow state do not become `.zro` ABI by default. M5 migrates VM, AOT, LSP, reflection, and debug consumers to canonical projections. Execution-side optimization and quickening continue to consume assembled execution structures rather than redefining front-end semantics.

## Verification

`test_pre_semantic_ir.c` fixes the complete opcode-family golden, a source-level local initialize/load/store golden, explicit ownership-operation and shared-loan lowering, structural validation before execution-sidecar construction, CFG join negatives for definite assignment, move availability, loan conflicts, and caller escape, plus store-after-move and conservative mutable-loan-end regressions. The compiler integration and ownership suites protect existing ExecBC behavior while the new semantic source is introduced.
