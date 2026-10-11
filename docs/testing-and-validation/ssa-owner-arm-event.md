---
related_code:
  - tests/core/test_ssa_owner_arm_event.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-owner-arm-event.cmake
  - tests/cmake/ssa-owner-arm-event-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-owner-arm-event-check.cmake
implementation_files:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - .codex/plans/20261010-ssa-nonasserting-owner-observers.md
  - 'Root: 2026-10-10 finite nonasserting Arm/Event primitive gate'
tests:
  - tests/core/test_ssa_owner_arm_event.c
  - tests/cmake/ssa-owner-arm-event-check.cmake
  - docs/testing-and-validation/ssa-owner-arm-event.md
  - docs/acceptance/ssa-owner-arm-event.md
doc_type: testing-guide
status: finite-support-accepted
---

# Owner Arm/Event finite validation

## Accepted finite gate

Root accepted actual functional RED, fresh default GREEN and three-unit MSVC compilation on 2026-10-10. RED produced eight exact failures/four passes; GREEN produced one deliberate failure/eleven passes. Both had zero ignored cases and checker exit0, while their actual Unity child exits remained8/1. The main has no phase argument. After accepted RED, Root authorized the minimal provider implementation and changed only the target default from RED to GREEN. The actual fresh GREEN configure omitted the phase override and its cache/settings recorded GREEN. Linux and full-project validation remain OPEN.

See the [module contract](../parser-and-semantics/ssa-owner-arm-event.md) and [acceptance boundary](../acceptance/ssa-owner-arm-event.md). The actual main stayed 416 physical lines/20575 bytes, SHA256 `8CA522546CD63B67F86A32EB7DC455306E8599C5E629D6CA704C31772C5EE7FD`, across RED, GREEN and MSVC. It uses the real allocator header declarations. Current providers guard invalid arguments, schedule the existing fault and copy the actual event; original asserting wrappers and unrelated allocator bytes are preserved by the reviewed whole-memory inverses.

## Descriptor and route

Target `zr_vm_ssa_owner_arm_event_test` links exactly, in order:

1. `tests/core/test_ssa_owner_arm_event.c`
2. `tests/core/support/ssa_exec_ir_owned_module_allocator.c`
3. `tests/third_party/zr_unity/Unity/src/unity.c`

CTest `ssa_owner_arm_event` invokes the actual CMake checker, without WILL_FAIL. Its timeout is 180 seconds; the actual Unity subprocess timeout is 120 seconds. No Source, VM, Core archive, fixture provider, forced include or stored source copy is selected.

The actual direct runs used E:/Git/zr_vm, fresh separate children beneath E:/cargo-targets/zr_vm/build, Windows/Ninja/Debug and Clang's MSVC frontend. The executable compiler ABI probe was retained. Actual C11/extensionsOFF, /MD, /W4 and /UNDEBUG settings were verified. The standard Windows CRT definition is _CRT_SECURE_NO_WARNINGS; actual Unity configuration and flushing are enabled. No /wd or -Wno bypass was used. Clang UBSan ran without recovery, with sanitizer driver flags before /link. Both runs freshly compiled all three units and linked, with zero warnings or sanitizer diagnostics.

| Cache/input | Role |
| --- | --- |
| ZR_SSA_OWNER_ARM_EVENT_SOURCE_ROOT | Actual checkout supplied by direct descriptor |
| ZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE | Current GREEN default; historical RED explicit; only RED/GREEN admitted |
| ZR_SSA_OWNER_ARM_EVENT_UBSAN | Direct Clang UBSan default ON |
| ZR_SSA_OWNER_ARM_EVENT_MAP_NAME | Optional validated .map basename in build child |
| ZR_ARM_EVENT_EXECUTABLE / EXPECTED_PHASE / OUTPUT_DIR | Actual checker inputs |

The direct target emitted `owner-arm-event-target-sources.tsv` with actual source hashes and `owner-arm-event-settings.tsv` with compiler/toolchain/link rule and target settings. Accepted receipts validate the actual three-source inventory, latest selected header dependencies, definitions/imports, fourteen sole-provider rows, link map and /MD CRT linkage. TSV declarations alone give no runtime or provider credit.

## Exact twelve-case order

