---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
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
record_id: 2026-07-21-array-element-separator-safe-fix-convergence
status: completed
completed_at: 2026-07-21 12:53 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: array-element-separator-safe-fix-convergence
---

# Array Element-Separator Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 12:53 +08:00 | 已完成 | `missing_array_element_separator`保留next-element token primary并在同token起点发布规范零宽`,` machine edit，完成通用LSP quickfix/stdio apply-rebind闭环；`array_element_assignment`保持无fix，consumer不按array AST/code/message/source重建；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `parse_array_literal`既有producer只在前一元素已完成且current token可开始下一元素时报告`missing_array_element_separator`，并把该token exact range传给shared reporter；本leaf不改变token集合、表达式选择或恢复规则。
- `ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator`先构造原structured diagnostic，保留next-element token primary，再把临时edit range收敛到`location.start`并发布`Insert missing ','`、edit text `,`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- `return [1 2];`报告line 0 characters 10..11，fix为characters 10..10；应用后`return [1, 2];`的新version清除`missing_array_element_separator`。
- Array语法仍接受`,`或`;`，machine fix选择规范逗号，不改写已有合法分号separator。
- `return [value = 1];`继续发布`array_element_assignment`且fix数组为空；标点插入不能把有副作用的assignment转换成合法value element。
- Fix分配失败会释放已构造diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增array-separator特判；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。

## TDD与根因证据

- Parser builder RED为33 Tests / 1 Failure，唯一新增separator case在`diagnostic.fixes.isValid`断言失败；assignment无fix负边界通过。
- Advanced-editor RED为56项 / 1 failure，唯一新增separator case没有machine action；assignment负边界通过。
- Stdio RED真实exit 1并精确停在`Expected one serialized array-element-separator diagnostic fix`。
- Support-first审计确认producer和shared reporter已经传递正确next-element token range，缺口只在builder未发布fix，因此没有修改parser语法、reporter、public API或LSP consumer。
- Focused GREEN为parser builder 33 Tests / 0 Failures、advanced editor 56项 / 0 failure、stdio diagnostic-fix smoke真实exit 0。

## 工具链与回归证据

- 正式隔离source内容为`HEAD 396a11472127c1ab87669d1007f25d4d8f942704`加5个array-element-separator code/test exact overlays；5个overlay与source snapshot逐文件SHA-256匹配，并发Syntax 03 M5中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用指向同一source snapshot的独立static cache，并重建相同20个验收目标。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，failure marker为0。
- 三套parser builder均33 Tests / 0 Failures；三套advanced editor均56项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`、fix结构、builder public API与parser reporter签名均不变；builder只填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。
- `diagnostic_builder.c`已超过约1100行，但本leaf只增加现有delimiter builder的同类职责；剩余delimiter leaf完成后，应按既有记录提取cohesive `diagnostic_builder_delimiter_fixes`模块。

## 未完成边界

- 本leaf不自动重写assignment expression，也不为缺失array element合成表达式。
- 其他delimiter producer与delimiter replacement/mix尚未完成machine-fix收敛。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
