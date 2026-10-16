---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/parser/test_ssa_source_finalized_module.c
  - tests/parser/support/ssa_source_finalized_module_fixture.h
  - tests/parser/support/ssa_source_finalized_module_fixture.c
  - tests/parser/support/ssa_source_finalized_module_witness.c
  - tests/parser/support/ssa_source_finalized_module_faults.c
  - tests/cmake/ssa-source-finalized-module.cmake
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - .codex/plans/20261009-ssa-source-finalized-module-design-v3.md
  - .codex/plans/20261009-ssa-source-finalized-module-test-protocol-v1.md
tests:
  - tests/parser/test_ssa_source_finalized_module.c
  - docs/testing-and-validation/source-finalized-module.md
doc_type: validation-protocol
status: child-null-zero-clang-ubsan-green-msvc-compile-verified
---

# Finalized Source module: finite validation

## Observed finite GREEN and preserved RED baseline

Actual GREEN r2 observed all24 ordered cases PASS/0 FAIL/0 ignored. Case23 emitted
`SOURCE_FINALIZED_LIFETIME cycles=16 declared_storage_cleared=16`; case24 emitted
`SOURCE_FINALIZED_FAULT_SWEEP measured=89 completed=89 original_verify=12 finalized_frame=18 whole_copy=17 candidate_verify=12 candidate_frame=18 snapshot=12`.
These are actual raw markers, not inherited lower denominators or89 Unity cases.
The predicates, complete-copy ownership, exact lower9 refusals, every measured
failure/retry and declared-storage lifetime below were reached in this finite run.
Genuine Source preparation and readable compiler storage>UINT32_MAX remain OPEN.

Historical RED r3/r4 observed guard1 PASS/23 feature FAIL/0 ignored and23 exact
`SOURCE_FINALIZED_FEATURE code=28 attempts=0 frees=0` markers. That baseline
proved healthy full9 refusal/preservation and cleanup, not later variants.
RED was committed as `1f9488b9c4065c54980e3841bc1a41279bdebb3a`; GREEN Root proof
and finite commit are currently pending. Failed attempts remain in the evidence record.

This fixture owns declared canonical/symbol/AST/SemIR/SourceIdentity witnesses
and two independent actual SZrTypeValue pools. It constructs owning ExecIR storage
through real Core APIs/returned IDs, verifies it and uses actual Clone/Attach/
const frame validation prerequisites. Parser Build/Binder/compactor preparation
is excluded: imports Semantic_FindSymbolById/SemanticIr_Validate and their genuine
provider/lifetime closure remain a separate OPEN Source gate. No fake provider,
whole mutable Semantic TU, VM constructor, SourceExecute or Oracle is allowed.

## Exact24 names and order

1. `test_null_arguments_reset_all_nine_fields`
2. `test_finalized_module_publishes_complete_independent_owned_output`
3. `test_same_function_and_partial_owner_aliases_refuse_before_traversal`
4. `test_native_pool_and_owner_headers_refuse_invalid_geometry`
5. `test_typed_headers_wide_counts_and_real_spare_storage_are_checked`
6. `test_module_and_entry_contract_domains_are_preserved`
7. `test_source_identity_schema_ids_signature_and_profile_are_checked`
8. `test_live_state_unique_symbol_and_script_origin_agree`
9. `test_origin_ranges_are_valid_contained_and_lossless`
10. `test_semantic_cfg_place_region_and_empty_profile_are_checked`
11. `test_semantic_roles_values_facts_and_operand_bounds_agree`
12. `test_keyed_source_maps_counts_ids_and_ranges_agree`
13. `test_dual_constant_counts_and_index_are_checked_before_reads`
14. `test_constant_canonical_types_flags_and_pool_tags_agree`
15. `test_negative_minimum_and_mutated_dual_pool_bits_are_checked`
16. `test_only_dead_place_to_nop_transition_is_accepted`
17. `test_retained_value_address_and_provenance_relationship_is_exact`
18. `test_present_empty_maps_and_sealed_finalized_lifetime_are_preserved`
19. `test_actual_frame_failure_preserves_every_lower_diagnostic_field`
20. `test_actual_core_failure_precedes_later_source_refusals`
21. `test_complete_candidate_copy_preserves_logical_pools_and_real_capacities`
22. `test_null_diagnostic_and_distinct_paired_error_priority_are_exact`
23. `test_old_output_borrow_and_post_source_release_lifetime_are_safe`
24. `test_every_measured_finalizer_allocation_failure_preserves_owners_and_retries`

Cases2-24 first require one successful admitted baseline through actual lower
prerequisites. Feature refusal cannot credit later mutations/refusal variants.
Required NULL guards are case1; NULL Function constants with positive count is
pool shape in case13; optional NULL diagnostic is case22.

## Case8 child NULL/zero extension: finite Clang/UBSan GREEN