| # | Exact RUN_TEST name | RED result | GREEN result |
| --- | --- | --- | --- |
| 1 | test_arm_first_malloc | MISSING_FEATURE | PASS |
| 2 | test_arm_second_calloc | MISSING_FEATURE | PASS |
| 3 | test_malloc_event_copy | MISSING_FEATURE | PASS |
| 4 | test_calloc_event_copy | MISSING_FEATURE | PASS |
| 5 | test_failed_realloc_event | MISSING_FEATURE | PASS |
| 6 | test_event_query_keeps_armed_fault | MISSING_FEATURE | PASS |
| 7 | test_arm_zero_refusal | PASS | PASS |
| 8 | test_arm_uintmax_refusal | PASS | PASS |
| 9 | test_second_arm_refusal | PASS | PASS |
| 10 | test_event_invalid_preserves_output | PASS | PASS |
| 11 | test_prior_unity_failure | PRIOR_FAILURE | PRIOR_FAILURE |
| 12 | test_prior_failure_witness | MISSING_FEATURE | PASS |

Each failure must have the exact reason prefix `OWNER_ARM_EVENT_`: MISSING_FEATURE or PRIOR_FAILURE as listed, without another reason or an extra failure. RED expects actual Unity exit8 and 12 Tests/8 Failures/0 Ignored. GREEN expects exit1 and 12 Tests/1 Failure/0 Ignored. Both Unity footers are FAIL. Wrapper acceptance is exit0 only after the entire raw protocol matches; it cannot replace the actual child exit.

## Eighteen-field CASE table

Each Unity result must be immediately followed by its exact `OWNER_ARM_EVENT_CASE` row. Fields are case, support, feature, drained, arm, event, next_event, attempts, records, live, frees, errors, failures, queries, record_frees, prior_failed, prior_ignored, flags_preserved. Common values are support/drained/flags_preserved1 and live/errors/prior_ignored0. prior_failed is 1 only in case11. Frees, queries and record_frees each equal records.

| Case | Attempts | Records RED/GREEN | Failures RED/GREEN | Feature RED/GREEN | Arm RED/GREEN | Event RED/GREEN | Next event RED/GREEN |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 1/0 | 0/1 | 0/1 | 0/1 | 0/0 | 0/0 |
| 2 | 2 | 2/1 | 0/1 | 0/1 | 0/1 | 0/0 | 0/0 |
| 3 | 1 | 1/1 | 0/0 | 0/1 | 0/0 | 0/1 | 0/0 |
| 4 | 1 | 1/1 | 0/0 | 0/1 | 0/0 | 0/1 | 0/0 |
| 5 | 2 | 1/1 | 1/1 | 0/1 | 0/0 | 0/1 | 0/0 |
| 6 | 2 | 1/1 | 1/1 | 0/1 | 0/0 | 0/1 | 0/0 |
| 7 | 1 | 1/1 | 0/0 | 1/1 | 0/0 | 0/0 | 0/0 |
| 8 | 2 | 2/2 | 0/0 | 1/1 | 0/0 | 0/0 | 0/0 |
| 9 | 1 | 0/0 | 1/1 | 1/1 | 0/0 | 0/0 | 0/0 |
| 10 | 2 | 1/1 | 1/1 | 1/1 | 0/0 | 0/0 | 0/0 |
| 11 | 2 | 2/1 | 0/1 | 0/1 | 0/1 | 0/1 | 0/1 |
| 12 | 2 | 2/1 | 0/1 | 0/1 | 0/1 | 0/1 | 0/1 |

The checker uses explicit RED/GREEN eight-column tables and named `table_expected` parameters to generate all eighteen fields. Actual RED and default GREEN raw rows matched every table field. No source code switches behavior based on phase.

## Final witness and raw logs

After twelve result/CASE pairs, exactly one `OWNER_ARM_EVENT_FINAL` has eight fields: drain1, live0, errors0, records RED2/GREEN1, frees/queries/record_frees equal records, and flags_preserved1. Then the actual 23-dash Unity separator, exact summary and FAIL footer appear once in order. Unknown stdout, duplicates, reordered rows, IGNORE, wrong fields/reasons/exit, timeout, crash, warning, sanitizer or ledger diagnostics refuse acceptance. Stderr must be exactly empty.

The checker refuses either pre-existing `owner-arm-event-<PHASE>-stdout.log` or `-stderr.log`, executes the actual child, then saves both captured streams before interpretation. CMake file(WRITE) is not an OS exclusive-create primitive; Root supplied fresh unique build children and one writer. The accepted Root driver applied its separately reviewed CreateNew policy to receipts and phase logs.

