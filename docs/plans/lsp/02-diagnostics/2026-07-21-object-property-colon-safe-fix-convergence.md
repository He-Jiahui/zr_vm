---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
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
record_id: 2026-07-21-object-property-colon-safe-fix-convergence
status: completed
completed_at: 2026-07-21 10:48 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: object-property-colon-safe-fix-convergence
---

# Object Property-Colon Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 10:48 +08:00 | 已完成 | `missing_object_property_colon`在首属性与后续属性producer间保留unexpected value token primary，并在同token起点发布零宽`:` machine edit，完成通用LSP quickfix/stdio apply-rebind闭环；consumer不按property位置/object AST/code/message/source重建；稳定`HEAD=3fc566e`上的GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingObjectPropertyColon`先构造既有structured diagnostic，保留unexpected value token primary，再把临时edit range收敛到`location.start`并发布`Insert missing ':'`、edit text `:`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- `parse_object_literal`的首属性与后续属性分支已经在`consume_token(ZR_TK_COLON)`失败后传入current value token range；本阶段复用同一canonical reporter，不增加property-position分流。
- `return {a 1};`报告line 0 characters 10..11/fix 10；`return {a: 0, b 1};`报告characters 16..17/fix 16。
- Fix分配失败会释放已构造的diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增property-colon特判；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。

## TDD与根因证据

- Parser builder RED为28 Tests / 1 Failure，唯一新增property-colon case在`diagnostic.fixes.isValid`断言失败。
- Advanced-editor RED为51项 / 1 failure，唯一新增后续属性case没有machine action；stdio RED真实exit 1并精确停在`Expected one serialized property-colon diagnostic fix`。
- Support-first审计确认两个object-property producer和shared reporter已经传递正确value token range，缺口只在builder未发布fix，因此没有修改parser语法选择、reporter或LSP consumer。
- 最终advanced-editor用后续属性producer冻结characters 16..17/fix 16，stdio用首属性producer冻结characters 10..11/fix 10并验证version 2应用`:`后清除该code。
- Focused GREEN为parser builder 28 Tests / 0 Failures、advanced editor 51项 / 0 failure、stdio diagnostic-fix smoke真实exit 0。

## 工具链与回归证据

- Syntax 03 M4在初轮验收后提交32条parser/core/AOT/CMake路径，因此初轮`HEAD=66e6805`矩阵被取代，不作为最终基线证据。
- 最终隔离source为`HEAD 3fc566ed59559788f3116c723ab410ef237d72a9`的32条commit paths加5个property-colon code/test overlays；32个commit blob与5个overlay SHA均逐文件匹配，并发Syntax 03 M5中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`重新配置指向同一source snapshot的独立static cache，并在刷新mtime后完整重建新M4 source/CMake图及20个验收目标。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均28 Tests / 0 Failures；三套advanced editor均51项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`、fix结构和builder public API签名均不变；builder只填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。
- `diagnostic_builder.c`已超过约1100行，但本leaf只增加现有delimiter builder的同类职责；剩余delimiter leaf完成后，应按既有记录提取cohesive `diagnostic_builder_delimiter_fixes`模块，避免继续增长该文件。

## 未完成边界

- `missing_object_property_separator`及其他delimiter producer与delimiter replacement/mix尚未完成machine-fix收敛。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
