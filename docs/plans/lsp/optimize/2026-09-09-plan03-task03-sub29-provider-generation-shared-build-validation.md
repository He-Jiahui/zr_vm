---
plan_id: optimize
task: plan03-task03-sub29
status: completed
related_code:
  - tests/language_server/test_lsp_analysis_provider_generation_cases.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_semantic_snapshot.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.29: Provider Generation Shared Build Validation

## Failure And Change

The MSVC shared-library build failed with LNK2019 because the Sub27 same-AST
test called the internal, unexported `Lsp_ProjectAnalyzeDocument`. The fixture
has no project index, so that path only synchronizes the generation and calls
`SemanticAnalyzer_Analyze`.

The regression now uses the existing exported analyzer lookup and Analyze APIs.
It still warms both caches, proves a same-AST cache hit, advances the provider
generation, and requires one fresh whole-document analysis plus fresh scoped
facts. It additionally checks immediately after lookup that the previous
semantic context and scoped analyzer are unavailable. The URI fixture uses a
mutable character array to match the existing Core string API without discarding
const. No production API or export was added.

## Verification

Validation source is `eb13caf8` plus this test edit and the existing shared
worktree changes. The three current builds are:

| Toolchain | Configuration | Build Directory |
| --- | --- | --- |
| GCC | Debug, static | `/home/hejiahui/.codex-builds/l8-callable-value-gcc` |
| Clang | Debug, shared, ASan/UBSan/LSan | `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang` |
| MSVC | Debug, shared | `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current` |

All three parity executables pass `21/21` and exit 0. Clang runs with
`ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1`; parity has no
sanitizer report. The MSVC const-discard warning in this test is gone. Snapshot
CTest passes `1/1` on Clang and MSVC, completing the GCC result from Sub28.

The complete project-feature executables run on Clang and MSVC with `53 PASS /
7 FAIL`. The failure-name sets exactly match Sub28 GCC. All three source,
watched-binary, and watched-descriptor refresh cases pass on each compiler.
The complete project executables still exit 1. Clang also reports `19160 bytes /
481 allocations` through LSan, matching the earlier
[project runner baseline](2026-09-07-plan01-task06-sub07-rename-canonical-type-assertions.md).
This is functional evidence for Sub28, not acceptance of the complete project
runner or the parent memory gate.

Build the following targets with `cmake --build <build> --target ... --parallel 16`:

```text
zr_vm_language_server_semantic_query_parity_test
zr_vm_language_server_lsp_project_features_test
zr_vm_language_server_lsp_semantic_snapshot_test
```

Run the corresponding executables under `<build>/bin/`, and run snapshot with
`ctest --test-dir <build> --output-on-failure -R '^language_server_lsp_semantic_snapshot$'`.
Windows build and CTest commands use the `using-vsdevcmd` environment wrapper.
Full local logs are `.codex/lsp-optimize-validation/plan03-task03-sub29-`
`parity-{gcc,clang,msvc}.log` and `project-{clang,msvc}.log`.

## 状态与产出记录

- 开始时间：2026-09-09 10:23 +08:00。
- 实际完成时间：2026-09-09 10:28 +08:00。
- 状态：Sub29 测试兼容修复完成；Sub27/Sub28 三工具链功能验证已补齐。
- 源码版本：`eb13caf8` 加本项 test header 修改与共享工作区既有修改。
- 产出：公开 API 驱动的 same-AST 回归、立即屏蔽旧 facts 的断言、当前三工具链验证记录。
- 剩余门槛：project runner 七项既有失败和 Clang LSan，multi-provider/non-source
  relation matrix、Task 3/7/8 与全计划验收保持未完成。
