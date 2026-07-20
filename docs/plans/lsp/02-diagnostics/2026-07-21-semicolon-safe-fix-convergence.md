---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/stdio/stdio_lsp_memory.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/stdio/stdio_lsp_memory.c
plan_sources:
  - user: strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_diagnostic_safe_fix_cases.h
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_diagnostic_fix_smoke.js
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-07-21-semicolon-safe-fix-convergence
status: completed
completed_at: 2026-07-21 04:30 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: semicolon-safe-fix-convergence
---

# Semicolon Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-21 04:30 +08:00 | 已完成 | `missing_statement_semicolon`从parser primary/fix双range、machine-applicable edit、LSP quickfix到snapshot resolve和apply-edit-rebind闭环；EOF变量声明与line-comment前插入使用lexer token identity，block comment和placeholder fix不生成自动action；三工具链十八目标矩阵、三套stdio/CLI与三套diagnostic-fix smoke真实exit 0、marker为0 |

## 已实现契约

- structured diagnostic builder保留用户可理解的primary location，并独立接收零宽fix location；`;` fix标记为`ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`。
- parser从同一source和lexer规则重放到primary位置之前的最后一个完整token，使用exact token end作为插入点。line/block comments和空白不由LSP扫描。
- 普通variable declaration在EOS也发布`missing_statement_semicolon`；for-header路径不改变。
- LSP删除`var`、`let`、`return`和`%import`源码前缀启发式，只消费`GetDiagnostics`中的machine-applicable structured fix。placeholder fix保留在diagnostic JSON但不提升为quickfix。
- caret/selection只按structured diagnostic/fix range与行区间匹配。quickfix继续使用code-action fingerprint，旧version resolve删除edit并disabled。
- 新公共`ZrLanguageServer_Lsp_FreeDiagnostics`统一in-process和stdio consumer的diagnostic数组所有权释放。

## TDD与根因证据

- Parser RED：新增builder断言后只有新case失败，原因是`diagnostic.fixes.isValid=false`；builder增加machine fix后19/19 GREEN。
- LSP RED：新增machine/placeholder case后machine case失败，placeholder负边界通过；fact consumer接入后新case通过，但两个旧semicolon case暴露fix仍指向next token/EOS。
- 精确range根因是parser原contract只有primary location。builder拆分primary/fix range后，parser token重放使`break`和`return answer // note`分别落在真实前一token end。
- EOF RED：无尾随newline的`var answer = 42`没有diagnostic，因为variable declaration显式排除EOS；移除普通声明的EOS豁免后parser diagnostics与advanced editor均GREEN。
- stdio首次apply/rebind断言取到排队中的version 2 notification；测试改为等待同URI且`version=3`，随后确认fixed source不再包含该code。生产行为未为通知顺序增加兼容。

## 工具链与回归证据

- 正式隔离source为稳定`HEAD 588c7903614d5bb4d069d91aa32ac2244d1ed3eb`加14个code/test exact overlays；14个文件与共享工作树SHA-256逐一一致，Syntax 03 M3中间态未进入快照。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228 / `VSCMD_VER=17.14.36`使用独立static cache。
- 每套执行相同18个测试目标并取得18/18真实process exit 0；共54个非空测试日志，`Fail -`、`FAIL:`和`:FAIL:` marker为0。
- 三套parser builder均19 Tests / 0 Failures；三套advanced editor均41项 / 0 failure；三套parser diagnostics均完成全部case。
- GCC、Clang和MSVC分别运行主`stdio_smoke.js`与`stdio_diagnostic_fix_smoke.js`，共6个Node进程真实exit 0。空stdout日志只与真实process exit配对计证据。

## Snapshot、Schema与协议边界

- 本阶段不改变semantic query generation、artifact/binary schema、public-contract hash或cache key。`SZrStructuredDiagnostic`/fix结构尺寸不变；只扩展builder函数参数并消费既有fix数组。
- code-action opaque snapshot schema不变，沿用version/generation/open-state/length/hash。stale resolve仍使用统一disabled reason。
- Protocol diagnostic继续在`Diagnostic.data.fixes`序列化applicability和单edit；quickfix producer不读取英文message/code/title来重建edit。

## 未完成边界

- delimiter、invalid module specifier、legacy migration以及ownership/property/module等其他diagnostic fix尚未完成registry-wide applicability收敛。
- 本阶段只覆盖单document局部`;`插入；跨文件/manifest、多edit fix仍需独立checksum/version计划。
- 没有采集p50/p95/p99、峰值内存、cancellation latency或100次乱序race，因此不晋级完整L3/L6。