Checker telemetry is `OWNER_ARM_EVENT_CHILD phase=<PHASE> actual_exit=<actual>` and, only after full acceptance, `OWNER_ARM_EVENT_PROTOCOL_ACCEPTED phase=<PHASE> cases=12 expected_failures=<8-or-1> ignored=0 actual_exit=<actual>`.

## Cleanup and limits

Ordinary cases disarm/free before feature assertions. Case11's nonasserting teardown saves actual pre-free query success/free0, performs real cleanup, and saves post-free success/free1 while preserving failed/ignored flags. Case12 independently verifies those values after Unity conclusion, without a health reset. No Recover launders errors. Final drain precedes UNITY_END and contains no Unity assertion.

The exact private 65536-event exhaustion boundary, real ignored-path execution, complete OOM/leak instrumentation, TryCapture, Source's twenty cases, genuine finalizer integration, VM/Function/network/FFI, full47, full normal/default project, Linux and MSVC runtime remain OPEN.

## Actual receipt and process record

Evidence base: E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b. All three receipts are validated true with no error. Root reported the runtime controllers naturally exited0 and closed: RED PID41648/session84428; GREEN PID164/session22891. MSVC PID17148 naturally exited0 in immediate exec chunk20bd2a; no unified session was allocated. No PID or terminal identity is inferred from a compile result.

| Route | Receipt basename and SHA256 | Actual result |
| --- | --- | --- |
| RED r1 | owner-arm-event-red-r1-receipt.json — D320E7E6CA066A9B4B90FA38C6A6ED84A9F3E7BBA52FBD7196797723B81699F6 | Six natural phases0; child8/wrapper0; 4PASS/8FAIL/0ignored |
| Default GREEN r1 | owner-arm-event-green-r1-receipt.json — 922FA62B6CEAB894060F8FB42A172FD1F2E7AEAC772C44B76B859BE7353524F9 | Six natural phases0; child1/wrapper0; 11PASS/1intentionalFAIL/0ignored |
| MSVC GREEN r1 | owner-arm-event-msvc-GREEN-r1-receipt.json — 92BCAFDB914FB7C4551C654D9E015EB5F1377CA62E00CAFAC55482A513564AEC | Three natural compile-only phases0; warnings0; no link/runtime |

The six runtime phases are configure/build/latest-deps/all-defined/all-undefined/ctest. Actual RED child PIDs were27068/44020/13704/20812/39108/38700; GREEN35072/40260/36340/15112/10548/13684. Both fresh builds performed three C compilations plus one link. Actual MSVC main/allocator/Unity PIDs28812/42492/42600 all exited0, with basename-only raw output, /std:c11, /experimental:c11atomics, /MD, /W4, /UNDEBUG and no forced include/archive. JSON1.2 header counts were26/31/19; ProvidedModule was present and empty, while ImportedModules and ImportedHeaderUnits were actually absent. There is no fixed ABI-byte claim.

Raw stdout hashes are RED `6FE2197C1F60D2D744E3556BDBA46935E05E2F804A5C6EB926C63871E9DBB738` and GREEN `21A7F59450F54B8E9C4CA4B6F5B1814BB893760143E05D108FC010F7D21ACAA6`. Both stderr files are zero bytes, SHA256 `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855`. Receipt artifact/metadata rows pin the real executable, objects, map, build/cache/dependency data and raw phase logs; the acceptance record identifies key hashes.

## Historical documentation pins

These guides were updated after Root's runtime/compiler review; this edit is not a rerun. All three receipts froze the previous parser/testing/acceptance guides at `E1F23E2605BFB5C9878E7A018CAD3298AAFCB98FB92D6612642D622047E55340`, `CE7A1F2F6269CB8BA9A15C479DA2724812CC256B1A7BA4FD0273D49BB2B94037` and `E6B5C21A4C1F4B63A9B86EAEB9E6D37247F552936CD8CCD9E8B17D0B30C1627B`. Root's final commit freeze must bridge these three historical guides for each receipt: nine distinct receipt/path rows. The RED allocator H/C/default transition needs three additional exact bridges. Every other source/header/tool/log/artifact remains subject to current strict hashes; no claim is made that the old whole input lists remain unchanged after the documentation edit. Shared registration and Git transaction remain separately tracked by Root.
