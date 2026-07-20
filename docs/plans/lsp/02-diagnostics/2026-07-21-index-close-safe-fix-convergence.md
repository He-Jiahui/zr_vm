---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
plan_sources:
  - user: strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_diagnostic_safe_fix_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-07-21-index-close-safe-fix-convergence
status: completed
completed_at: 2026-07-21 06:21 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: index-close-safe-fix-convergence
---

# Index-Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 06:21 +08:00 | 已完成 | `missing_index_close`从parser-owned独立primary/fix range到零宽`]` machine edit、通用LSP quickfix和stdio apply/rebind清除闭环；primary range保持opening `[`，consumer不按code/message/source重建；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingIndexClose`接受primary `location`和独立`fixLocation`；先构造既有structured diagnostic，再把fix range收敛到`fixLocation.start`并发布`Insert missing ']'`、edit text `]`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- `report_missing_index_close`保留opening `[`作为primary range，同时把parser当前token location传给builder。`value[0;`因此只在`;`起点插入`]`，不覆盖`[`、索引表达式或`;`。
- Fix分配失败会释放已构造的diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增index-specific分支；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。
- 修正后的document version重新parse/rebind，不再发布`missing_index_close`。

## TDD与根因证据

- Parser API RED在稳定`1682722`生产基线上编译失败，新增dual-range builder case对旧单range API产生预期的too-many-arguments错误，证明旧contract无法同时保留opening primary与恢复点edit。
- LSP RED为43项 / 1 failure；新增index-close case已有structured diagnostic但没有machine fix，其余42项通过。
- Producer-only GREEN后builder为21 Tests / 0 Failures，advanced-editor为43项0 failure，GCC stdio diagnostic-fix smoke真实exit 0；未修改任何LSP production文件。
- Exact protocol fixture使用`var value = [1, 2];\nreturn value[0;\n`，edit固定为line 1 character 14的零宽`]`插入；version 2改为`return value[0];`后该code清除。

## 工具链与回归证据

- 正式隔离source为`HEAD 1682722e223e3eb6339a21cb031fdf305315ed03`加7个code/test exact overlays；后续Syntax 03 M4中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用指向同一source snapshot的独立static cache。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均21 Tests / 0 Failures；三套advanced editor均43项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`与fix结构不变；builder public API只增加独立fix range输入，并填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。

## 未完成边界

- `missing_parameter_list_close`及其他delimiter producer尚未完成machine-fix收敛；delimiter replacement/mix仍需独立语义边界。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
