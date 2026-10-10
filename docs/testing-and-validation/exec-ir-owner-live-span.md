---
related_code:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/core/test_ssa_exec_ir_owner_live_span.c
  - tests/unity_config.h
  - tests/cmake/ssa-owner-live-span.cmake
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-owner-live-span-direct-validation/CMakeLists.txt
implementation_files:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - 'Root: 2026-10-09 finite test-ledger live-span query contract'
  - .codex/plans/20261009-ssa-exec-owner-live-span-protocol-v1.md
tests:
  - tests/core/test_ssa_exec_ir_owner_live_span.c
  - docs/testing-and-validation/exec-ir-owner-live-span.md
  - docs/acceptance/exec-ir-owner-live-span.md
doc_type: testing-guide
status: finite-green
---

# ExecIR test-ledger live-span validation

## Current state

Status is `finite-green`. Actual functional RED r3 observed eight required feature failures and four guard passes; GREEN r1 observed twelve PASS, zero failures and zero ignored cases through the dedicated Clang UBSan CTest route. Separate MSVC GREEN r1 accepted four compile-only units. Evidence paths and pins are in the [acceptance record](../acceptance/exec-ir-owner-live-span.md). The module is included by the normal shared registration; normal full-project configuration/runtime remains unexercised by this finite child. Root's session tracks the Git transaction separately from these native runtime/compiler receipts. No complete allocation denominator or platform parity is inferred.

## Exact finite case order

1. `test_malloc_exact_live_span_reports_record_allocation_and_bytes`
2. `test_calloc_readable_subspans_include_last_byte`
3. `test_realloc_null_creates_real_live_record`
4. `test_successful_realloc_uses_current_pointer_and_size`
5. `test_released_record_is_not_a_live_span`
6. `test_valid_half_open_windows_select_exact_record`
7. `test_empty_reversed_and_out_of_inventory_windows_preserve_output`
8. `test_null_and_zero_query_arguments_preserve_output`
9. `test_multiplication_and_address_overflow_preserve_output`
10. `test_readable_foreign_and_outside_spans_preserve_output`
11. `test_independent_live_allocations_are_not_cross_matched`
12. `test_armed_queries_preserve_stats_events_and_pending_fault`

The names above are the actual source and receipt order. Their finite results are:

| Case numbers | RED r3 | GREEN r1 |
|---|---|---|
| 1–6, 11–12 | FAIL at `OWNER_LIVE_SPAN_FEATURE actual live span query must succeed` | PASS |
| 7–10 | PASS | PASS |
| Unity total | 12 tests, 8 failures, 0 ignored | 12 tests, 0 failures, 0 ignored |
| CTest exit | 8, expected functional RED | 0 |

Both accepted runtime receipts have seven actual phases: configure, build, latest dependency inspection, allocator defined symbols, allocator undefined symbols, main undefined symbols, and CTest. RED's first six phases exit 0; GREEN's seven phases exit 0. The runtime logs contain no UBSan, ledger, unknown-free, resource, or warning diagnostic. Rejected RED r1/r2 stopped before CTest and supply no behavioral results.

## Real storage and observations

The fixture creates small genuine malloc/calloc/realloc allocations through the shared strict allocator. Exact-base, interior, last-byte, one-past, growth/shrink, record-window, released-record, overflow, and independent-allocation queries use these actual records. Refusing output starts with explicit sentinel fields; each observation copies those fields and counters into bounded fixture storage.

The query output's pointer becomes a numeric witness while its allocation is live. Released-record checks use a previously saved integer and never dereference the reconstructed address. Successful realloc checks use only the returned current pointer. No constructed ledger row, fake owner, guessed typed buffer, source execution, or provider mock is added.

Before and after each query, the fixture saves all six allocator statistics and asserts their equality after cleanup. The armed test schedules attempt 3 before acquiring resources, creates two actual allocations, queries them, then makes the real third request and verifies the original pending fault still fires. It inspects actual existing event fields after cleanup. GREEN r1 passes this observation. This is one finite pending-fault observation, not a sweep denominator or general allocation-failure coverage.

The fixture owns at most four root slots and stores at most 32 query observations. These are finite fixture bounds, not API or production capacity promises. Genuine heap requests include 16/24/32/48/8-byte profiles; none establishes the byte size of `SZrOwnerLiveSpan`.

