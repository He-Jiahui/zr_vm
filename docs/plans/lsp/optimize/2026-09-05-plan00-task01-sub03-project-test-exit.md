---
related_code:
  - tests/language_server/test_lsp_project_features.c
  - tests/cmake/run_executable_suite.cmake
implementation_files:
  - tests/language_server/test_lsp_project_features.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/06-modularization-performance-and-acceptance.md
tests:
  - tests/language_server/test_lsp_project_features.c
doc_type: milestone-record
---

# Project Test Failure Exit Status

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 18:11 +08:00 | 2026-09-05 18:22 +08:00 | completed (harness subitem; semantic parent pending) | Count project-feature TEST_FAIL reports and return nonzero; retain every fixture and assertion; report the failure count. | GCC/Clang/MSVC each build successfully and run all cases: 46 printed passes, 14 printed failures, exit 1. Independent specification and quality reviews approved. |

## RED And Repair

The [frozen GCC baseline](2026-09-05-plan00-task01-sub02-gcc-baseline.md) records
`zr_vm_language_server_lsp_project_features_test` exiting 0 despite 14 explicit
failure blocks. CTest's executable-suite runner consumes the exit code, so this
can incorrectly report success when the earlier members pass.

The existing `TEST_FAIL` macro now increments a file-local counter, following
the adjacent semantic-analyzer test's convention. `main` reports that count and
returns 1 when it is nonzero. Setup failures retain their explicit early return
1. No semantic fixture, expected target, assertion or case registration changes.
The counter belongs to the single serial test process and stores no compiler or
snapshot identity.

## Verification

The pre-fix real GCC executable is the archived RED: 14 failures / exit 0.
All three rebuilt executables preserve exactly the same 14 failure names and
46 printed passes, and now report `Project feature tests completed with 14
failure(s)` followed by exit 1. The expected nonzero status proves the harness
repair; it is not a passing semantic suite.

```text
cmake --build <build-dir> --target zr_vm_language_server_lsp_project_features_test --parallel 6
wsl.exe --cd /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc --exec env LD_LIBRARY_PATH=/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc/lib /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc/bin/zr_vm_language_server_lsp_project_features_test
wsl.exe --cd /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang --exec env LD_LIBRARY_PATH=/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang/lib /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang/bin/zr_vm_language_server_lsp_project_features_test
E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_lsp_project_features_test.exe
git diff --check -- tests/language_server/test_lsp_project_features.c
```

GCC 11.4 and Clang 14 Debug shared, MSVC 19.44 Debug static builds exit 0.
Clang reports pre-existing incomplete descriptor-fixture initializers; MSVC
reports the existing /W3 override and unrelated long object-path warnings.
No changed-source compiler diagnostic. Scoped whitespace check passes.

Raw generated logs are `.codex/lsp-project-harness-gcc.log`,
`.codex/lsp-project-harness-clang.log` and `.codex/lsp-project-harness-msvc.log`.
Comparing the failure names with the archived baseline JSON, ignoring elapsed
time, yields zero changes for all three toolchains. The same semantic/provider
failures keep their Plan 03/04 owners from the baseline record.

## Source And Remaining Gates

Builds use the existing isolated frozen source: original c95e5387 and exact
gitlinks plus this session's capability corrections, with only this test file
added for the harness repair. Concurrent ownership and semantic commits/overlays
are excluded. The record's containing commit identifies the test change; future
integrated acceptance must replay all current committed source together.

The 8118-line project test remains oversized. This five-line harness correction
adds no test responsibility or fixture and avoids mixing the pending source,
binary/native provider case extraction into failure-reporting repair. Plan 06
must extract the shared test harness and provider case groups with unchanged
registration/exit semantics. No syntax rule or compiler ownership changes here.
Module guidance is [LSP Stdio Validation](../../../cli-and-tooling/lsp-stdio-validation.md).
