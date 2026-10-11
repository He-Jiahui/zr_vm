---
related_code:
  - tests/core/test_ssa_owner_try_capture.c
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-owner-try-capture.cmake
  - tests/cmake/ssa-owner-try-capture-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-owner-try-capture-check.cmake
  - tests/cmake/ssa-execir-owned-module.cmake
  - tests/cmake/ssa-execir-module-copy.cmake
  - tests/cmake/ssa-function-execir-owner.cmake
  - tests/cmake/ssa-source-finalized-module.cmake
  - tests/cmake/ssa-source-returned-execir-retention.cmake
implementation_files:
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
plan_sources:
  - .codex/plans/20261010-ssa-nonasserting-owner-observers.md
  - 'Root: 2026-10-10 finite pure TryCapture RED authoring'
tests:
  - tests/core/test_ssa_owner_try_capture.c
  - tests/cmake/ssa-owner-try-capture-check.cmake
  - docs/testing-and-validation/ssa-owner-try-capture.md
  - docs/acceptance/ssa-owner-try-capture.md
doc_type: testing-guide
status: finite-validated
---

# Pure TryCapture finite validation

## Current gate

The exact functional RED and fresh default GREEN protocols have been executed and accepted. Public TryCapture now commits a checked candidate on success and preserves the entire output on refusal. The main has no phase parameter. The target default is GREEN; historical RED was explicitly selected against the earlier refusing provider. The [module contract](../parser-and-semantics/ssa-owner-try-capture.md) explains readable typed storage and the shared legacy visitor; the [acceptance record](../acceptance/ssa-owner-try-capture.md) defines required gates and OPEN limits.

## Exact closure and configuration

Target `zr_vm_ssa_owner_try_capture_test` selects exactly, in order:

1. `tests/core/test_ssa_owner_try_capture.c`
2. `tests/core/support/ssa_exec_ir_owner_capture.c`
3. `tests/third_party/zr_unity/Unity/src/unity.c`

CTest `ssa_owner_try_capture` invokes the actual checker without WILL_FAIL. CTest timeout is 180 seconds; its actual Unity subprocess timeout is 120. The child selects no allocatorC, fixtureC, Core/Parser library, Source/VM provider, archive, forced include or stored source copy. Common/Core schema headers are actual checkout headers.

The direct descriptor requires E:/Git/zr_vm and a separate child beneath E:/cargo-targets/zr_vm/build, Windows/Ninja/Debug and the MSVC frontend. It retains the executable compiler ABI probe. Root must supply a fresh unique child/cache/map for each actual phase. Source and output roots are validated; the descriptor does not remove or overwrite an old build tree.

Authored settings are C11/extensionsOFF, /MD, /W4 and /UNDEBUG. The seven normal definitions on the reviewed Windows route are ZR_LIBRARY_TYPE_STATIC=1, ZR_DEBUG=1, ZR_PLATFORM_WIN=1, ZR_PLATFORM_WIN_USE_MSVC=1, _CRT_SECURE_NO_WARNINGS, UNITY_INCLUDE_CONFIG_H and UNITY_USE_FLUSH_STDOUT. The four ZR definitions follow the actual transitive Common/Core header policy; the CRT definition follows normal Windows policy. No /wd or -Wno bypass is introduced. Direct Clang UBSan defaults ON without recovery, and the direct route requires sanitizer driver flags before /link.

| Cache/input | Role |
| --- | --- |
| ZR_SSA_OWNER_TRY_CAPTURE_SOURCE_ROOT | Actual checkout supplied by direct descriptor |
| ZR_SSA_OWNER_TRY_CAPTURE_EXPECTED_PHASE | Current GREEN default; exact RED/GREEN only; checker expectation |
| ZR_SSA_OWNER_TRY_CAPTURE_UBSAN | Direct Clang UBSan default ON |
| ZR_SSA_OWNER_TRY_CAPTURE_MAP_NAME | Optional validated .map basename in build child |
| ZR_TRY_CAPTURE_EXECUTABLE / EXPECTED_PHASE / OUTPUT_DIR | Actual checker inputs |

The target emits `owner-try-capture-target-sources.tsv` and `owner-try-capture-settings.tsv` for actual hashes, compiler/toolchain/link rule and target properties. Those declarations are metadata, not proof of actual compiled sources, latest dependencies, symbols, imports, map providers or CRT linkage. The accepted native gate checks actual latest dependencies, definitions/imports, sole map providers and MD CRT linkage.

## Exact twelve-case order

The following exact order and outcomes match actual historical RED r2 and current default GREEN r1 raw output.

