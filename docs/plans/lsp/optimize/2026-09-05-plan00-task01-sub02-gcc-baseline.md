---
related_code:
  - tests/language_server/collect_lsp_baseline.js
  - tests/cmake/run_executable_suite.cmake
  - tests/language_server/test_lsp_project_features.c
implementation_files:
  - tests/language_server/collect_lsp_baseline.js
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/06-modularization-performance-and-acceptance.md
tests:
  - tests/language_server/collect_lsp_baseline.js
  - tests/language_server/collect_lsp_baseline_test.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/test_lsp_reaching_definition_navigation.c
doc_type: milestone-record
---

# Current GCC Failure Baseline

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-05 17:02 +08:00 | 2026-09-05 18:04 +08:00 | completed (collection subitem; parent pending) | All 83 aggregate executables run; 73 pass and 10 fail; exact 66 failure blocks archived; exit-zero false success identified. Collector detects stdout/stderr failures and timeout/spawn errors, preserves evidence and supports explicit build configuration. | Real GCC collection exit 1 as expected (73/10); Node 12 and Node 22 regression tests 11/11; new CTest registration 1/1 (0.44 s); spec and independent quality reviews approved. Integrated Task 1 baseline still awaits active semantic commits. |

## Source And Reproduction

Source code: `670e3cd0bba9b2cae2a88fa6ea1f5e6be0e7160f`, with exact gitlink
revisions exported from the original c95e5387 snapshot. The only production
changes since that export are the committed resolve and navigation capability
withdrawals. Existing sessions' analyzer/symbol-projection/core/FFI/benchmark
overlays are excluded. GCC 11.4.0, CMake 3.22.1, Ninja, WSL Node 12.22.9,
Debug shared build:
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc`.

The existing CTest aggregate stops at its first failed executable. The new
collector consumes `ctest --show-only=json-v1`, extracts that aggregate's exact
executable list, optionally builds those targets, runs all of them serially and
records exit code, signal, timeout/error, duration and exact failure messages.
It rejects an existing completed output directory and fails when output contains
the test harness's failure markers even if a test process incorrectly exits 0.
Both stdout and stderr are inspected. A timeout/spawn error or terminating
signal also fails the result, including a child that handles SIGTERM and exits 0.
Multi-config builds require `--config=Debug` (or their selected configuration),
which selects the same CTest/CMake configuration and native library directory.
The supplied source commit is caller provenance, not independently attested by
the collector; this record explains the source export that establishes it.

```text
wsl.exe --exec node /mnt/e/Git/zr_vm/tests/language_server/collect_lsp_baseline.js /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/logs/full-leaf-baseline-v3 --source-commit=670e3cd0bba9b2cae2a88fa6ea1f5e6be0e7160f
```

The earlier collection used `--build` to build every registered aggregate
target. The final unchanged binaries produce 73 passed, 10 failed, exit 1.
Results and all exact observed failure blocks are archived in
[the machine-readable baseline](2026-09-05-plan00-task01-sub02-gcc-failures.json).
Raw per-executable logs remain in the output directory named above. These are
83 aggregate members, not every parser test or every standalone LSP executable.

The archived run predates the collector's review fixes; its raw stderr streams
contain no additional failure markers, and its result set has no timeout/spawn
errors. Therefore the 83/73/10 counts and 66 failure blocks remain unchanged.
The final collector has 11 Node 12-compatible isolated regression cases. Review
RED reproduces stdout-only collection (5/6), then zero-status timeout and missing
multi-config handling (7/11). Corrected source passes 11/11 on WSL Node 12 and
Windows Node 22. Registered GCC CTest passes 1/1 (0.44 s). Both independent
review stages approve the fixes; scoped whitespace and syntax checks pass.

```text
wsl.exe --exec node /mnt/e/Git/zr_vm/tests/language_server/collect_lsp_baseline_test.js
node tests/language_server/collect_lsp_baseline_test.js
ctest --test-dir <build-dir> --output-on-failure -R ^language_server_baseline_collector$
```

Only the output parser/CLI validation is isolated with injected process and
filesystem effects in these tests; the archived aggregate run uses real native
executables. Multi-config argument/library selection is fixture-verified and
is not a claim of a fresh Visual Studio multi-config aggregate run.

## Failures And Owners

Each input is the named test fixture in the corresponding C/JS source. The JSON
asset records each case's name and exact actual/expected failure text, including
source ranges, missing facts and diagnostics. The counts below are emitted
failure blocks; some test harnesses emit both a detailed failure and case label.

| Executable suffix (prefix `zr_vm_language_server_`) | Failure blocks / exit | Expected versus actual summary | Responsible support layer and plan |
| --- | --- | --- | --- |
| semantic_analyzer_test | 12 / 1 | Canonical diagnostics, symbols and callable/generic signatures; missing identities or expression-only hover appears. | Compiler facts and analyzer projection, Plan 03 Tasks 6/7/8; active projection/type-query owners retained. |
| ownership_diagnostics_test | 17 / 1 | Loan/move/release/alias-region/guard facts should retain exact owner evidence; facts or related locations absent. | Compiler ownership/flow producer and LSP projection, syntax 02/04 and Plan 03 Tasks 6/7/8. |
| local_semantic_query_test | 3 / 1 | Local/member write resolution and direct function-value invalidation; unresolved member or conservative dependency instead. | Compiler references/provider dependencies and local query, Plan 02 Task 3 and Plan 03 Task 7. |
| local_semantic_hover_test | 4 / 1 | Assignment/member-write hover should expose canonical reference; hover/rich hover null. | Plan 03 Task 7 hover consumer after lower reference binding. |
| lsp_decorator_utf16_ranges_test | 1 / 1 | Expected UTF-16 range 2:14-3:16; actual 2:12-2:19. | Parser-origin range/hover projection, Plan 03 Task 7 and Plan 04 Task 7. |
| lsp_source_contracts_test | 2 / 1 | Canonical constructor/super-signature boundary checks fail. | Source-contract audit against current consumer implementation, Plan 03 Task 7; do not waive without code evidence. |
| lsp_interface_test | 8 / 1 | Decorator/statement/variable/member/generic queries should expose exact semantic data; assertions fail as archived. | Compiler/query consumer convergence, Plan 03 Tasks 7/8 and Plan 04 acceptance. |
| lsp_project_features_test | 14 / 0 | Module/native/FFI/pooling projection expectations fail, yet test main unconditionally returns 0. | Underlying Plan 03 Task 3/7 provider facts; harness exit-status repair Plan 00 Task 1 / Plan 06 Task 3. |
| lsp_advanced_editor_features_test | 1 / 1 | A possibly_uninitialized_read diagnostic should retain its placeholder fix without promoting it to a safe code action; the expected diagnostic/fix or suppression contract fails. | Compiler diagnostic producer and diagnostic/edit consumer, Plan 03 Task 6 and Plan 04 Task 6; inspect producer evidence before editing the consumer. |
| semantic_query_parity_test | 4 / 1 | Local references/missing-canonical fixture/detached-analyzer source hover should resolve exact facts; initial query fails. | Active canonical symbol projection, Plan 03 Task 7; no concurrent overlay included. |

## Other Current Evidence

The initial full `ctest -R language_server` at ce04018c reports 12/15 pass. Its
three failures are the first failed aggregate executable above, generic
completion detail in stdio smoke, and missing possibly_uninitialized_read in
diagnostic-fix smoke. Both protocol failures already exist in c95e5387.
Inline-value, UTF-16 handshake, type hierarchy, snapshot workspace diagnostics,
workspace folders, sync, lifecycle and capability inventory pass in that run.

The additional reaching-definition executable is not in the aggregate. Its
three cases all fail: single/branch writes return no locations and the
missing-source case cannot prepare a query. The protocol `return seed` probe
also returns `[]`; this remains Plan 03/04 work.

Clang and MSVC focused protocol checks expose a setup-timeout defect: cancel-known
starts a 3000 ms deadline while the preceding 2048-class document is still being
analyzed. A probe measures 3897.40 ms for that analysis/queued response, then
0.33 ms between diagnostics and the correct cancellation error. Plan 00 Task 3
and Plan 01 Task 4 own the fixture/state investigation without relaxing the
frozen cancellation observation budget.

Extension compile/configured noEmit passed after lockfile installation, and the
latest unit run is 39/39. The tsconfig excludes the worker: an explicit strict
worker check has 17 identical baseline/current type diagnostics, zero introduced
by capability withdrawal. Plan 05 owns their closure and full WASM parity.

## Acceptance Boundary

This baseline records failures; it does not repair or accept them. Original
historical records are retained. Plan 00 Task 1 full current-source acceptance
still waits for the explicitly retained semantic session commits and a fresh
integrated replay. No subsequent phase is promoted by this collection.
