---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - tests/parser/test_ssa_source_callable_query.c
  - tests/parser/support/ssa_source_callable_query_shim.h
  - tests/cmake/ssa-source-callable-query.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c
plan_sources:
  - .codex/plans/20261009-ssa-source-callable-query-design-v1.md
  - .codex/plans/20261009-ssa-source-callable-query-execution-v1.md
tests:
  - tests/parser/test_ssa_source_callable_query.c
  - docs/acceptance/source-callable-query.md
doc_type: module-detail
status: functional-green-verified-commit-pending
---

# Const Source callable field query

## Current delivery boundary

The private query now implements a current declared canonical noargs/no-receiver/
no-effects primitive INT64 field predicate. It validates the five traversed spans,
sorted IDs, active union kinds, callable/return hashes and profile, then publishes
five actual numeric facts once. It owns and retains nothing, performs no heap
allocation/free, and preserves output on refusal. Exact nine-field diagnostics
and optional independent diagnostic behavior are implemented.

Actual GREEN r1 passes18 cases with exact marker
`SOURCE_CALLABLE_ZERO_ALLOC cases=18 attempts=0 frees=0`; lower lookup8 passes.
MSVC GREEN r1 passes four compile-only units, natural0/zero warnings. The
zero-counter observer covers only query/lookup FI, supplemented by actual object
import/provider evidence. It is not a CRT-internals audit or allocation fault sweep.
RED was committed as `27ebf3195f547889ec0cd0a64bd0aa5ffaf7d77f`; historical RED/
failed gates remain in acceptance. Current GREEN Root proof/commit are pending.

## API, result and lifetime

`source_callable_query(context, callableTypeId, functionToken, output, diagnostic)`
uses actual SemanticContext, TypeId, MetadataToken and ExecIR diagnostic types.
The private facts result holds callableTypeId, returnTypeId, signatureHash,
returnStructuralHash and returnPrimitive. These are actual numeric fields only;
there are no borrowed pointers or owners. Supplied functionToken is an opaque
diagnostic scalar, including0, and is never bound or authenticated.

Context and traversed storage must be initialized, correctly aligned, genuinely
readable, live, stable and externally serialized. Output is live writable storage.
Diagnostic may be NULL; otherwise it is independent writable storage disjoint
from output, context and all context-owned allocations. Growth/Reset/Free or
concurrent mutation is forbidden during the call. Correct numeric spans cannot
establish arbitrary-address readability or allocator ownership.

The query never dereferences context.state, symbols or unselected nested union
storage. A declared typed context without runtime state is valid for this scope.
Output/diagnostic independence from untraversed context allocations is a caller
precondition, not an exhaustive ownership check. Overlapping diagnostic violates
that precondition; writing its refusal cannot preserve aliased input bytes.

The implementation constructs a zeroed local candidate, assigns its five numeric fields after
all checks and publishes once. Failure preserves output and input content,
pointers/capacities. Candidate padding is not identity evidence. Success returns
all nine diagnostic fields zero. Query retains no storage pointer after return.

## Exact five spans and union discipline

Register context header, output header, optional diagnostic header, canonicalTypes
allocation by capacity and selected FUNCTION parameterContracts allocation by
capacity. Empty NULL/zero-capacity storage adds no span. Canonical array header
is embedded in context; parameter header is embedded in a preflighted node.
Neither is a separate independent span. Primitive return has no traversed pool.

Array checks are isValid, actual elementSize, length<=capacity, checked
capacity*sizeof(element), NULL/nonzero-capacity or nonNULL/zero-capacity mismatch,
uintptr end, prior-span overlap, then registration. Overflow/inventory exhaustion
is CAPACITY_OVERFLOW; malformed shape/alias is INVALID_RANGE. Half-open capacity
spans cover spare storage without reading it. Empty valid parameter storage with
real spare allocation is accepted, matching actual InternFunction behavior.

