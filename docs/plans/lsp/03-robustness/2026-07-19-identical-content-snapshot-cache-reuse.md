---
plan_id: lsp-03-robustness
record_id: 2026-07-19-identical-content-snapshot-cache-reuse
status: completed
completed_at: 2026-07-19 19:17 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: identical-content-snapshot-cache-reuse
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
related_tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/stdio_smoke.js
---

# Identical-Content Snapshot And Semantic Cache Reuse

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 19:17 +08:00 | 已完成 | 相同内容document version更新复用text block、content generation、AST和semantic context；语义分析请求/执行/cache-hit可观测指标；真实内容变化的快照重建与缓存失效对照；GCC/Clang/MSVC十四目标矩阵和stdio/CLI JSON-RPC冒烟 |

## 已完成契约

- 已存在文档收到字节长度和内容完全相同的新版本时，只推进单调递增的document version，不分配新的text block，不增加content generation，也不标记parser dirty。
- LSP document update仍执行parse和semantic调用；未变快照复用原AST，并由现有AST hash、scope hash和scope range缓存键命中semantic cache。
- `SZrSemanticAnalysisMetrics` 分别记录有效分析请求、实际成功执行和cache hit数量，并保留最近一次实际执行范围，使增量行为能够由测试计数而不是指针结果推断。
- 内容变化继续创建新text block、增加content generation、重建AST并执行语义分析；analyzer实例生命周期保持稳定。

## TDD与验证证据

- 编译RED：新测试最初因缺失`SZrSemanticAnalysisMetrics`和`ZrLanguageServer_SemanticAnalyzer_GetMetrics`失败。
- 运行RED：仅加入指标后，相同内容v1到v2更新得到`blockSame=0`、generation `2/1`、`astSame=0`、execution `2/1`和hits `0/0`；变化内容对照保持通过。
- MSVC契约RED：测试直接引用内部`UpdateDocumentCore`导致`LNK2019`；测试改走公共`ZrLanguageServer_Lsp_UpdateDocument`，未扩大内部符号导出面。
- GREEN：GCC、Clang和MSVC定向`zr_vm_language_server_lsp_interface_test`均为79/79。
- 回归：三套工具链分别运行相同十四目标矩阵，均为`run=14 failures=0`，且没有`Fail -`或`:FAIL:`标记；各自的`language_server_stdio_smoke`均为1/1通过。
- 稳定计数套件包括semantic query 16/16、compiler query diagnostics 16/16、parser 75/75、expression facts 28/28、type inference 118/118和dataflow 9/9；其余LSP/semantic目标全部通过。

## 未完成边界

- 本记录只消除相同内容version更新造成的无效快照和语义重算，不表示L6完成。
- 真实body edit仍按整文件重建AST并失效语义缓存；声明级change range、function CFG/query cache最小失效、signature/import依赖传播尚未实现。
- stale version拒绝、cancellation、immutable snapshot race、历史快照上限、provider parity、p50/p95/p99和峰值内存门槛仍待后续子里程碑完成。
- 冻结快照为编译当前`HEAD`引用的枚举使用了未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
