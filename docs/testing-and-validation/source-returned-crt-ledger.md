---
related_code:
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/test_ssa_source_returned_crt_ledger.c
  - tests/cmake/ssa-source-returned-crt-ledger.cmake
  - tests/cmake/ssa-source-returned-crt-ledger-direct-validation/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/aot_ir.h
implementation_files:
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
plan_sources:
  - 'Root: 2026-10-10 finite Returned CRT ledger extraction'
  - .codex/plans/20261010-ssa-returned-crt-ledger-extraction.md
tests:
  - tests/parser/test_ssa_source_returned_crt_ledger.c
  - docs/parser-and-semantics/source-returned-crt-ledger.md
  - docs/acceptance/source-returned-crt-ledger.md
doc_type: testing-guide
status: finite-green
---

# Returned CRT ledger finite validation

## Accepted finite state

The tests were authored before public `TryStats` GREEN. Root's actual RED r1 observed the specified sixteen public-feature failures and two guard passes. After Root accepted RED, only the five-line `TryStats` guard/copy implementation changed; actual GREEN r1 passed all eighteen cases. Root also accepted a separate five-unit MSVC compile-only retry. Receipt identities and the unsuccessful first MSVC attempt are retained in the [acceptance record](../acceptance/source-returned-crt-ledger.md). These are finite runtime/compiler gates; Git transactions are tracked separately by Root.

## Exact eighteen-case order

1. `test_empty_ledgers_report_all_six_zero_stats`
2. `test_owner_live_and_freed_stats_use_actual_requests`
3. `test_six_descriptor_sites_use_typed_sizes_and_zeroed_storage`
4. `test_descriptor_fault_site_1`
5. `test_descriptor_fault_site_2`
6. `test_descriptor_fault_site_3`
7. `test_descriptor_fault_site_4`
8. `test_descriptor_fault_site_5`
9. `test_descriptor_fault_site_6`
10. `test_invalid_public_reads_preserve_outputs_stats_and_events`
11. `test_exact_live_lookup_excludes_interior_foreign_freed_and_other_kind`
12. `test_queries_preserve_pending_owner_fault`
13. `test_begin_call_rebases_sites_without_clearing_unarmed_faults`
14. `test_invalid_geometry_adds_errors_without_attempts_or_events`
15. `test_real_256_owner_attempts_reject_the_next_without_an_event`
16. `test_recovery_reports_one_error_per_live_kind_and_releases_records`
17. `test_repeated_recovery_is_idempotent_and_does_not_disarm_fault`
18. `test_null_and_foreign_frees_have_explicit_error_accounting`

| Cases | Accepted actual RED r1 | Accepted actual GREEN r1 |
|---|---|---|
| 1–9, 12–18 | 16 feature FAIL at `RETURNED_CRT_STATS_FEATURE actual TryStats must succeed` | All 16 PASS |
| 10–11 | 2 genuine guard/lookup PASS | Both PASS |
| Unity total | 18 tests, 16 failures, 0 ignored | 18 tests, 0 failures, 0 ignored |

RED contained no setup, purity, event, free-count, cleanup or unexpected error failure; the exact specified feature marker followed those supporting checks. GREEN had no ignored tests, warnings or UBSan diagnostics. A static case list alone supplies no runtime acceptance.

## Storage, observations and real requests

Owner requests use actual small 8/16/24-byte allocations. Descriptor requests use the actual six typed shapes from the [module contract](../parser-and-semantics/source-returned-crt-ledger.md). The fixture registers each returned pointer immediately, before checking bytes or querying observations. It reads actual descriptor allocation bytes as unsigned characters while live and saves only the zeroing error count for later assertions.

The fixture has 257 registered root slots, allowing the actual 256 live owner allocations and immediate registration of the next expected refusal should it unexpectedly return a pointer. It has bounded arrays for 1,024 purity reads, 32 stats reads, 1,024 integer results and 512 event results. These are fixture bounds, not production size promises.

Every public query saves genuine output fields and before/after health copies from the same authoritative ledger. Stats sentinels are six distinct values; event sentinels cover all six named fields. Output comparison is field-by-field, avoiding padding/byte-layout assumptions. Internal Stats is used only for health/purity and cannot substitute for any successful public feature read.

The six injection cases separately acquire the genuine descriptor prefix through the armed site and stop there. They require attempts equal to that site, successes/live equal to site minus one, one failure, zero errors, correct actual event geometry and the sentinel record on the failed attempt. They observe every successful prefix record's one free after cleanup.

Invalid-read guards acquire real allocations, observe existing actual events before and after refusals, and verify outputs and all named counters unchanged. Exact lookup observes actual bases of independent allocations in both kinds, refusing interior/one-past/readable foreign/other-kind addresses and a numeric witness saved before free. It never dereferences a freed address.