Case8 now additionally checks four separate refusals: SCRIPT statements header
NULL, statements count0 with the genuine nodes allocation/capacity retained,
sole RETURN child NULL, and RETURN expression NULL. A well-formed empty statement
array and the general language's bare return are outside this finite single
INTEGER-return profile; neither broadens the finalizer's admission contract.
Each refusal requires UNSUPPORTED, the actual selected Function token and all
seven inactive diagnostic fields zero. NULL nodes with count1/positive capacity
instead belongs to the earlier INVALID_RANGE storage guard and is not this
count0 check.

Existing REFUSE restores each field before any Unity assertion. Fm_Observe and
Fm_Refused compare input bytes, the unchanged old output and live allocation
records, including release of any temporary allocations. Each restored input
then runs Fm_Feature again, requiring successful publication, complete diagnostic
clearing and exact old-owner release. VerifyModule runs before AST admission,
so these refusals do not require zero allocation attempts.

The first current Clang/UBSan build failed before runtime: the fixture header's
existing `tests/core/...` include could not resolve because the standalone test
target lacked the actual repository root include directory. Its CMake target now
adds that root privately; existing source-path migrations and strict flags are
preserved. The failed build receipt/raw logs remain unchanged under
`E:/cargo-targets/zr_vm/reports/ssa-lower-child-boundaries-01a0fe2b/`.
The corrected build and actual execution subsequently passed as recorded below.
The existing24 test names/order and output protocol remain unchanged. The current
Clang/UBSan test was recompiled and linked in r3 (build31508, exit0); direct
children5596 and31920 naturally exited0 with all24 ordered PASS, zero failures
and ignored cases. The strict r4 raw output is3229B stdout/223B stderr, containing
exact lifetime16 and independently measured/completed89 fault markers. No UBSan
or ledger diagnostic occurred. The current executable SHA256 is
`562C37331C4ED2EAFB619A176F591781DE093C54452A903B93E335A38C742E2D`.

The r6 completion receipt rechecks r3 compilation/configuration and r4 raw runtime
identities, then records31 actual MSVC14.44 compile-only units: all naturally
exit0, warning0 and only the exact expected source-basename output. It checks50
strict source/tool/config inputs,31 Clang objects and273 actual consumed header
identities before/after;63 products are the31 Clang objects,31 MSVC objects and
runtime executable. Reused production objects were actually compiled in this
batch's earlier build phases; this is no claim of an atomic checkout snapshot or
fresh full rebuild in r6. No MSVC link/runtime or whole-project W0 is credited.

Completion receipt:
`E:/cargo-targets/zr_vm/reports/ssa-lower-child-boundaries-01a0fe2b/r6/receipt.json`
SHA256 `3F3BE7307F88F407496975FDF29EC8C0FF2B970AE0687CAD71C35B8A2B1DDDA1`.
Earlier failed receipts remain false: missing include root, omitted actual .def
header admission, a footer regex matching normal Ignored text, MSVC macro quote
escaping and Windows dependency-path case comparison. Those verifier failures
do not change the actual successful compile/runtime phases; their raw logs are
retained. This adds declared lower-fixture coverage only, with no new genuine
Source22, VM, FFI, network, Linux/full MSVC or all47 credit.

## Fixture modules and observations

Main orchestrates product mutations. New fixture C owns construction and teardown;
witness C owns typed equality/full9 checks; faults C owns independent measurement/
direct lower witnesses/all-site retries. Reuse generic strict allocator/shim and
logical Capture/Equal/Diagnostic by reference, without calling generic Build/
SetUp/TearDown or copying its implementation.

Register every partial Module/Function/output/scratch before Unity assertions.
Restore malformed headers/mutations and disarm fault injection before assertions.
Allocator callbacks/armed calls/observers never Unity-longjmp. Capture known owned
initialized buffers rather than dereference mutated headers. Generic logical
4096-word witness requires exact preflighted measured budget; overflow is visible,
not truncated equality. Padding/capacity/spare bytes are not semantic identity.

Cases3-5 exercise genuine typed geometry, actual initialized spare capacity and
aligned overlap probes. Same selected/finalized header has UNSUPPORTED/T before
second traversal; partial/shared owners have INVALID_RANGE. Product/end overflow
uses readable malformed headers, not claimed giant allocations. Actual readable
storage with compiler length>UINT32_MAX remains unexecuted; a forged length alone
would not establish that wide-storage branch.

Cases6-15 prove current versions/domains, explicit Source identity, unique symbol/
AST origin, finite Semantic roles/inert fields/types/bounds and complete source
maps M. Dedicated count/type/index/map refusals occur at their actual first
comparison. Pool bits use zero,-1,INT64_MIN and9/8 declared data; never execute
new Source programs or read AST integer bits as producer truth.

Cases16-18 compare live operand/result slices, retained value/address/provenance
relationship, all block fields and sole PLACE_BASE→NOP transition. Present-empty
StateMap and sealed finalized are genuine lower storage boundaries. No borrowed
descendant Free or clone-capacity==count requirement is valid.