| # | Exact RUN_TEST name | RED r2 actual | GREEN r1 actual |
| --- | --- | --- | --- |
| 1 | test_capture_empty_module | MISSING_FEATURE | PASS |
| 2 | test_capture_scalar_contract_order | MISSING_FEATURE | PASS |
| 3 | test_capture_module_pools | MISSING_FEATURE | PASS |
| 4 | test_capture_function_pools | MISSING_FEATURE | PASS |
| 5 | test_capture_optional_nested_maps | MISSING_FEATURE | PASS |
| 6 | test_capture_excluded_storage_details | MISSING_FEATURE | PASS |
| 7 | test_capture_exact_word_budget | MISSING_FEATURE | PASS |
| 8 | test_capture_null_refusal | PASS | PASS |
| 9 | test_capture_budget_refusal | PASS | PASS |
| 10 | test_capture_structural_refusal | PASS | PASS |
| 11 | test_capture_prior_unity_failure | PRIOR_FAILURE | PRIOR_FAILURE |
| 12 | test_capture_prior_failure_witness | MISSING_FEATURE | PASS |

Failure reasons must be exactly OWNER_TRY_CAPTURE_MISSING_FEATURE or OWNER_TRY_CAPTURE_PRIOR_FAILURE as listed. RED expects actual Unity exit 9, nine failures/three passes/zero ignored. GREEN expects actual exit 1, one deliberate failure/eleven passes/zero ignored. Both actual Unity footers must be FAIL. Wrapper exit 0 accepts the exact negative protocol and cannot substitute for the Unity child exit.

## Fifteen-field CASE protocol

After each actual RUN_TEST returns through Unity conclusion, one exact `OWNER_TRY_CAPTURE_CASE` row follows. Its fifteen named numbers are case, support, feature, drained, calls, accepted, words, expected_words, graph_preserved, output_checked, acquired, freed, prior_failed, prior_ignored and flags_preserved. Common support/drained/graph_preserved/output_checked/flags_preserved values are 1; prior_ignored is 0; prior_failed is 1 only in case 11. Freed equals acquired.

| Case | Calls | Accepted RED/GREEN | Words RED/GREEN | Expected words | Acquired=freed | Feature RED/GREEN |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 0/1 | 0/19 | 19 | 0 | 0/1 |
| 2 | 1 | 0/1 | 0/19 | 19 | 0 | 0/1 |
| 3 | 1 | 0/1 | 0/35 | 35 | 3 | 0/1 |
| 4 | 1 | 0/1 | 0/182 | 182 | 18 | 0/1 |
| 5 | 1 | 0/1 | 0/255 | 255 | 32 | 0/1 |
| 6 | 3 | 0/3 | 0/22 | 22 | 2 | 0/1 |
| 7 | 1 | 0/1 | 0/4096 | 4096 | 1 | 0/1 |
| 8 | 2 | 0/0 | 0/0 | 0 | 0 | 1/1 |
| 9 | 1 | 0/0 | 0/0 | 4099 | 1 | 1/1 |
| 10 | 60 | 0/0 | 0/0 | 0 | 32 | 1/1 |
| 11 | 1 | 0/1 | 0/22 | 22 | 1 | 0/1 |
| 12 | 0 | 0/0 | 0/0 | 22 | 0 | 0/1 |

The checker independently hardcodes explicit seven-column RED/GREEN tables and uses named table_expected parameters to generate all fifteen fields. Words is the last accepted call's count, while calls/accepted are per-case totals. Case 6 calls three times on independent storage and a changed logical value; its final count remains 22. Case 10 makes sixty structural-refusal calls and allocates 32 real roots. Case 12 has no new call/allocation: its feature comes from independent saved-case 11 witness checks, so calls/accepted/words/acquired/freed honestly remain 0.

The graph preservation observation hashes initialized genuine typed object and registered pool bytes before/after each call. Positive feature checks compare every expected logical word and all unused words, without invoking the visitor to generate expectations. Refusal checks compare the entire output representation initialized to 0xA5. This is readable typed storage testing, not arbitrary pointer validation or an ExecIR verifier.

## Final witness and raw output

The exact seven-number final line in both phases is `OWNER_TRY_CAPTURE_FINAL drain=1 live=0 acquired=90 freed=90 prior_failed=0 prior_ignored=0 flags_preserved=1`. It follows twelve ordered result/CASE pairs and precedes exactly one 23-dash separator, exact `12 Tests 9 Failures 0 Ignored` or `12 Tests 1 Failures 0 Ignored` summary and FAIL footer. There are 28 nonempty raw stdout lines. Stderr must be exactly empty.

Unknown output, duplicates, reordered pairs, IGNORE, a wrong named number or reason, unexpected failures, timeout/crash, warnings, sanitizer or ledger diagnostics refuse acceptance. Checker telemetry is `OWNER_TRY_CAPTURE_CHILD phase=<PHASE> actual_exit=<actual>` followed only upon full acceptance by `OWNER_TRY_CAPTURE_PROTOCOL_ACCEPTED phase=<PHASE> cases=12 expected_failures=<9-or-1> ignored=0 actual_exit=<actual>`.

The checker refuses either existing `owner-try-capture-<PHASE>-stdout.log` or `-stderr.log`, runs the actual child, then saves both captured streams before interpretation. CMake file(WRITE) is not OS exclusive creation; Root must supply a fresh unique child and one writer. The actual driver separately controls CreateNew receipts/phase logs. Accepted RED/GREEN receipts retain both raw streams and distinguish Unity exits 9/1 from wrapper exit 0.

