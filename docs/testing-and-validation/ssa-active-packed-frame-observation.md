---
related_code:
  - tests/parser/test_ssa_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
  - tests/parser/test_ssa_active_packed_frame_observation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/execution_contract.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-active-packed-frame-observation.cmake
  - tests/cmake/ssa-active-packed-frame-observation-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-active-packed-frame-observation-check.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/parser/test_ssa_active_packed_frame_observation.c
  - tests/parser/test_ssa_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
  - docs/testing-and-validation/ssa-active-packed-frame-observation.md
  - docs/acceptance/ssa-active-packed-frame-observation.md
doc_type: testing-guide
status: finite-validated
---

# Active packed-frame observation validation

## Current state

Finite validation is accepted for the implemented primitive: functional RED, explicit-GREEN runtime with both legacy regressions, and MSVC compile-only checks. The sixteen-case names/order and numeric protocol below were observed in the accepted raw streams. The RED orchestration receipt remains false; its postterminal Root audit separately accepts the actual functional result. See the acceptance record for exact evidence and history.

## Direct route and settings

The [direct entry](../../tests/cmake/ssa-active-packed-frame-observation-direct-validation/CMakeLists.txt) uses the actual E:/Git/zr_vm checkout and a separate child under E:/cargo-targets/zr_vm/build. It requires Windows, Ninja, Debug and a compiler with the MSVC frontend. The compiler ABI probe is a real executable; a STATIC_LIBRARY probe is refused. Root must supply and authenticate a fresh build/cache/map and exact toolchain before execution.

Target: `zr_vm_ssa_active_packed_frame_observation_test`. CTest: `ssa_active_packed_frame_observation`. Exact eighteen TUs are runner, Unity, Parser roots/layout and the fourteen Core sources in the accepted plan. No broad archives, FI or fake providers are selected. Normal definitions are ZR_LIBRARY_TYPE_STATIC=1, ZR_DEBUG=1, ZR_PLATFORM_WIN=1, ZR_PLATFORM_WIN_USE_MSVC=1, _CRT_SECURE_NO_WARNINGS, UNITY_INCLUDE_CONFIG_H and UNITY_USE_FLUSH_STDOUT. The CRT definition follows existing project policy; it does not waive unexpected diagnostics.

C11/extensionsOFF, /MD, /W4 and /UNDEBUG are explicit. Clang UBSan has no recovery; its driver flags must precede /link. CTest sets all four profiling flags0. The checker also removes output/trace environment routes before launching the real child. The accepted runtime used these settings with explicit GREEN; the later single-literal default change did not rerun the gate or the normal project. Shared registration is now present.

| Setting / output | Name |
| --- | --- |
| Actual source root | ZR_SSA_ACTIVE_PACKED_FRAME_SOURCE_ROOT |
| Expected raw phase | ZR_SSA_ACTIVE_PACKED_FRAME_EXPECTED_PHASE; current default GREEN |
| UBSan | ZR_SSA_ACTIVE_PACKED_FRAME_UBSAN; direct default ON |
| Map basename | ZR_SSA_ACTIVE_PACKED_FRAME_MAP_NAME |
| Default map | ssa-active-packed-frame-observation-link.map |
| Source metadata | active-packed-frame-observation-target-sources.tsv |
| Settings metadata | active-packed-frame-observation-settings.tsv |
| Interface header baseline | active-packed-frame-observation-header-hashes.tsv; eight headers, not the complete selected dependency closure |
| Raw stdout | active-packed-frame-observation-${phase}-stdout.log |
| Raw stderr | active-packed-frame-observation-${phase}-stderr.log |

Metadata records actual source hashes and target/tool settings, including OFF/empty values. It contains no repository source bodies. Latest actual dependencies, maps and sole provider definitions must be verified independently by Root's admitted driver.

## Separately selected legacy targets

The direct route also configures two private executables from the unchanged original tests. Both were selected, freshly built and run once in accepted GREEN2. The RED child selected only the focused target; configuration alone grants no build/runtime credit.