Cases19-20 require direct same-input real frame/Core FALSE witnesses before
claiming lower precedence, comparing all9 after restore. Do not force a legal
Core profile into a manufactured production failure. Case22 pairs distinguishable
earlier/later errors and checks optional NULL diagnostic independently. Identical
payloads cannot prove their internal relative order.

Case23 borrows finalized from an actual prior successful old output until last
publication. Record IDs/frees witness old release; no stale pointer comparisons.
Two live owners have independent reachable addresses. Release allocated source storage and clear declared inline context/
AST/pool witnesses, then inspect stored logical arrays/bits/IDs without canonical lookup.
Clearing these inline fixtures is not genuine context/interner destruction.
Actual GREEN case23 completed16 construct/finalize/free cycles with healthy
ledger boundaries and the literal lifetime marker above; historical RED did not reach them.

## Historical RED feature observation

Before intended feature failure, compare complete9, unchanged input/old output
contents and records, allocation/free delta0 and healthy cleanup. Print exactly
`SOURCE_FINALIZED_FEATURE code=28 attempts=0 frees=0` once per actual reached
baseline refusal, then fail with
`actual finalized primitive source module snapshot must succeed`.
No feature marker for unrelated prerequisite/ledger failure and no inverted
assertions, ignored cases or guessed actual result.

## Independently measured case24 sweep

Require unarmed finalizer success first. Measure original VerifyModule, finalized
const frame validation, CloneModuleReplacingFunction, candidate Verify/frame and
CloneOwnedModule separately on actual inputs/candidate scratch. Measure complete
finalizer N and ordered request geometry independently. Query/scans zero allocation
are not fault sites. Historical Core88/frame18/generic61 are not this denominator.

For every relative site1..N, obtain same-phase direct FALSE/full9/failed event
kind/count/width/bytes; clean its registered partial output before fallback.
Eager requests may continue after failed ordinal, so scan actual failed events.
Arm only finalizer interval; immediately Disarm, preserve its exact diagnostic,
prove candidate cleanup and old/live inputs unchanged before recovery. Retry
disarmed with same live inputs must publish independent complete output with
zero9 and exact old record release. Healthy teardown precedes Reset.

Check remaining32768-record/65536-event budgets before arming from actual direct/
full/scratch/prerequisite/retry traces. A quadratic bound is valid only if proved
to cover all work; do not hardcap/reset/silently omit sites. Completion marker
`SOURCE_FINALIZED_FAULT_SWEEP measured=N completed=N` appears only after every
fault and retry, with measured phase counts. Wrapper does not invent OOM; envelope
V1/0/token0 and every other lower9 come from actual same-phase calls.

Cache original zero-health/exact frees before recovery. Print
`SOURCE_FINALIZED_LEDGER` only for real errors/live residue, then recover for
longjmp safety and assert original/current health. Fallback cannot mask leaks.

## Native closure and platform gates

GREEN r2 establishes selected30 translation units/FI23, support36 and configured66.
Groups are Core15+modulecopy1+frame3+finalizer/query/lookup4+test/support7. No residual
archive or unrelated Compiler/VM/Semantic provider is imported. Query/lookup use
the generic owner shim. Build r2 compiled only the two production C files and
linked in three steps with zero warnings;66 current objects are admitted by
actual selected dependencies/hash evidence, not a claimed clean rebuild.

Six independent lower gates passed: CoreModel (28 Function/60 Module clone fault
loops), generic owner14/two61, modulecopy20/88, frame18/18, query18/zeroallocation
and lookup8. Finalizer89 was independently measured in six actual phases and is
not a sum or reuse of those lower historical denominators.

MSVC v2 GREEN r2 observed31 compile-only units:23 real providers with generic FI,
seven unredirected test/support units and one fresh tiny declaration probe.
All31 children naturally exited0, zero warnings and exact raw basename profiles.
Actual C Version1.2 JSON has Source/ProvidedModule(empty)/Includes; optional import
fields are absent, not observed empty arrays. Selected current pins240 and each
JSON/raw/object/tool/script were checked. Broad prebaseline is historical inventory.
This gives no MSVC runtime, fault execution, whole-project or full MSVC credit.
Exact raw/receipt hashes and17/31 phases appear in the acceptance document.

The nonasserting TryRecordFrees getter and deferred numeric observers allow
cleanup after Unity marks a test failed. Teardown records original health,
releases registered storage before assertions, then reports any real residue;
recovery cannot turn failed health into accepted evidence. The historical RED r2
aborting assertion path and its31 live records remain failed history.

Genuine Source/Binder preparation, registry lifetime, normal/default pipeline,
Linux/full MSVC/backend parity and all47 remain OPEN.

- [Contract](../parser-and-semantics/source-finalized-module.md)
- [Evidence](../acceptance/source-finalized-module.md)
