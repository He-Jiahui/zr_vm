---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
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
record_id: 2026-07-21-array-close-safe-fix-convergence
status: completed
completed_at: 2026-07-21 08:27 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: array-close-safe-fix-convergence
---

# Array-Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 08:27 +08:00 | 已完成 | `missing_array_close`从exact opening-token primary与current-token fix双range到零宽`]` machine edit、通用LSP quickfix和stdio apply/rebind清除闭环；producer不再用post-token cursor构造primary，consumer不按array AST/code/message/source重建；GCC/Clang/MSVC十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingArrayClose`接受opening primary `location`和独立`fixLocation`；先构造既有structured diagnostic，再把fix range收敛到`fixLocation.start`并发布`Insert missing ']'`、edit text `]`、`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- `parse_array_literal`在消费`[`前用`get_current_token_location`捕获exact opening range。`report_missing_array_close`保留该primary，同时读取parser current token作为fix range。
- `return [1, 2`的primary保持line 0 characters 7..8，edit只在EOF character 12插入`]`，不覆盖opening bracket或任何array element。Version 2使用`return [1, 2];`并清除该code。
- Fix分配失败会释放已构造的diagnostic并返回失败，不发布半初始化fact。
- LSP production无需新增array-specific逻辑；既有structured-fix consumer复制title/range/text并复用code-action snapshot，stdio从同一fix数组序列化`Diagnostic.data.fixes`。

## TDD与根因证据

- Parser API RED真实编译失败，新增dual-range builder case对旧单range API产生`too many arguments to function`，证明primary不能兼作EOF edit。
- Advanced-editor RED为47项 / 1 failure；新增array case无法取得精确machine-applicable quickfix，其余46项通过。
- Stdio RED真实exit 1；该code已发布，但实际primary为character 9..9而非opening characters 7..8，证明根因位于parser producer range而非协议转换层。
- Support-first审计确认`parse_array_literal`使用`get_current_location`读取lexer post-token cursor。改为exact current-token range并补齐builder fix后，parser builder为25 Tests / 0 Failures，advanced editor为47项0 failure，stdio diagnostic-fix smoke真实exit 0。

## 工具链与回归证据

- 正式隔离source为`HEAD b9d73a20674bb416ad7553b411edf8356cdf9564`加8个code/test exact overlays；并发Syntax 03 M4中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用指向同一source snapshot的独立static cache；public header和parser source变化后每套均完整重建受影响依赖。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；54个测试日志全部非空，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均25 Tests / 0 Failures；三套advanced editor均47项 / 0 failure。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。

## Snapshot、Schema与协议边界

- 本阶段不改变diagnostic registry、semantic query generation、artifact/binary schema、public-contract hash或cache key。
- `SZrStructuredDiagnostic`与fix结构不变；builder public API只增加独立fix range输入，并填充已有fix数组。
- Code-action snapshot opaque data和resolve规则不变；本leaf复用已验收的version/generation/open-state/length/hash门禁。
- 8个code/test owned路径与正式隔离source逐文件内容一致；共享工作树中的Syntax 03 M4路径未进入测试或提交范围。

## 未完成边界

- `missing_object_close`及其他delimiter producer尚未完成machine-fix收敛；delimiter replacement/mix仍需独立语义边界。
- Registry-wide applicability审计、migration edit、多document fix和workspace command仍未完成。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