The pending-owner test queries genuine existing storage, then makes the real scheduled request to prove reads did not consume it. BeginCall's unarmed rebase test preserves both previously scheduled faults and observes the new local sites. Recovery's fault-preservation case makes a real request after repeated recovery; teardown explicitly disarms independently.

## Cleanup before every Unity assertion

There are no Unity assertions inside allocation/read helpers. `finish` disarms, clears registered root slots before actual frees, captures the original cleanup state, calls recovery, captures the recovered state and only then asserts. `tearDown` repeats the idempotent close and health check after a failed feature assertion. Setup disarms and resets only before resources are acquired.

Original cleanup must already have live zero and successes equal frees. Recovery cannot hide an ordinary leak because original and recovered named counters are compared. Intentional geometry/free/capacity error cases record their exact expected error count; they do not force errors to zero or call an inappropriate zero-error balanced assertion.

Recovery-specific tests explicitly transfer registered roots to the actual ledger recovery before it frees them, then observe one error per affected live kind and exact record frees. Repeated recovery must preserve the prior counters. These intentional errors are feature observations, not accepted leak diagnostics from ordinary cases.

All refusing reads and genuine non-stats observations are asserted before the public `TryStats` feature assertion. Missing positive stats emit `RETURNED_CRT_STATS_FEATURE read=<index>` and fail only after cleanup and supporting checks. Success must return exactly one and match all six expected counters.

## Dedicated build and accepted evidence

Target `zr_vm_ssa_source_returned_crt_ledger_test` selects exactly test main, extracted ledger C and real Unity C. CTest is `ssa_source_returned_crt_ledger`, timeout 180 seconds and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. There is no forced include, linked Core/Parser/VM archive, lifecycle fixture or generated provider.

The direct CMake root is `tests/cmake/ssa-source-returned-crt-ledger-direct-validation`. It requires the actual `E:/Git/zr_vm` checkout and a separate child under `E:/cargo-targets/zr_vm/build`. C11 without extensions, assertions, `/W4 /UNDEBUG` and dynamic MSVC CRT are required where applicable. Clang UBSan with no recovery defaults on; clang-cl sanitizer flags must precede `/link`. Other frontends receive `-Wall -Wextra -UNDEBUG`.

Actual configuration generated source/settings TSV metadata and compile commands. The map name is a basename ending in `.map`. Root accepted the actual configured three-source graph, latest read-header dependencies, imports and sole ledger provider map, selected tool/input/artifact pins, raw ordered results and natural terminal records. Latest read-header counts were ledger184, main191 and Unity29; primary C edges are recorded separately. Ledger undefined symbols included real `__imp_calloc`, `__imp_malloc` and `__imp_free`, memory/security operations and UBSan abort handlers.

Runtime RED r1 used a fresh cache and actually compiled all three C units plus linked. Its seven phases ended naturally: six exit0 and CTest expected exit8. GREEN r1 reused that cache and compiled only the changed ledger plus linked; all seven phases ended naturally at exit0. At Root's runtime review before this documentation update, all 208 selected inputs, seven metadata pins and five artifacts (220 pins) had zero drift. After the update, the remaining 219 compiled-input/tool/metadata/artifact pins remain strictly current; this guide's old SHA256 `DD11CDCF8BDBAD9CC66DB76592A2E70D65CA01B1AFDC110A0D89095F996EB599` is historical protocol metadata. The updated guide is separately frozen with the exact eighteen-case order unchanged. No runtime rerun or C/header/tool change is claimed. This is incremental GREEN evidence, not a clean or full-project build.

The first MSVC attempt compiled test/ledger/Unity but the coordinator failed C1189 because C atomic support was not enabled; the typed header probe was not invoked. The v2 driver added only `/experimental:c11atomics`, matching the actual top-level CMake setting. Retry r2 compiled test, ledger, Unity, coordinator and typed header probe with exit0, zero warnings and no forced include. Actual `/sourceDependencies` JSON Version1.2 reported read-header counts86/79/19/185/18 in that order, present-empty `ProvidedModule` and absent imported-module/header-unit fields. Root checked 255 selected input pins and fifteen raw/object/dependency artifacts with zero drift. This supplies no fixed struct byte-size or MSVC runtime claim.

Shared normal registration and Git transactions are tracked separately by Root. Linux, full MSVC runtime, normal/default Source/full-project configuration and tests, VM lifecycle, finalizer handoff and all47 remain OPEN. No Source/VM/provider/network/FFI/security execution occurred. See the [acceptance record](../acceptance/source-returned-crt-ledger.md).
