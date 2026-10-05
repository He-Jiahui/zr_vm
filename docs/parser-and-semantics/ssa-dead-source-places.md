---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
plan_sources:
  - .codex/plans/20261005-ssa-dead-source-places.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
tests:
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - tests/acceptance/ssa-dead-source-places.md
  - docs/acceptance/ssa-dead-source-places.md
doc_type: module-detail
status: dead-source-places-focused-windows-green-accepted
fixture_extraction_status: focused-windows-green-accepted
---

# Dead Source Temporary Places

## Purpose and ownership

The actual literal SCRIPT builder produces a scalar constant, a temporary
`PLACE_BASE` address and its external provenance, followed by RETURN. An unused
address still occupies a value row in shared CoreExecIR. Existing DCE and the
ExecBC VM projection do not compact that shared value storage. This entry
provides the finite prerequisite for a later primitive frame producer.

```c
TZrBool ZrParser_ExecIr_EliminateDeadSourcePlaces(
        const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context,
        const SZrExecIrFunction *input,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic);
```

`semantic`, its AST and `context` remain live borrowed source evidence. `input`
is borrowed and unchanged. `output` must be initialized and independently own
its storage. Success replaces its previous owned graph; failure preserves it.
The diagnostic pointer may be null. No source file copy is created.

## Admission and actual identity

The first slice accepts only the actual no-argument, effect-free integer
literal SCRIPT graph, or its exact already-compacted NOP form. The tested
sources are `return 9;\n` and `return 8;\n`. This is not generic place deletion.

The proof resolves the exact SemIR `symbolId` and `callableTypeId` in the live
context. The FUNCTION symbol must own the actual SCRIPT AST and that callable
TypeId; the canonical FUNCTION structural hash must match the graph signature.
Matching a structural hash through another symbol is insufficient. The return
type must be the actual canonical primitive i64 type. AST shape, SemIR CFG,
regions, instruction/value/place rows, and all source ranges must agree.

The current source graph has CONSTANT, PLACE_BASE and RETURN, one scalar value,
one TEMPORARY place and one block. It has no locals, projections, loans, escape,
view, bounds or scratch proofs. The scalar SemIR facts are VALUE ownership and
NONNULL; the actual builder maps ownership to ExecIR UNKNOWN and retains
NONNULL. Address and provenance rows must have the exact builder flags,
definitions, type and IDs. No scratch flag or proof is manufactured.

Storage is checked before verification and cloning. Ownership checks use
allocation-capacity byte spans, including reserved empty pools and nested
frame/GC/state allocations. Interior and cross-array overlaps within either
owner, or between input and output, are invalid. Arithmetic for byte spans is
checked. `input == output` is invalid.

## Shared literal source fixture

The single actual source preparation path has moved from the dead-place test
into `tests/parser/support/ssa_literal_script_fixture.h/.c`. Its public source
enum selects only `return 9;\n` or `return 8;\n`. `Prepare` performs the real
Parse → CanonicalizeAst → module Prepare → compile/validate/assemble → module
Finalize → BuildModule sequence. It checks genuine SCRIPT_ENTRY/canonical
identity, attaches actual module constants and external-place initial values,
and proves the original three-value source graph and its maps with VERIFY_ALL.
It does not compact the graph, attach a frame or implicitly execute Oracle.

The caller creates the runtime state and registers an initialized fixture with
its teardown owner before `Prepare` can assert. The fixture owns its AST,
compiler, rooted compiled function, CoreExecIR module and Oracle input buffers;
it borrows the runtime state. `Function` returns a borrowed module function.
`AssertSourceMaps` and the digest helpers observe these live records; digests
include allocation identity/capacity for within-test mutation detection and are
not canonical ABI identities.

`AssertOracle` takes a caller-owned initialized execution result. That owner
must remain reachable by teardown even if a Unity assertion aborts. The
dead-place main retains its global Oracle result, output/mutated graphs, CLI
selection and all 30 compaction/guard cases; thin adapters call shared support.
Its wrapper explicitly executes Oracle after preparation. `Free` supports
partially prepared fixtures: release module/input buffers, unroot/free the
compiled function, release compiler and clear/free AST identity. The caller
destroys the state only after every fixture and Oracle result is released.
The same support TU is explicitly attached to both CMake test target routes,
with its header included in direct validation hash metadata.

This extraction is a separate accepted finite Windows validation step:
configure/build/independent prerequisites/full CTest exited 0, prerequisites
passed 2/2 and all 30 cases passed with no observed UBSan diagnostic. Its current
main/support/CMake hashes, actual support-TU membership, logs and binary are
recorded in the [acceptance record](../acceptance/ssa-dead-source-places.md).
The final v2 extraction evidence follows removal of one extra blank line at
the support TU's EOF found during Root's staged whitespace check. Its new TU
hash is 05FB2408…CCA23; the original extraction receipt remains historical.
The production TU pin is unchanged. The older producer
GREEN and eight-suite consumer evidence remains historical evidence for the
committed pre-extraction fixture.

## Transaction and preserved records

1. Validate storage and ownership; run Core `VERIFY_ALL` on the input.
2. Refuse sealed or unsupported metadata, and independently prove the source
   identity, exact unique source maps and unused address/provenance relationship.
3. Deep-clone a private candidate. Change the proved PLACE_BASE to an
   operand/result-free NOP while preserving its instruction ID and source ID.
4. Remove the address and exclusive provenance values. Keep surviving values
   in their original order, assign dense ValueIds, and rebuild operand/result
   pools and every instruction range, including empty ranges.
5. Run `VERIFY_ALL` on the candidate, then release the previous output and
   transfer the candidate ownership. Every earlier failure releases temporary
   allocations and leaves the input and previous output unchanged.

Instruction IDs, block identity, source maps and contract identity survive.
The CONSTANT `layoutId` remains the real constant-pool index. Constants remain
owned by the actual module; this API does not derive their bits from AST text.
The empty state-map header preserves functionToken, signatureHash and generation.
It has no layoutHash field. Reprocessing the exact NOP form is idempotent.

## Refusals and coverage boundary

Malformed Core ranges retain verifier diagnostics such as `INVALID_RANGE`;
sealed input reports `SEALED`; invalid ownership/arguments report
`INVALID_ARGUMENT`. Source identity/provenance disagreement, actual address
uses, existing frames, GC/deopt/binding metadata, nonempty state pools,
phi/effect/memory metadata and richer semantic graphs are unsupported. Those
records are never silently erased. Allocation and capacity failures retain
their corresponding diagnostics.

The [acceptance record](../acceptance/ssa-dead-source-places.md) records the
behavioral RED and committed pre-extraction focused GREEN: 30/30 cases, comprising
2 source prerequisites, 8 feature cases and 20 guards. The separate prerequisite
run passed 2/2. Build and CTest exited 0 with no observed UBSan diagnostic.
After the selected current shared libraries were rebuilt, configure, focused
relink and CTest again exited 0 with 30/30. The related consumer regression
passed 8/8 CTest entries: 85 Unity cases plus a standalone dominator CFG test.
No UBSan diagnostic was observed in either run. An absolute-path MSVC smoke
compiled only this production TU; WSL enumeration was denied and GCC/Clang
validation remains OPEN. Primitive frame
attachment requires explicit target layout rows and follows this gate. Returned
artifact retention, serialization, canonical AOT/native execution and complete
pass-manager integration remain separate work. Full SSA47 remains **OPEN**.

Network, FFI, external providers, capability and hotpatch entry points are
outside this fixture's execution scope. Its local Core Oracle callback only
reads actual module constants and place metadata.