| Target / CTest | Explicit source order | TU occurrences |
| --- | --- | --- |
| zr_vm_ssa_active_frame_legacy_layout_test / ssa_active_frame_legacy_layout | original test_ssa_frame_layout.c, layoutC, fourteen Core providers | 16 |
| zr_vm_ssa_active_frame_legacy_roots_test / ssa_active_frame_legacy_roots | original test_ssa_roots_observation.c, rootsC, layoutC, fourteen Core providers | 17 |

Both use C11/MD/W4/UNDEBUG and the same Clang UBSan policy. They have five normal static/debug/platform/CRT definitions, no Unity definitions and only Common/Core/Parser include roots. The actual old mains use assert/CHECK. The layout child normally has empty stdout; the roots child prints `parser roots observation PASS (including reused scalar conflicts)`. Each CTest requires an actual exit0. Maps are `${target}-link.map`. The combined TSV distinguishes all three configured targets; an actual RED driver must admit only the focused eighteen compiled TUs, while GREEN selection of all three gives51 separately compiled TU occurrences and three links. The accepted GREEN2 build followed this exact graph, with51 VALID latest dependency records and three executable links.

## Test responsibilities

The real builder uses A=10 with [0,2), B=20 with [2,5), and C=30 with [0,5), eight-byte scalar payloads. A/B share storage; C is independent. Tests derive offsets from actual returned slots. Structural negatives mutate typed copies of genuine metadata using readable live backing storage and independent diagnostics.

Coverage includes selected A/B reuse; reversed B/C invalidation order; readonly and empty selection; widths1/2/4/8; real mixed-width reuse with full physical-width mirrors; ID/refusal diagnostics; equal and unequal active occupant conflict; insufficient capacities; complete inactive-layout geometry; unsupported active classes with valid scalar-only mixed selection; writable alias refusal; repeated layout lifetime; unchanged legacy behavior; actual prior Unity failure and a subsequent saved-witness check.

The names, numerical CASE rows and FINAL fields below match the frozen runner and accepted raw streams. All allocations/layouts are registered immediately and drained before assertions. TearDown finishes API observation and cleanup before reporting. Its numerical saved witnesses distinguish support health from a feature result.

## Protocol admission

The checker must preserve the actual Unity nonzero exit, exact failure reasons, zero ignored cases, each ordered CASE witness and the final drain. Expected failures cannot admit arbitrary FAIL, crash, timeout, sanitizer or warning output. CTest wrapper0 means the precisely expected child was recognized, not that the child itself exited0.

Raw streams are saved from the real execute_process result. Existing logs are refused before file(WRITE); Root supplies a fresh single-writer child. This is not an OS exclusive-create guarantee. Child timeout is120 seconds and CTest timeout180.

## Exact accepted protocol

Actual functional RED had fifteen FAIL and one PASS: case14 is the genuine legacy PASS; case15 is the deliberate `ACTIVE_PACKED_FRAME_PRIOR_FAILURE`; all other RED failures require exactly `ACTIVE_PACKED_FRAME_MISSING_FEATURE`. Accepted GREEN had fifteen PASS and the same sole deliberate case15 failure. Actual child exit must be15 for RED or1 for GREEN, zero ignored and stderr exactly empty. Both raw streams were independently checked; the failed RED controller is not promoted to a passing receipt.

