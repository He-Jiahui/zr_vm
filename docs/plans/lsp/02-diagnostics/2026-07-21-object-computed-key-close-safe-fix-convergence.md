---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
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
record_id: 2026-07-21-object-computed-key-close-safe-fix-convergence
status: completed
completed_at: 2026-07-21 09:59 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: object-computed-key-close-safe-fix-convergence
---

# Object Computed-Key Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 09:59 +08:00 | 已完成 | `missing_object_computed_key_close`在首属性与后续属性producer间统一exact opening-token primary与current-token fix双range，发布零宽`]` machine edit并完成通用LSP quickfix/stdio apply-rebind闭环；consumer不按property位置/object AST/code/message/source重建；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose`接受opening primary `location`和独立`fixLocation`；先构造既有structured diagnostic，再把fix range收敛到`fixLocation.start`并发布`Insert missing ']'`、edit text `]`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- `parse_object_literal`的首属性与后续属性分支都已在消费`[`前用`get_current_token_location`保存exact opener；本阶段复用这些canonical producer ranges，不修改或按property位置区分它们。
- `report_missing_object_computed_key_close`保留producer primary并读取parser current token作为fix range。`return {[1: 2};`报告line 0 characters 8..9/fix 10；`return {a: 0, [1: 2};`报告characters 14..15/fix 16。
- Fix分配失败会释放已构造的diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增computed-key特判；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。

## TDD与根因证据

- Parser API RED真实编译失败，新增dual-range builder case对旧单range API产生`too many arguments to function`。
- Advanced-editor RED为50项 / 1 failure，唯一新增computed-key case没有machine action；stdio RED真实exit 1并精确停在`Expected one serialized computed-key close diagnostic fix`。
- Support-first审计确认两个object-literal computed-key producer已经保存exact opening token，缺口只在shared builder/reporter仍使用单range且不发布fix，因此没有修改parser语法选择或LSP consumer。
- 最终advanced-editor用后续属性producer冻结characters 14..15/fix 16，stdio用首属性producer冻结characters 8..9/fix 10并验证version 2应用`]`后清除该code。
- Parser builder最终为27 Tests / 0 Failures，advanced editor为50项 / 0 failure，stdio diagnostic-fix smoke真实exit 0。

## 工具链与回归证据

- 正式隔离source内容为`HEAD f35d650e063fd9f9931a0df3f6416ab9e7deb867`加7个code/test exact overlays；并发Syntax 03 M4中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用指向同一source snapshot的独立static cache；public header和parser source变化后每套均重建受影响依赖。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均27 Tests / 0 Failures；三套advanced editor均50项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`与fix结构不变；builder public API只增加独立fix range输入，并填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。
- 7个code/test owned路径与正式隔离source逐文件内容一致；共享工作树中的Syntax 03 M4路径未进入测试或提交范围。
- `diagnostic_builder.c`与`parser_state.c`已经超过约1100行，但本leaf只增加现有delimiter builder/reporter的同类职责；在delimiter family尚未收口时迁移全部peer会扩大public/internal API与三工具链重建范围。完成剩余delimiter leaf后，应分别提取cohesive `diagnostic_builder_delimiter_fixes`与`parser_delimiter_diagnostics`模块，避免继续增长这两个文件。

## 未完成边界

- 其他delimiter producer与delimiter replacement/mix尚未完成machine-fix收敛。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
