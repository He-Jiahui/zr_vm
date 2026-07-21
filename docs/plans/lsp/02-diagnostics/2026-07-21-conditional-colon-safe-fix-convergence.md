---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
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
record_id: 2026-07-21-conditional-colon-safe-fix-convergence
status: completed
completed_at: 2026-07-21 12:26 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: conditional-colon-safe-fix-convergence
---

# Conditional Colon Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 12:26 +08:00 | 已完成 | `missing_conditional_colon`保留`?` token primary，仅当current token可开始alternate expression时在其起点发布零宽`:` machine edit；`return true ? 1;`保留诊断但不发布无效fix，missing consequent/alternate同样无machine fix；通用LSP quickfix/stdio完成apply-rebind闭环；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `parse_conditional_expression`复用既有`parser_token_can_start_expression`，在缺少colon时把`hasAlternateExpression`结构化事实传给reporter；没有复制token whitelist，也没有改变expression选择或恢复规则。
- `ZrParser_DiagnosticBuilder_BuildMissingConditionalColon`保留question token primary，并接受current-token fix range与`hasAlternateExpression`。只有后者为true时才发布`Insert missing ':'`、edit text `:`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- `return true ? 1 2;`报告line 0 characters 12..13，fix为characters 16..16；应用后`return true ? 1 : 2;`的新version清除`missing_conditional_colon`。
- `return true ? 1;`仍报告`missing_conditional_colon`，但`;`不能开始alternate expression，因此fix数组为空。`return true ? : 2;`与`return true ? 1 : ;`分别保留`missing_conditional_consequent`与`missing_conditional_alternate`，均无machine fix。
- Fix分配失败会释放已构造diagnostic并返回失败；无alternate时返回完整structured diagnostic，不发布半初始化fix。
- LSP production无需新增conditional特判；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。

## TDD与审查证据

- 初始builder API RED在旧单range签名上编译失败；advanced-editor RED为54项 / 1 failure，stdio RED真实exit 1并停在缺少serialized conditional-colon fix。
- 首轮focused GREEN后，提交前Critical/Important审查发现`return true ? 1;`不能只插入`:`；该实现与证据作废，新增parser/LSP/stdio负边界后重新取得三层RED。
- 第二轮parser RED因旧四参数builder API无法接收`hasAlternateExpression`而编译失败；advanced-editor为54项 / 1 failure，唯一失败是no-alternate case仍暴露action；stdio真实exit 1并停在`missing_conditional_colon`不应发布machine fix。
- Support-first修复只扩展既有parser调用链传递结构化bool；没有在diagnostic builder或LSP复制expression-start token集合，也没有按diagnostic code、message或source text推断alternate。
- Focused GREEN为parser builder 31 Tests / 0 Failures、advanced editor 54项 / 0 failure、stdio diagnostic-fix smoke真实exit 0。

## 工具链与回归证据

- 正式隔离source内容为`HEAD 38dce5935218e8d81642226802400f4802feba01`加9个conditional-colon code/test exact overlays；9个overlay与source snapshot逐文件SHA-256匹配，并发Syntax 03 M5中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用指向同一source snapshot的独立static cache，并重建相同20个验收目标。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，failure marker为0。
- 三套parser builder均31 Tests / 0 Failures；三套advanced editor均54项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`与fix结构不变；public builder API新增独立fix range及`hasAlternateExpression`，parser reporter同步传递该producer-owned fact。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。
- `diagnostic_builder.c`已超过约1100行，但本leaf只扩展既有delimiter builder职责；剩余delimiter leaf完成后，应按既有记录提取cohesive `diagnostic_builder_delimiter_fixes`模块。

## 未完成边界

- 本leaf不为缺失consequent或alternate expression合成表达式，也不把这两类placeholder建议提升为machine fix。
- 其他delimiter producer与delimiter replacement/mix尚未完成machine-fix收敛。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
