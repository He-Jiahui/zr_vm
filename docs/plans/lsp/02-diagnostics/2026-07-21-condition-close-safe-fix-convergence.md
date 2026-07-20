---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
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
record_id: 2026-07-21-condition-close-safe-fix-convergence
status: completed
completed_at: 2026-07-21 05:42 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: condition-close-safe-fix-convergence
---

# Condition-Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 05:42 +08:00 | 已完成 | `missing_condition_close`从parser-owned零宽`)` machine edit到通用LSP quickfix和stdio apply/rebind清除闭环；primary range保持block opener token，consumer不按code/message/statement kind/source重建；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingConditionClose`先构造既有structured diagnostic，再在`location.start`发布`Insert missing ')'`、edit text `)`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`的零宽fix。
- Primary diagnostic range继续覆盖触发parser恢复的block opener；fix range不覆盖或替换`{`，只在其前插入`)`。
- Fix分配失败会释放已构造的diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增condition-specific分支；既有structured-fix consumer复制title/range/text并复用code-action snapshot。stdio继续从同一fix数组序列化`Diagnostic.data.fixes`。
- 修正后的document version重新parse/rebind，不再发布`missing_condition_close`。

## TDD与根因证据

- Parser RED在稳定`ef7c6e1`生产基线上为20 Tests / 1 Failure；新增builder case只在`diagnostic.fixes.isValid`断言失败。
- LSP RED为42项 / 1 failure；既有diagnostic存在，但没有可供通用code-action consumer投影的machine fix，其余41项通过。
- Producer-only GREEN后builder为20/20，advanced-editor为42项0 failure，stdio diagnostic-fix smoke真实exit 0；未修改任何LSP production文件。
- Exact range用`if (ready { return 1; }`固定为line 0 character 10的零宽edit，primary仍为`{` token range。

## 工具链与回归证据

- 正式隔离source为`HEAD d057d7d395c13be5641e6bab227e07fa42f1273a`加5个code/test exact overlays；Syntax 03 M3的19-path commit完整进入快照，后续M4中间态未进入。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用独立static cache。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均20 Tests / 0 Failures；三套advanced editor均42项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。
- MSVC首次build命令使用了该cache不存在的`zr_vm_cli`alias，在编译前以unknown target退出；该轮作废。改用真实target `zr_vm_cli_executable`后491/491完成，随后才执行上述正式测试。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`与fix结构不变；只让既有builder填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用上一里程碑已经验收的version/generation/open-state/length/hash门禁。

## 未完成边界

- `missing_index_close`、`missing_parameter_list_close`及其他delimiter producer尚未完成machine-fix收敛；delimiter replacement/mix仍需独立语义边界。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