## Cleanup, frozen inputs and finite evidence

Resource release and supporting observations finish before ordinary feature assertions. Case 11 uses a real prior Unity failure and nonasserting teardown with actual capture/output/flag witnesses; case 12 checks them after conclusion without a health reset. Final drain contains no Unity assertion. The ignored path remains unexecuted.

The actual pure repository freeze contains exactly fifteen paths: three C sources; captureH; actual exec_ir.h and exec_ir_state_map.h; tests/unity_config.h; actual UnityH/internalsH; three CMake files; and these three guides. Actual transitive header/tool/artifact pins are additional evidence. FixtureH/C and the five changed consumer descriptors have separate Root source review. Lower owned-module/module-copy regression now has actual evidence; Function and Source consumers remain OPEN. This pure fifteen-path set does not include their runtime proof.

Functional RED, the positive GREEN adapter, fresh default GREEN, latest dependency/symbol/map closure and three-unit MSVC compilation are accepted below. The original fifteen-file freezes retain historical hashes for these three guides; this documentation update creates distinct current guide pins without a runtime rerun. RED provider H/C and default-CMake pins are also historical after the authorized GREEN transition. The acceptance record lists these bridges. Ledger/schedule purity, Source's twenty cases and genuine finalizer, VM/Function/network/FFI, full47, full normal/default project, Linux, full MSVC runtime, ignored-path execution and complete OOM/leak instrumentation remain OPEN.

## Actual native results

Receipts and phase/raw logs are under E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b. Exact receipt SHA256 values, source history and artifact pins are in the [acceptance record](../acceptance/ssa-owner-try-capture.md).

- Pure RED r2 used fresh E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-try-capture-red-r2: three actual C compilations plus link, six natural terminal phases all 0, three genuine guard passes and nine specified failures, child 9/wrapper 0. All twelve CASE rows and FINAL match; stderr is empty.
- Pure default GREEN r1 used fresh E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-try-capture-green-r1: configure had no phase override, actual cache/settings GREEN, three compilations plus link, six natural terminal phases all 0. Eleven passes and the exact case 11 prior failure give child 1/wrapper 0, zero ignored, 28 nonempty raw lines and ninety registered roots freed. Exact 4096 words, 4099-word refusal and sixty shape refusals are observed. Actual latest dependency counts are 190/183/29 for main/capture/Unity.
- Both pure builds have zero warning or UBSan diagnostics, actual C11/MD/W4/UNDEBUG and seven normal definitions. Four sole providers are TryCapture, VisitContents, UnityDefaultTestRun and UnityEnd. Bounded ordinary AMD64 COFF evidence separates five RED/six GREEN owned functions from seven SDK COMDAT wrappers; only SDK _vsnprintf_l imports __imp___stdio_common_vsprintf. This is graph admission, not an extra execution-purity result.
- MSVC GREEN r1 used fresh E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-try-capture-msvc-GREEN-r1. Three natural compile-only children exit 0 with zero warnings, basename-only stdout and empty stderr. SourceDependencies JSON1.2 includes 85/78/19 headers; ProvidedModule is present and empty, while ImportedModules and ImportedHeaderUnits are absent. There is no MSVC link or runtime result, or fixed ABI-byte claim.
- Lower consumer GREEN r3 used fresh E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-owner-capture-consumer-regression-green-r3. Actual build is 43 C compilations and three links, 25 unique C files, 43 latest VALID dependency records, 32 reviewed forced-include units and eleven without forced includes. Those legacy allocator shims are confined to this separate regression, not the pure child. Zero warnings/UBSan diagnostics; only owned-module, CoreModel and module-copy CTests execute and pass. Their results are 14+20 Unity passes, seventeen CoreModel default calls, module-copy measured 60+28=88/completed88 and two owned measured61/completed61 sweeps. Twenty-six unselected configured compile commands remain unbuilt and unrun. Root's complete raw/input review found 578 selected pin occurrences matching before this guide update.

Historical failures remain failures: pure RED r1 stopped before CTest because the adapter rejected normal SDK inline formatting imports; consumer r1 stopped before compilation because the CMake3.31 double-underscore rule selector was wrong; consumer r2 stopped before CTest because broad diagnostic matching mistook COFF R ledger-format constants for emitted errors. Reviewed driver corrections admit the bounded SDK graph and distinguish symbol listings from actual diagnostics. They change no project source or diagnostic suppression policy.

Root reports naturally closed controllers: default GREEN 28544/unified38297/terminal d378e6 exit0; MSVC 22048/unified21583/terminal 2e31bc exit0; consumer 23040/unified32150/terminal 4f02df exit0. Receipt child phase records independently retain actual WaitForExit terminal status. Root has appended the shared registration; the Git transaction remains tracked separately by Root, with no commit claimed here.
