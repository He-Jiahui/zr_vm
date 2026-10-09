---
related_code:
  - tests/cmake/ssa-canonical-type-lookup.cmake
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-sccp-conversion-tests.cmake
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_core/include/zr_vm_core/array.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
plan_sources:
  - .codex/plans/20261009-ssa-canonical-lookup-extraction-v1.md
tests:
  - tests/parser/test_ssa_canonical_type_lookup.c
  - docs/acceptance/canonical-type-lookup.md
doc_type: module-detail
status: finite-validated-rootproof-commit-pending
---

# Canonical type borrowed lookup

## Purpose and extraction boundary

The authorized extraction moves the existing `ZrParser_CanonicalType_Find`
definition and adjacent documentation from canonical_type_index.c into a small
canonical_type_lookup.c. The public declaration and algorithm remain unchanged.
This lets a finite consumer link the actual pure lookup without importing the
mutable hash-index and VM allocation machinery in the old translation unit.
The index continues to own mutation/index responsibilities.

Production index/lookup and eight-case main are physically frozen and read-only
reviewed. Retained index now199 lines: only its final empty line was removed;
remaining prefix and moved Find body/comment are unchanged. Current native r2 observes8PASS/0FAIL/0ignored; MSVC r2 observes four
natural exits0/zero warnings, compile-only. Root current proof and finite commit
remain pending. This document describes the current established Find contract and
the behavior-preserving extraction and its observed finite gate. Full normal
Parser/source/runtime integration is outside that evidence.
No validation, allocation, interning, hash generation or new exported API is
introduced by this move.

## Actual lookup behavior

Find accepts a borrowed const SemanticContext and a semantic type ID. NULL
context or `ZR_SEMANTIC_ID_INVALID` returns NULL before accessing the array.
Otherwise the existing lower-bound binary search reads canonicalTypes.length,
searches `[low, high)`, and uses `low + (high-low)/2` for its midpoint. It returns
the actual node address only if the located node's ID equals the requested ID;
absent IDs and an empty table return NULL. Sparse numeric IDs are valid lookup
keys when the owning table satisfies its sorted-ID contract.

`ZrCore_Array_Get` is an inline borrowed accessor. Its index bound is asserted
in debug builds; it computes an address using the array's recorded element
size. The const cast at this existing callsite does not itself mutate storage.
Find writes neither context headers nor nodes and performs no allocation/free.
It returns no diagnostic object; a miss is NULL, not a newly classified error.

## Caller preconditions and borrowed lifetime

Apart from the explicit NULL/invalid-ID guards, context and its canonicalTypes
header/storage must be genuinely initialized, readable, correctly typed and
stable throughout lookup. The array's elementSize must describe actual
SZrCanonicalTypeNode rows, live count must fit its owned storage/capacity, and
element address arithmetic must be valid. Live node IDs are sorted unique
nonzero IDs, as produced by the interner's monotonic append order. No concurrent
mutation, reallocation or context teardown may race with this call.

Find does not inspect isValid, validate capacity/element width/overflow, sort
IDs or repair malformed arrays. These remain caller requirements; the extraction
does not create arbitrary-pointer safety or a new public validation layer.
Constructed lookup tests use declared live node arrays and initialized readable
headers. They must not invoke Find on malformed storage outside this contract.

The returned pointer is owned by that context's array. Subsequent interning may
reallocate canonicalTypes; Reset/Free invalidates it. Callers must finish reads
before any such operation and reacquire lookup afterward when appropriate.
Two independent contexts may contain equal numeric IDs and different nodes;
the ID alone does not establish registry identity. No pointer or registry
lifetime is retained or extended by Find.

## Registration and evidence boundary

Normal Parser recursive discovery includes the new C file. Root also owns the
four explicit source-list additions: three in ssa-tests.cmake and one in
ssa-sccp-conversion-tests.cmake. Mixed working files require exact owned patches,
preserving foreign changes. This documentation does not claim those edits or
ordinary full Parser linkage have already been validated.

The planned native lookup gate uses only the actual lookup object, constructed
node fixture, assertions and CRT; the sole Find map provider must be the moved
object, with no mutable index or VM allocator objects in this finite link.
Actual fresh small-direct lookup gate and finite MSVC four-unit compile-only
checks are observed; existing committed Core support is a historical prerequisite
and was not rerun for this extraction with no Core changes. No Source wrapper/Binder, canonical interner, registry retention,
default adoption, Oracle or SSA leaf completion follows from this extraction.

- [Finite lookup tests](../testing-and-validation/canonical-type-lookup.md)
- [Finite evidence](../acceptance/canonical-type-lookup.md)
