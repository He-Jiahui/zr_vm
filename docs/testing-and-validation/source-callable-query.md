---
related_code:
  - tests/parser/test_ssa_source_callable_query.c
  - tests/parser/support/ssa_source_callable_query_shim.h
  - tests/cmake/ssa-source-callable-query.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
implementation_files:
  - tests/parser/test_ssa_source_callable_query.c
  - tests/parser/support/ssa_source_callable_query_shim.h
plan_sources:
  - .codex/plans/20261009-ssa-source-callable-query-design-v1.md
  - .codex/plans/20261009-ssa-source-callable-query-execution-v1.md
tests:
  - tests/parser/test_ssa_source_callable_query.c
  - docs/acceptance/source-callable-query.md
doc_type: testing-guide
status: functional-red
---

# Source callable query finite protocol

## Current state and actual input profile

The accepted test plan uses declared sorted typed canonical tables, actual node
union tags and independently owned parameter storage. It requires no runtime
SemanticContext lifecycle or interning. IDs/hashes remain declared field
witnesses rather than Source/Binder-authenticated identity. Actual RED r4 observed case2 PASS/17 exact feature FAIL/0 ignored with17 full9
UNSUPPORTED baseline markers and attempts0/frees0. It gives no later variant or
GREEN marker credit. Current MSVC RED r5 passed4 compile-only units; exact receipts
and physical freezes are in acceptance, final Root proof/RED commit pending.

## Exact eighteen names and order

1. `test_current_declared_callable_returns_exact_numeric_facts`
2. `test_null_arguments_reset_all_nine_fields`
3. `test_null_diagnostic_preserves_result_and_refusals`
4. `test_zero_callable_id_precedes_storage_checks`
5. `test_canonical_header_validity_width_and_capacity_are_checked`
6. `test_canonical_pointer_capacity_and_checked_spans_are_checked`
7. `test_output_overlap_is_refused_without_publication`
8. `test_canonical_ids_are_nonzero_strictly_sorted`
9. `test_empty_sparse_and_missing_callable_tables_refuse_exactly`
10. `test_callable_kind_and_hash_precede_parameter_union_reads`
11. `test_parameter_header_and_allocation_geometry_precede_profile`
12. `test_noargs_receiver_and_effect_profile_order_is_exact`
13. `test_zero_missing_and_wrong_kind_return_nodes_refuse_exactly`
14. `test_return_hash_precedes_primitive_profile`
15. `test_non_i64_return_is_unsupported`
16. `test_live_spare_parameter_storage_and_maximum_ids_are_supported`
17. `test_independent_equal_ids_and_repeat_calls_preserve_inputs`
18. `test_paired_errors_and_every_failure_preserve_output`

Case2 is NULL guards only. Every other case first calls the admitted declared
baseline. RED refusal must check all9 UNSUPPORTED28/T/other0 fields, unchanged
output/inputs and zero actual allocation/free counters before printing
`SOURCE_CALLABLE_FEATURE code=28 attempts=0 frees=0` and failing with exact text
`actual const source callable query must succeed`. Actual RED r4 observed1 guard PASS/17 feature
FAIL; later GREEN predicates remain unexecuted. No later variants are
credited when the baseline fails.

## Storage and diagnostic test discipline

Every variant checks all9 fields including inactive payloads, poisoned independent
output preservation and unchanged live input headers/owners/content. Success
checks five actual result fields; padding is not equality identity. Restore
mutations before assertion/cleanup. Token is an opaque supplied scalar, not
authenticated through a provider. IDs/enums use32-bit V and hashes64-bit H;
TZrSize capacity/product errors have zero payload instead of truncation.

Cases6/11 use genuinely readable malformed headers for arithmetic overflow;
reject before purported storage traversal. Numeric pointer-end probes must never
dereference fabricated addresses or claim arbitrary-pointer validation. Case7
uses explicitly aligned union storage with independent diagnostic; do not poison
an overlapping output and thereby corrupt input shape before the call. Preserve
overlapping bytes when rejecting output/input overlap. Overlapping diagnostic is
outside the independent writable caller contract and is not a preservation test.

Empty canonical storage is geometrically valid but lacks a callable. Empty
parameter length with real spare allocation is valid; no spare bytes are read.
Maximum-ID sort checks use actual previous/current IDs without overflow. Wrong
FUNCTION or PRIMITIVE kind must refuse before inactive union access. Actual Find
ID contradictions cannot be reached under valid sorted geometry; do not mock
Find. Parameter geometry precedes profile refusal even for nonzero arity.

Arity/receiver/effects are tested separately and each paired with a later distinct
return refusal. Their identical UNSUPPORTED payloads do not independently prove
their internal relative order. No source symbol/AST/hash authenticity is inferred.

## Zero-allocation observer and actual provider closure

Force-include the new shim only in actual query and lookup TUs. Main/Unity are
unredirected. CRT callbacks reside in main, count malloc/calloc/realloc/free and
delegate to ordinary CRT, with no dynamic bookkeeping, assertions or nonlocal
exits. Reset counters only when no query runs. Every test requires zero attempts
and frees from this instrumented closure; ordinary CRT activity elsewhere is
outside that observation.

GREEN emits `SOURCE_CALLABLE_ZERO_ALLOC cases=18 attempts=0 frees=0` only after
all18 completed successfully. An import/provider/map proof is still required;
the marker alone is insufficient. Zero attempts give no allocation fault sweep
denominator or completed fault marker. Frame18/Core88/owner61 counts are unrelated.

## Actual finite RED native/MSVC gates

Root owns `ssa_source_callable_query` registration and native execution. Intended
runtime selected membership is query+lookup+main+Unity, two production FI TUs;
actual configuration/provider/object evidence must establish these counts.
No semantic lifecycle/index/interner/Compiler/VM/AOT/provider object is linked.
Actual RED r4 selected4/FI2/providers2 within configured55, with assertions/UBSan
and strict object import/provider gates accepted. Lower lookup8 PASS. r4 build compiles4 affected TUs and links2 executables; r1 built4 TUs+link. No full rebuild or GREEN scope is credited.

Root-executed compile-only MSVC driver at
`E:/cargo-targets/zr_vm/tmp/ssa-source-callable-query-msvc-v4.ps1` compiles query,
lookup, main and fresh tiny private declaration probe, in that order. Query/lookup
use the new shim; main/probe do not. Four /c /TC C11 /UNDEBUG /MD /W4 compilations
must naturally exit0 with zero warnings, actual selected source/header/tool/script pre/post
pins and log/object hashes. It links/runs nothing and arms no faults. Actual `callable-query-msvc-RED-r5-receipt.json` records four natural exit0/
zero warnings with189 selected current input pins and4304 historical baseline rows.
Actual C JSON Source/Includes and explicit absent module fields pin dependencies.
This is MSVC r5, paired with runtime r4, and
establishes compile-only compatibility, not query GREEN behavior.
Runtime broad5657 header inventory is historical gate-time evidence. Current
proof checks selected4 and lookup-support3 actual Ninja transitive dependencies,
all nonheader/tool/source/CMake/metadata/raw/object/U/map gates and MSVC JSON.
Real type_system stays selected; Compiler private headers are genuinely absent.
The fresh final Root proof remains unexecuted until this documentation freeze.

Normal/full Parser/Linux/full MSVC/default output, genuine Source/Binder/interner,
registry lifetime, canonical hash authenticity, finalizer and all47 remain OPEN.

- [API, five spans and complete diagnostics](../parser-and-semantics/source-callable-query.md)
- [Pending actual acceptance](../acceptance/source-callable-query.md)
