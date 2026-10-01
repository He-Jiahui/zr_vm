---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch_storage.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch_rules.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch_rules.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch_storage.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch_rules.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_scalar_scratch_rules.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
  - tests/acceptance/ssa-source-execbc-vm.md
tests:
  - tests/parser/test_exec_ir_scalar_scratch_eligibility.c
  - tests/cmake/exec-ir-scalar-scratch-eligibility.cmake
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_diagnostics.inc
  - tests/acceptance/ssa-source-execbc-vm.md
doc_type: module-detail
status: focused-msvc-validated
---

# Direct scalar literal scratch promotion

## Purpose

Source lowering currently gives literal results private temporary Places and
represents their initialization with `PLACE_BASE` and `INITIALIZE`. This narrow
proof lets ExecIR's existing place-promotion pass represent a compiler-owned
bool or signed i64 literal directly as an SSA value. The source conditional
then reaches the already-supported scalar ExecBC materializer without adding a
runtime exception for memory effects.

The proof is separate metadata on `SZrSemanticIrFunction`. It does not change
`PlaceBaseKind`, and it does not make arbitrary `TEMPORARY` Places promotable.
The ordinary memory-backed representation remains in place whenever the
compiler cannot produce the proof or the builder cannot recheck it.

## Behavior model

The side-table row contains only:

- a root `placeId`;
- its direct `constantValueId`;
- the canonical `typeId` from the owning semantic context;
- the expected runtime scalar type (`bool` or signed `i64`).

The compiler records a row while lowering a literal only after it confirms
that the value is defined by a direct SemIR `CONSTANT`, that the instruction
has a constant-pool index, that the index is within the live compiler pool,
that the canonical type is primitive, and that canonical type, literal type,
and actual `SZrTypeValue.type` all agree. Other types and malformed pool
entries keep the normal place representation.

ExecIR Build treats a row as a candidate, not as authority. It validates the
whole proof array shape and rejects duplicate place or constant identities for
promotion. A row is usable only when all of these checks hold:

1. The Place is a root `TEMPORARY`, has no projections or descendants, and is
   not referenced by a local, loan, escape, contiguous-view, or bounds fact.
2. Its base identity is unique among root Places across base kinds. The
   PlaceGraph reports `DISJOINT` against every other Place; `UNKNOWN` and
   `OVERLAP` both keep the temporary in memory.
3. The referenced SemIR value and instruction still form a direct `CONSTANT`
   with `hasConstantPoolIndex`, matching proof type and Place type.
4. Exactly one `PLACE_BASE` and one `INITIALIZE` refer to the Place. The
   constant and base precede initialization. Any load, store, borrow, call
   related place operation, or other operation naming the Place rejects the
   candidate.
5. The generated ExecIR constant value and Place-address value have the
   canonical type ID recorded by the proof. That ID need not equal a runtime
   `EZrValueType` tag.

A malformed or absent optional proof disables this promotion. It does not
turn arbitrary SemIR into an ExecIR build failure. The existing eligibility
logic still assigns ordinary `PLACE_ADDRESS` flags; only a candidate passing
all checks receives `PROMOTABLE_PLACE`.

## Proof storage and allocation behavior

`ZrParser_SemanticIrFunction_Init` initializes `scalarScratchProofs` and
`ZrParser_SemanticIrFunction_Free` releases it. Compiler reset uses those same
functions. Proof append validates the existing array and candidate before
mutation. If capacity is exhausted, it checks the capacity and byte-count
arithmetic, allocates a new buffer through `ZrCore_Memory_RawMallocWithType`,
copies the old rows and candidate, then publishes the new head/capacity/length
and frees the old allocation. The core raw allocator returns null on failure;
it does not throw. A null result leaves the original pointer, rows, count, and
capacity unchanged. Array initialization has no error return, so an
uninitialized/invalid proof array is treated as having no trusted proofs and
cannot enable promotion.

`semantic_ir_scalar_scratch.c` contains the allocation-free validation used by
standalone builder targets. Proof append and buffer ownership live in
`semantic_ir_scalar_scratch_storage.c`; both use the same validation helpers.

The side table follows the existing `SZrSemanticIrFunction` ownership:

- `compiler_semantic_ir_isolation_begin` transfers the complete function
  descriptor to the isolation record, initializes a fresh child function,
  then `compiler_semantic_ir_isolation_end` frees the child and restores the
  original descriptor. The proof buffer moves with that descriptor and is
  neither separately copied nor freed during the transfer.
- Parent compiler snapshots in `compiler_function.c` and lambda snapshots in
  `compile_expression_lambda.c` copy the descriptor for read-only semantic
  analysis. These snapshots are borrowed views; they do not free or mutate the
  proof array.
- ExecIR Build and CFG normalization read borrowed descriptors. The
  normalizer shallow-copies the function, owns only its new block/edge arrays,
  and its dispose frees only those arrays.
- CFG finalization stages a shallow compiler copy but separately clones and
  commits only the CFG, instructions, source map, and value operands. It does
  not mutate or dispose the borrowed proof array, so no proof clone is needed
  for that transaction.

## Pool-validation boundary

The compiler classifier checks the actual compiler constant pool before it
records a proof. ExecIR Build receives SemIR but no compiler pool, so it can
recheck only the direct `CONSTANT` definition, `hasConstantPoolIndex`, and
type/proof consistency. It cannot verify that the index refers to a real
module constant. Core's separate owned-module constant verifier checks pool
bounds and opaque type identities when the module supplies constants, as
documented in `docs/core-runtime/exec-ir-module-constants.md`. The source
fixture supplies the actual compiler pool to its Oracle and projection.

## Test coverage and current status

`exec_ir_scalar_scratch_eligibility` directly exercises the production
eligibility and side-table code. Its cases cover the accepted literal shape,
missing and malformed proof rows, duplicate rows, unknown proof types, wrong
constant and type, missing pool provenance, compiler classifier range/type
rejection, ExecIR token mismatch, load-before-initialize, later store, other
place operations, loan and escape facts, same-id TEMPORARY/LOCAL roots,
unknown PlaceGraph overlap, projections, and append allocation failure with
unchanged existing rows.

`ssa_source_execbc_vm` covers real source parsing and compilation through
SemIR, published BuildModule identity, Oracle, projection, materialization,
and the Core dispatcher. It runs both outcomes of a source conditional and
compares returns 9 and 8. Its trace checks each observed VM PC against the
ExecIR PC/source map and checks that the collapsed VM blocks are CFG-valid and
end at the Oracle's reported final block. The Oracle exposes only its final
block, so the test does not claim complete block-history equality. These
branches do not merge values; loop-carried source phi coverage remains open.

Root's current MSVC build and CTest passed the eligibility target and both
source branches in `current-scalar-shape-fixtures-ctest.log`. The eligibility
fixture uses canonical ID 191, which differs from runtime bool/i64 tags,
and rejects a mismatched canonical ID and malformed Place array descriptors.
The source path's earlier materialization `UNSUPPORTED` RED is retained in
the source acceptance record. Current GCC/Clang and the wider semantic matrix
remain open; the independent conditional-cleanup failure is recorded without
claiming the full eight-suite selection passed.

## Plan sources and limits

This slice supports only direct bool and signed i64 literal temporaries with
the proof and exclusivity conditions above. It does not promote general
temporaries, remove arbitrary stores, relax materializer memory-token guards,
or add source loop-phi coverage. SSA 01.02 promotion gates and the full 01.05
Oracle/ExecBC/AOTIR/C/LLVM matrix remain open.
