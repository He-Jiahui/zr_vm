---
plan_id: optimize
task: plan03-task03-sub36
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_module_ranges.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_module_ranges.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_project_module_summary_range_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/acceptance/2026-09-12-plan03-task03-sub36-project-module-summary-range.md
doc_type: milestone-record
---

# Plan 03 Task 3.36: Exact Source Project-Module Summary Ranges

## Failure And Change

`projectModules` already retained each source record's canonical URI and
module identity, but every source summary used the synthetic `(0, 0)` file
entry range. That made a source module look navigable while losing the
declaration token that the parser had already recorded. The focused GCC
interface runner was RED: the new `lsp_ownership/main.zr` assertion found the
`main` source summary but rejected its zero range.

The project layer now exposes a small, read-only source-range helper. It asks
the incremental parser for the current AST, requires a script with an
explicit `ZR_AST_MODULE_DECLARATION`, and verifies that the parsed module
value still matches the project record. The helper returns the parser-owned
module-name `SZrFileRange`, preserving its source URI; missing ASTs, implicit
module names, or stale identity return the existing origin fallback.

`lsp_interface.c` converts that range through the document-aware UTF-16
projection before publishing both direct source records and source records
reached through an import. Binary metadata and native summaries keep their
existing entry/projection ranges. The new helper lives in its own project
module so the already-large project index implementation does not gain another
responsibility.

## Verification

The focused GCC validation build is `/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc`.
After refreshing its CMake glob list for the new source module, the target
compiled and linked successfully:

```text
wsl.exe -e /bin/bash -c "cd /mnt/e/Git/zr_vm && \
  ninja -C .codex/build-lsp-opt-gcc zr_vm_language_server_lsp_interface_test"
```

The interface runner exits 1 because the frozen two-case baseline remains:
`LSP Class Member Navigation And Completion` and `LSP Hover And Completion
Surface Explicit Exact Type Failures`. The new
`LSP Project Modules Publish Exact Source Declaration Range` case passes,
along with the adjacent project module summary and source-bootstrap cases.
The RED run on the same target failed only at the new exact-range assertion.

## 状态与产出记录

- Started: 2026-09-12 01:27 +08:00.
- Completed: 2026-09-12 01:52 +08:00.
- Status: completed for the source `projectModules` range submilestone; parent
  Task 3 and Tasks 7/8 remain in progress.
- RED: source summary for explicit `module main;` returned the old zero entry
  range.
- GREEN: parser-backed range extraction and content-aware UTF-16 projection
  return LSP range `(0,7)-(0,11)` for `main`; implicit/no-AST fallback remains
  `(0,0)`.
- Outputs: dedicated project range module, internal API declaration, focused
  test header/call, and synchronized module/plan/acceptance documentation.
- Remaining: Clang/MSVC replay for this slice, binary virtual declaration URI
  production, source/binary origin producers, multi-definition identity and
  the full Task 3/7/8 acceptance matrix.
