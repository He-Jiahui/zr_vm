---
related_code:
  - tests/language_server/collect_lsp_baseline.js
  - tests/cmake/run_executable_suite.cmake
  - tests/CMakeLists.txt
implementation_files: []
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - language_server
doc_type: acceptance-record
---

# Plan 00 Task 1 Sub04: Current GCC Aggregate

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-06 10:53 +08:00 | 2026-09-06 11:03 +08:00 | completed (collection subitem; parent pending) | Built and ran all 83 aggregate members; 74 passed, nine failed, 64 failure blocks preserved. | GCC build 375/375; collector exit 1; no spawn errors, signals or timeouts. |

## Source And Reproduction

HEAD at collection was `6a17a075c3f9f5664a63db5b480e9797d072006b`.
The source checkout includes uncommitted semantic projection, call-binding,
runtime, parser and library changes from other sessions. The collector's
`sourceCommit` is the checkout HEAD only; it does not attest that those overlays
belong to that commit. The source tree was not frozen during this replay, so
this record cannot close the final same-commit acceptance gate.

The GCC 11.4.0 Debug shared build is `.codex/build-lsp-opt-gcc`, configured
with the private Linux Node 22.13.1 runtime. The first missing symbol-table
target was built successfully (45 steps, including updated shared libraries).
Building all 83 target names from the CMake aggregate then completed 375/375.
This resolves the missing-product boundary in the earlier
[focused replay](2026-09-06-plan00-task01-current-gcc-replay.md).

```text
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --target <all 83 language_server aggregate targets> --parallel 8
  [375/375] linked successfully

/home/hejiahui/.codex-tools/node-22.13.1/node-v22.13.1-linux-x64/bin/node \
  /mnt/e/Git/zr_vm/tests/language_server/collect_lsp_baseline.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/current-gcc-aggregate-20260906-1103 \
  --source-commit=6a17a075c3f9f5664a63db5b480e9797d072006b
  Suite baseline: 74 passed, 9 failed; exit 1
```

The collector reads the exact executable list from CTest JSON and runs every
member even when an earlier member fails. The complete result, exact failure
text and per-executable log names are retained in the
[machine-readable result](2026-09-06-plan00-task01-sub04-current-gcc-failures.json).
Raw logs remain in the output directory above. Each failed test's input is its
named fixture; the failure blocks contain the observed assertion and expected
value. Every failed executable exits 1, and all 83 results have no signal,
spawn error or timeout. These are aggregate members, not all standalone LSP
and protocol tests.

## Failures And Owners

Executable names below have the prefix `zr_vm_language_server_`.

| Executable suffix | Failure blocks | Responsible layer and remaining plan |
| --- | ---: | --- |
| semantic_analyzer_test | 12 | Compiler facts and analyzer symbol/type projection; Plan 03 Tasks 6/7/8. |
| ownership_diagnostics_test | 17 | Ownership/flow evidence and diagnostic projection; syntax 02/04, Plan 03 Tasks 6/7/8. |
| local_semantic_query_test | 3 | Reference identities and provider dependencies; Plan 02 Task 3, Plan 03 Task 7. |
| local_semantic_hover_test | 4 | Local/member write reference facts and hover projection; Plan 03 Task 7. |
| lsp_decorator_utf16_ranges_test | 1 | Canonical declaration range versus expression token range; Plan 03 Task 7, Plan 04 Task 7. |
| lsp_interface_test | 8 | Canonical member/generic/external/local query consumers; Plan 03 Tasks 7/8. |
| lsp_project_features_test | 14 | Source/binary/native provider facts and project refresh; Plan 03 Tasks 3/7. |
| lsp_advanced_editor_features_test | 1 | Diagnostic placeholder-fix producer and safe edit filtering; Plan 03 Task 6, Plan 04 Task 6. |
| semantic_query_parity_test | 4 | Canonical local references and detached-analyzer projection; Plan 03 Task 7. |

The source-contract executable now passes. Relative to the historical frozen
GCC collection (73/10, 66 blocks), the observed result has one additional pass
and two fewer failure blocks. The source and overlays differ, so this is a
comparison of observations rather than attribution to one commit.

## Remaining Gates

Plan 00 Task 1 remains pending for the exact semantic-owner commits or release,
a reproducible frozen current-source baseline, and the full prescribed LSP
matrix. This subitem accepts collection completeness only. No semantic failure
is waived, and no later phase is promoted.