| Case | Exact RUN_TEST name | Builders / acquired / freed | Calls | GREEN accepted | GREEN observed / invalidations |
| --- | --- | --- | --- | --- | --- |
| 1 | test_active_reused_scalar_selection | 1 | 2 | 2 | 2 |
| 2 | test_active_selection_order | 1 | 1 | 1 | 2 |
| 3 | test_active_readonly_selection | 1 | 2 | 2 | 2 |
| 4 | test_active_empty_selection | 2 | 3 | 3 | 0 |
| 5 | test_active_scalar_widths | 4 | 4 | 4 | 4 |
| 6 | test_active_mixed_width_reuse | 1 | 1 | 1 | 1 |
| 7 | test_active_value_id_refusal | 1 | 5 | 0 | 0 |
| 8 | test_active_conflict_refusal | 1 | 2 | 0 | 0 |
| 9 | test_active_capacity_refusal | 1 | 11 | 0 | 0 |
| 10 | test_active_layout_geometry_refusal | 1 | 20 | 0 | 0 |
| 11 | test_active_unsupported_and_mixed_layout | 5 | 5 | 1 | 1 |
| 12 | test_active_alias_refusal | 1 | 6 | 0 | 0 |
| 13 | test_active_repeated_layout_lifetime | 2 | 4098 | 4097 | 4097 |
| 14 | test_active_legacy_contract | 1 | 0 | 0 | 0 |
| 15 | test_active_prior_unity_failure | 1 | 1 | 1 | 1 |
| 16 | test_active_prior_failure_witness | 0 | 0 | 0 | 0 |

Each CASE contains sixteen named numeric fields, in exact order:

```text
ACTIVE_PACKED_FRAME_CASE case support feature drained builders calls accepted observed invalidations layout_preserved output_checked acquired freed prior_failed prior_ignored flags_preserved
```

Common support, drained, layout_preserved, output_checked and flags_preserved are1; prior_ignored is0. Only case15 has prior_failed1. RED feature is1 only for case14, with accepted/observed/invalidations0 in every case. GREEN feature is1 throughout; the table gives its exact counters. Each CASE follows its own actual Unity result after UnityConclude. Case16 performs no new API call/allocation; it checks saved case15 bytes/result/flags after release. Case14 active API calls are0 because it checks the separate unchanged legacy API.

The final line has seven numeric fields, observed identically in both phases:

```text
ACTIVE_PACKED_FRAME_FINAL drain=1 live=0 acquired=24 freed=24 prior_failed=0 prior_ignored=0 flags_preserved=1
```

The checker requires36 nonempty lines: sixteen alternating Unity/CASE pairs, FINAL, the actual23-dash Unity separator, `16 Tests 15 Failures 0 Ignored` (RED) or `16 Tests 1 Failures 0 Ignored` (GREEN), then FAIL. Each Unity summary has one trailing ASCII space, which exact admission must retain. `ACTIVE_PACKED_FRAME_CHILD` retains the actual15/1 exit; only complete exact admission emits `ACTIVE_PACKED_FRAME_PROTOCOL_ACCEPTED`. Wrapper exit0 cannot substitute child exit0. Registered layout-owner counts cover24 real builder lifetimes, including an empty layout; they do not count every internal malloc or prove full heap-leak absence.

## Limits

GREEN2 completed configure/build/latest-deps/defined-nm/undefined-nm/CTest naturally with exit0, fresh51 C compiles and three links, warnings0/UBSan0. The focused child exited1 with15 PASS and the sole exact deliberate prior-failure; its wrapper and both legacy CTests exited0. Totals were4161 API calls,4112 accepted calls,4110 observed values/invalidations and24 registered owners acquired/freed. The two MSVC compile-only children exited0 with warnings0 and real sourceDependencies1.2 (79/86 included headers). Neither MSVC linking nor execution was performed. Linux remains OPEN. This finite primitive cannot establish genuine program liveness, VM frame authenticity, Source behavior, moving-GC correctness, debugger resume, complete OOM, complete heap-leak absence, performance or any all47 milestone completion.

The receipts pin the original three guide versions reviewed before this documentation update. Those guide hashes are historical protocol metadata, not current unchanged documentation; the sixteen-case protocol remains unchanged. The later descriptor default change is likewise a separate historical bridge to the same explicit-GREEN settings, not a new runtime result. Root separately tracks registration/Git transactions.

See the [module contract](../parser-and-semantics/ssa-active-packed-frame-observation.md) and [acceptance record](../acceptance/ssa-active-packed-frame-observation.md).
