---
related_code:
  - tests/cmake/ssa-canonical-type-lookup.cmake
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-sccp-conversion-tests.cmake
  - tests/parser/test_ssa_canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_core/include/zr_vm_core/array.h
implementation_files:
  - tests/parser/test_ssa_canonical_type_lookup.c
plan_sources:
  - .codex/plans/20261009-ssa-canonical-lookup-extraction-v1.md
tests:
  - tests/parser/test_ssa_canonical_type_lookup.c
  - docs/acceptance/canonical-type-lookup.md
doc_type: testing-guide
status: finite-validated-rootproof-commit-pending
---

# Canonical lookup finite validation

## Planned eight-case observations

The physically frozen main registers eight constructed lookup cases. Current native r2
passes all eight cases with0 failures/ignored; the raw output is pinned in acceptance.

| Actual case / observation | Required assertion |
| --- | --- |
| NULL context and invalid ID | NULL result before array access/mutation |
| Empty declared table | NULL with zero live length, including readable spare rows |
| Single declared row | Exact borrowed address; adjacent misses |
| Sparse first/middle/last | Exact addresses in sorted three-row table |
| Gaps/outside/maximum ID | Present UINT32_MAX row and adjacent misses without wraparound |
| Independent equal-ID contexts | Distinct owning node addresses for equal IDs |
| Repeated mixed queries | Sixteen repetitions preserve owning pointer identity |
| Complete const representation | Initialized same-object header/node bytes remain unchanged |

Use independently owned declared sorted canonical arrays and readable initialized
SemanticContext/array headers. No SemanticContext_New, VM/runtime construction,
Parser compile, interner, source programs, Oracle or generated native execution
belongs to this fixture. The test labels constructed lookup precisely; matching
numeric IDs do not prove SCRIPT Binder or canonical registry provenance.

## Lifetime and geometry checks

Only dereference returned nodes while their declaring owner is live and stable.
Do not retain or use a pointer after an owner expires or after reset/reallocation.
Compare exact returned addresses and live logical node/header contents. The
const case additionally compares the initialized same object's representation
before/after reads; this is absence-of-writes evidence, not equality of padding
or spare storage across different owners. Equal numeric IDs in distinct owned tables
must be checked against their own context, not a global registry assumption.

The finite gate obeys Find's existing correctly typed/sorted/live-array
preconditions. Malformed header geometry, duplicate/unsorted IDs, invalid element
widths and dangling pointers are outside this extraction's tested contract.
NULL/invalid-ID behavior is unchanged, not a new validation API. Because Find
allocates nothing, there is no lookup allocation-fault denominator to invent.

## Production and link proof

Independent source review must confirm the moved body and adjacent documentation
preserve binary search and exported signature, and the old TU no longer defines
Find. The actual native map must show one Find provider from lookup.c.obj, with
no index/VM allocator object supplying it or accompanying this finite link.
No test-local replacement or forced-success lookup is permitted.

Root owns the fresh small-direct CMake/native UBSan gate, complete current
input/header/def/inc/toolchain/object/settings pins and natural phase exits.
Existing committed Core support is a historical prerequisite; no Core source
changed and it was not rerun in this lookup gate. Actual raw default output and diagnostics are required before a
PASS claim. Zero current warnings, enabled assertions/UBSan and current map pins
cannot be inferred from older source/helper reports.

Current MSVC r2 four-unit checks compile moved lookup, retained index, test main and
a tiny public-signature probe. They are compile-only and must report real child
PIDs, natural exits, zero warnings and raw-log/object hashes. They do not prove
ordinary full Parser linkage or runtime behavior. Root's exact four normal
source-list edits require separate review while preserving foreign working state.

## Open boundaries

Current native r2 has8PASS/0/0, selected3 TUs with three unchanged current objects. Runtime r2 only relinks
the new map; historical r1 performed3 compiles+link. Configured
project44 TUs is a distinct count. Current MSVC r2 four-unit gate is compile-only0/zero warnings.
Root independent current proof and finite commit remain pending. No successful source Binder, Source wrapper GREEN, registry retention,
canonical interning, frame validation/finalized-module admission, default
canonical output, full normal/Linux/MSVC/native parity or all47 acceptance is
claimed. The later const frame validator has its own diagnostics and leases.

- [Lookup contract](../parser-and-semantics/canonical-type-lookup.md)
- [Evidence ledger](../acceptance/canonical-type-lookup.md)
