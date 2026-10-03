---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.h
  - tests/parser/test_ssa_owned_row_direct_call_graph.c
  - tests/cmake/ssa-owned-row-direct-call-graph-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.h
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/02-static-binding-facts.md
tests:
  - tests/parser/test_ssa_owned_row_direct_call_graph.c
  - tests/parser/test_ssa_interprocedural_inlining.c
  - tests/acceptance/ssa-owned-row-direct-call-graph.md
doc_type: module
---

# Owned-row local DIRECT call graph

## Scope and entry points

`ZrParser_ExecIr_BuildCallGraph` consumes Core-owned binding rows for a finite
same-module scalar DIRECT subset. This is metadata analysis: it neither
executes a call nor invokes a module, native, plugin or AOT provider.
The public build and validate interfaces remain in `exec_ir_interprocedural.h`.
The private `exec_ir_call_target` helper holds legacy target selection, typed
selection, scalar edge rederivation and typed call-site correspondence.

The build first validates the module and each function's structure/SSA. A
malformed Core row still fails this preflight with its existing location and
diagnostic. The old graph survives unchanged. A valid row outside the admitted
subset produces a successful conservative UNKNOWN edge.

## Admitted owned row

The caller must use `ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED`. Its instruction's
one-based `bindingRow` is resolved exclusively through
`ZrCore_ExecIr_FunctionBindingRowAt(caller, reference)`; numeric compact hints,
`layoutId`, `typeToken` and a coincidentally equal FunctionId are ignored.
Typed-empty CALL/INVOKE remains unresolved and still owns a graph edge.

The row must be DIRECT/CALL with a nonzero MemberDef RID. Relocation is NONE,
targetIndex is SLOT_NONE, ownerDepth/flags are zero, ownerTypeToken and layout
version/hash are zero, and dispatchSlot is SLOT_NONE. The row, caller contract
and module carry the same nonzero module identity. The unique callee contract
must carry that module identity, a nonzero generation, and a nonzero signature
equal to both row.signatureHash and callee.signatureHash.

The effective publication token is `contract.targetToken` when present and
`functionToken` otherwise. All effective matches are counted before signature
filtering. Two publications are ambiguous even when their signatures differ.
An obsolete functionToken is not a second published key when targetToken is
present. `signatureToken` remains Core's validated metadata shape; ExecIR has
no separate callee signature token to compare here.

## Edge and summary semantics

A admitted edge publishes DIRECT/resolved, the actual callee id, effective
token, signature and generation, exactReceiver=true, guarded=false. Publication
is frozen only for a sealed callee with nonzero generation. An unsealed callee
with a valid generation resolves as patchable and retains conservative effects.
Existing SCC and effect propagation consume the resolved edge, including typed
recursion and imported native effects. A summary can conservatively widen
nativeEffectsUnknown=true; validation requires true for unresolved typed rows
and a nonpatchable callee proven to import native unknown effects.

Unsupported valid shapes publish zero callee/token/signature/generation,
UNKNOWN/unresolved and nativeEffectsUnknown=true. They include MemberRef,
missing/ambiguous token, cross-module identities, signature disagreement,
generation zero, owner-layout contracts, virtual/interface/typed-function,
GET/SET/META and CONSTANT/MODULE/AOT/VM_MODULE relocation. No external pool or
receiver proof is invented. Every typed edge has inlineEligible=false and
inlineReason=UNSUPPORTED. Typed inlining and devirtualization continue to reject
the module without changing its rows or graph. Oracle typed execution remains
UNSUPPORTED. EIS6 metadata preservation is unchanged; legacy artifact versions
do not gain typed-row support.

## Public validation

With a module supplied, `ZrParser_ExecIr_CallGraphValidate` proves Core shape,
caller/instruction ranges and CALL/INVOKE opcode before reading a typed row.
It reruns the private selection and compares callee id, resolved/kind, complete
target identity, exact/guard/patchable classification and unsupported inlining.
Zero identities are compared for unresolved and typed-empty rows too.
This check applies with zero, retained or recomputed graphHash; a checksum is
not a substitute for the consumer contract. A mismatch reports TARGET_MISMATCH
at the caller's exact instruction/block/source with expected and actual values.

Every typed CALL/INVOKE must correspond to exactly one edge. Present typed site
keys are collected after range checks, with checked count and allocation size,
then sorted lexicographically without subtracting unsigned ids. Duplicates and
missing sites report TARGET_MISMATCH at their actual/expected site. Empty typed
tables participate in the same correspondence. Scratch allocation is skipped
when no typed edge exists; allocation failure reports OUT_OF_MEMORY. All
allocated scratch is freed once and neither graph nor module is modified.
Legacy sites retain their existing validation and target resolution behavior.

## Build and tests

Product parser discovers the helper through `zr_vm_common/CommonMacros.cmake`.
The new test fragment supplies the helper to the existing manually listed
interprocedural test and registers `ssa_owned_row_direct_call_graph`. The parent
includes it directly after `ssa-tests.cmake` in the serialized DIRECT integration
epoch. The integration proceeds independently of the DIV work.

The standalone fixture uses real Core module construction, typed row setters,
full `VerifyModule(ALL)` and public graph APIs. It covers CALL/INVOKE, canonical
publication ambiguity, patchability, valid conservative forms, malformed
preflight with old-graph preservation, semantic rebuild hashes, recursion,
native effect import, typed rewrite rejection, forged tuples and omitted or
duplicated typed sites. Forgery cases cover zero, retained and recomputed graph
checksums and assert precise caller/instruction/block/source diagnostics.
The unchanged interprocedural test retains its fourteen legacy groups.

## Current production validation

The adopted current E source epoch passed fresh MSVC compilation of 38 original-E
translation-unit declarations and registered CTest 2/2, including 59 owned-row
cases and all fourteen unchanged interprocedural groups. GCC and Clang each
freshly compiled nineteen real Core/parser/fixture TUs from the 68-input
source/header snapshot verified against current E, then passed all 59 cases
under ASan/UBSan in actual Linux ELF executables. Native Windows LLD used actual
GNU/Clang Linux SDK plans; the receipts bind compile/link/runtime PIDs and argv,
dependencies, SDK/object/binary hashes and raw logs. Source guards stayed stable.

Root independently repeated the registered 2/2 tests and both sanitizer ELF
fixtures, with an explicit Linux D temporary directory for its Linux repeats.
The original source-validation drivers' inherited Linux temporary-directory
scope remains recorded in their historical receipts. Root verified the actual
parent CMake include directly after `ssa-tests.cmake` and its registration in
the serialized DIRECT integration epoch. DIRECT integration proceeds
independently of the DIV work.

The formal current-production receipt is `D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/direct-current-e-formal/receipt.json`,
SHA256 `241784cc81d2e2538de1390b2cb9ac86947270802c90ef42bf6f57ad18374cc0`, bound to root-verified main HEAD `c8c793da5d5868e2f42aeb4812e00e13c0fab9c6`.
It binds the original source gates, actual root repeats and parent registration
receipts. Earlier D baseline RED, private candidate passes and setup failures
remain historical evidence.

Typed execution, external/indirect resolution, inlining and devirtualization
remain outside this local scalar graph contract. Validation uses local metadata
fixtures without provider callbacks, plugin loads or network/security routes.