## Cleanup precedes Unity assertions

`finish` closes owners before health and semantic assertions. `close_owners` disarms fault injection, clears each root slot before freeing its actual allocation, records the original ledger state, then calls recovery and records the resulting state. Repeated teardown is idempotent through the fixture's closed flag.

The original ledger must already be balanced before recovery: live 0, errors 0, records equal frees. Recovery is fallback cleanup and cannot hide a leak because both saved states are compared. Query assertions use stored numeric output fields after allocation cleanup. `tearDown` repeats the idempotent close and health check, so an assertion failure cannot leave tracked roots live for the next case. Reset occurs in setup before resources are acquired.

## Actual build route and evidence

`zr_vm_ssa_exec_ir_owner_live_span_test` has exactly three actual selected repository translation units: test main, allocator, Unity. CTest is `ssa_exec_ir_owner_live_span`, timeout 180 seconds, with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The observed target has no forced include, linked Core/Parser/VM archive, or Source fixture. The link map identifies the actual allocator object as `ZrTests_ExecOwner_TryLiveSpan`'s provider; its undefined symbols include the real CRT malloc/calloc/realloc/free imports.

The standalone CMake root is `tests/cmake/ssa-owner-live-span-direct-validation`. The accepted route used the actual E checkout and `E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-owner-live-span`. The optional map name must be a basename ending in `.map`. Actual generated source/settings metadata, compile commands, tool and selected input pins, latest VALID Ninja dependencies, objects, executable and map are recorded in each accepted receipt. No source copies or snapshots were created.

C11 and assertions are required. The actual runtime used Clang 19.1.5 `clang-cl`, Ninja Debug, `/W4 /UNDEBUG`, dynamic CRT, and UBSan with no recovery; sanitizer driver flags precede `/link`. The target also supplies `-Wall -Wextra -UNDEBUG` for other compiler frontends, which this finite Windows run does not validate. RED r1 built three objects plus the executable; RED r2/r3 only linked incrementally. GREEN r1 recompiled main and allocator and linked, reusing Unity's object. These step counts do not establish a clean GREEN full rebuild.

The actual Ninja rule uses `deps = msvc` and `/showIncludes`: the primary C source comes from the configured object/source edge, while the latest VALID dependency record supplies read headers. RED r1's gate incorrectly expected the primary source in header dependencies. RED r2's broad diagnostic scan incorrectly treated the COFF `R` symbol containing `EXEC_OWNER_LEDGER` as runtime output. Both attempts are preserved as rejected evidence; RED r3 corrected those evidence checks before accepting the expected functional failures.

The separate MSVC GREEN r1 receipt records four natural compile exits 0, zero warnings, no forced include, and one expected source-basename line in each raw log. Units are actual main, allocator, Unity, and an authored tiny typed declaration probe; no selected repository source is embedded or copied into the probe. Actual `/sourceDependencies` JSON Version 1.2 identifies each primary source and respectively 26/31/19/7 read headers. `ProvidedModule` is present as an empty string; `ImportedModules` and `ImportedHeaderUnits` are absent. The actual selected dependency closure and artifacts had zero drift under Root's review. This check links and executes nothing and does not measure struct byte layout or provide runtime, Linux, 32-bit, or full MSVC acceptance.

Root accepted the runtime and MSVC receipts after reading their actual commands, raw output, current pins, artifacts and natural terminal states. Root added the target module include to `tests/cmake/ssa-tests.cmake`; the finite child did not configure or execute the normal full project.

## Gaps retained

All twelve cases use finite real storage. Synthetic high integer addresses exercise numeric refusal without reads. They do not prove pointer authenticity, arbitrary-width geometry, every spare byte, duplicate-record ambiguity, 32-bit behavior, global allocator saturation, or a complete OOM sweep.

Source/Binder input provenance, typed owner admission, canonical rules, interner/registry lifetimes, source/default adoption, normal full suite, all47, Linux and platform parity remain OPEN. See the [module contract](../core-runtime/exec-ir-owner-live-span.md) and [acceptance record](../acceptance/exec-ir-owner-live-span.md).
