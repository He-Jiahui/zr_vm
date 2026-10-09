---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_scalar_constant.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_scalar_constant.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/parser/support/ssa_source_scalar_constant_noalloc_shim.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_scalar_constant.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_scalar_constant.c
plan_sources:
  - docs/plans/ssa
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
  - .codex/plans/20261008-ssa-source-scalar-constant-source-tests-design.md
tests:
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/acceptance/ssa-source-scalar-constant.md
doc_type: module-detail
status: source-scalar-constant-finite-green
---

# Scalar constants from the actual compiler snapshot

## Responsibility and current state

`ZrParser_ExecIr_MakeSourceScalarConstant` constructs one independent Core
`SZrExecIrConstant` from a live, finalized literal SCRIPT compiler snapshot.
It is a prerequisite for retained returned-source metadata. It consumes the
actual compiler/runtime pools and canonical types; source text and fixture
expected return values are not API inputs.

The implementation performs ordered read-only admission and constructs the
constant from actual canonical and typed pool facts. It clears the entire
optional diagnostic, preserves output on refusal, and publishes one local
candidate only after every check succeeds. Its only nonlocal lookup helpers
are the existing read-only canonical binary search and string-length accessor.

Actual current validation has all24 default cases passing,0 failures/ignores,
with no allocator or UBSan error. The finite three-unit MSVC compilation has
zero exits and warnings. The invalid RETURN operand test was independently
corrected and committed as `9d8283a6542734b2137c688721eed9c736102abb`.
The lower metadata-string-heap cleanup fault found in the first attempt was
repaired and independently committed as `5bd92350596d95c9dd99a217b8e98efd335a294a`.

## API and ownership

```c
TZrBool ZrParser_ExecIr_MakeSourceScalarConstant(
    const struct SZrCompilerState *compiler,
    SZrExecIrConstant *output, SZrExecIrDiagnostic *diagnostic);
```

The caller keeps the current State, Global, SCRIPT AST, Function, SemanticIR,
canonical context and typed backing allocations alive and serializes mutations.
Output and optional diagnostic are distinct writable storage, disjoint from
input owners. Geometry establishes bounded spans; arbitrary pointer readability
and historical allocation sizes remain caller preconditions.

Every refusal preserves every output byte and source owner. Success publishes
a fully zeroed local candidate: actual CONSTANT `typeId` becomes `typeToken`,
flags are zero, and compiler `nativeInt64` converted to UInt64 becomes `bits`.
Both typed pool tags must be INT64 before either union member is read. No
borrow, ownership transfer or allocation survives the call. The returned value
can remain live after compiler/AST/Function destruction.

## Finite admission and check order

The accepted profile contains exactly one I64 CONSTANT at pool index zero,
the existing inert PLACE_BASE and RETURN of that constant in one terminated
entry CFG block. Dead-address proof belongs to the separate source-place pass.
The bridge does not claim general constant-pool serialization.

Checks run in this order:

1. NULL arguments, State/context agreement and typed-array geometry.
2. Actual SCRIPT/current snapshot eligibility and certified terminated SemIR.
3. Finalized callable identity, current canonical FUNCTION, schema, symbol,
   signature, no arguments/receiver/effects and nonnullable I64 return.
4. Actual opcode, source map, CFG, definition, operand and pool-index facts.
5. Compiler/runtime pool counts and actual index before row dereference.
6. Canonical CONSTANT kind, source/result/return type agreement and RETURN
   operand identity; then pool tags and signed I64 value agreement.

Typed arrays require correct element width, validity, length <= capacity,
storage for nonzero capacity, checked multiplication and checked address spans.
Canonical lookup is a read-only binary search of `canonicalTypes`; its geometry
and sorted nonzero IDs must be established first. Resolve kind before reading
the canonical union. Empty callable parameter contracts still need valid
geometry. Certification flags do not excuse contradictory live facts.

The implementation uses local checks for this profile. The general SemIR
validator can reject stale value facts before the required source/result type
diagnostic. `ResolveValueFacts`, canonical interning, identity finalization,
module binding and compiler producers are outside the bridge call.

## Diagnostics and side effects

The public header records precise diagnostic precedence and payloads.
`functionToken`, `blockId`, `instructionId` remain zero. `sourceId` is the safely
identified offending SemIR instruction ID, or zero before identification.
Numeric comparisons use `expectedHash`/`actualHash`; schema comparisons use
`expectedVersion`/`actualVersion`. Other fields are reset individually by the
whole diagnostic initialization. Diagnostic storage may be NULL.

There is no allocation/free, VM execution, compilation, callback, interning,
module append, compaction or hash generation inside this API. The no-allocation
shim instruments only its actual private OBJECT. Independent VM allocator
counts cover the call, and read-only helper closure review is required: the
shim alone cannot establish arbitrary uninstrumented helper nonallocation.

## Validation and integration limits

The real literal fixture parses, canonicalizes, compiles, validates, assembles,
finalizes and builds source9/source8. Twenty-four default Unity cases cover
prerequisites, independent values, optional diagnostics, eligibility, geometry,
pool/type/value disagreement, actual signature witnesses and a fresh Build.
Faults restore before assertions; fresh module, fixture, saved GC state and
Global cleanup are registered even after a failed assertion.

Actual source9/source8 trace confirms one live SemIR value in each fixture:
CONSTANT result1, PLACE_BASE result0 and RETURN operand1. Case22 explicitly
checks the invalid RETURN operand sentinel in a bounded live slot. A different
valid live SemIR value is absent from these producers, so that variant remains
OPEN. Geometry overflow, identity schema and nested-header variations receive
source review here and are not advertised as isolated executed tests.

The fresh-Build case appends only the production bridge result, then invokes
the existing allocating binder outside the bridge observation interval. It
does not copy the fixture's constants. No Oracle or source program is executed.

Detailed actual evidence belongs to the [acceptance record](../acceptance/ssa-source-scalar-constant.md)
and [test record](../../tests/acceptance/ssa-source-scalar-constant.md).
Returned-source retention, normal-route migration, native parity, Linux, the
full MSVC matrix and all47 SSA leaves remain OPEN.