Check every live canonical ID for nonzero strictly increasing order before Find.
Use previous/current IDs directly, never previous+1 at UINT32_MAX. Check callable
kind FUNCTION before reading its function union; preflight parameter storage
before arity/receiver/effects. Check return kind PRIMITIVE before primitive union.
No unselected node kind/hash/nested payload is validated.

## Complete implemented first-error diagnostics

Nine fields: code/functionToken/blockId/instructionId/sourceId/expectedVersion/
actualVersion/expectedHash/actualHash. Locations always0. T is supplied token;
V is UInt32 expected/actualVersion; H is UInt64 expected/actualHash. Every refusal
resets all fields and populates only those listed. TZrSize geometry is never
truncated into V. Existing IDs/enums fit V; actual structural hashes use H.

| Order | Predicate | Code; active payload |
| --- | --- | --- |
|1|NULL context/output|INVALID_ARGUMENT; token0; V/H0|
|2|callable ID0|SIGNATURE_MISMATCH; T; V1/0|
|3|context/output/diag header arithmetic, then overlap in registration order|CAPACITY_OVERFLOW or INVALID_RANGE; T; V/H0|
|4|canonical validity/width/count-cap, product, pointer-cap, address/overlap|INVALID_RANGE or CAPACITY_OVERFLOW by exact storage order; T; V/H0|
|5a|live canonical ID0|SIGNATURE_MISMATCH; T; V1/0|
|5b|current ID<=previous ID|SIGNATURE_MISMATCH; T; Vprevious/current|
|6a|callable Find absent|SIGNATURE_MISMATCH; T; Vrequested/0|
|6b|defensive callable ID contradiction|SIGNATURE_MISMATCH; T; Vrequested/found|
|6c|callable kind not FUNCTION|SIGNATURE_MISMATCH; T; VFUNCTION/actualkind|
|6d|callable hash0|SIGNATURE_MISMATCH; T; H1/0|
|7|parameter validity/width/count-cap, product, pointer-cap, address/overlap|INVALID_RANGE or CAPACITY_OVERFLOW by exact storage order; T; V/H0|
|8a,b,c|arity nonzero, receiver not NONE, effects not NONE|UNSUPPORTED; T; V/H0|
|9|return ID0|SIGNATURE_MISMATCH; T; V1/0|
|10a|return Find absent|SIGNATURE_MISMATCH; T; VreturnID/0|
|10b|defensive return ID contradiction|SIGNATURE_MISMATCH; T; VreturnID/found|
|10c|return kind not PRIMITIVE|SIGNATURE_MISMATCH; T; VPRIMITIVE/actualkind|
|10d|return hash0|SIGNATURE_MISMATCH; T; H1/0|
|11|primitive not INT64|UNSUPPORTED; T; V/H0|
|12|success|All9 zero; publish numeric candidate|

The design freezes storage traversal rather than globally prioritizing error codes.
Core's diagnostic version/location/token fields are32 bits, hashes64 bits.
Defensive found-ID contradictions are unreachable with valid sorted storage and
actual Find; no fake provider is used to claim branch coverage. Arity/receiver/
effects share identical diagnostic payloads, so tests cannot distinguish their
relative order solely by those outcomes.

## Minimal closure and limits

Implemented callees are actual allocation-free canonical Find and inline
Array_Get; no index/interner/semantic lifecycle/compiler/AOT/VM allocator TU is
required. Actual runtime FI observations and imports/providers establish the finite
zero malloc/calloc/realloc/free query/lookup closure. There is no fault sweep
denominator. Hashes are read, not computed or authenticated. Nonzero return hash
is an additive query predicate, not a modification of Binder's return admission.

Declared fixture IDs/hashes prove current typed field consistency only. Source/
Binder provenance, registry retention, target ABI, semantic/source admission,
finalizer integration, normal Parser/default output, Linux/full MSVC and all47
remain OPEN. A future finalizer's real constant-pool witnesses are separate work.

- [Exact eighteen-case protocol](../testing-and-validation/source-callable-query.md)
- [Pending finite evidence](../acceptance/source-callable-query.md)
