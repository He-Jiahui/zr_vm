---
plan_id: lsp-03-robustness
record_id: 2026-07-19-minimal-change-range-and-declaration-classification
status: completed
completed_at: 2026-07-19 20:12 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: minimal-change-range-and-declaration-classification
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_change.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_change.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_change.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
related_tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/stdio_smoke.js
---

# Minimal Change Range And Declaration Classification

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 20:12 +08:00 | 已完成 | 真实内容变化的old/new最小byte range测量；基于匹配旧文本AST的module、declaration signature、declaration body分类；声明owner类型和范围记录；边界插入、fallback AST与重复分类保守降级；GCC/Clang/MSVC十四目标矩阵和stdio/CLI JSON-RPC冒烟 |

## 已完成契约

- 真实内容更新先计算最长公共byte prefix和不重叠的最长公共byte suffix，得到精确的旧内容范围和新内容范围；删除、插入和长度变化都保留各自坐标，不再用整文件范围代替实际变化范围。
- LSP update在释放旧AST之前完成分类。分类结果只保存声明类型和旧声明范围，不保存会在parse后悬空的旧AST指针。
- 函数、class/struct method、meta function、test、lambda和property accessor的body内部变化标记为`DECLARATION_BODY`；声明内但body外的变化标记为`DECLARATION_SIGNATURE`；无法证明声明归属或位于声明边界的插入保守标记为`MODULE`。
- 零长度插入触及声明根边界时不归入该声明，触及body边界时不归入body；重复分类和模块级退化会清空旧的声明类型、范围和`hasDeclaration`状态。
- `usesFallbackAst`表示AST对应更早的last-good内容；此时当前文本change range不得映射到该AST，分类保持`MODULE`。
- `lastChangeRange`继续提供兼容的新内容范围视图，完整old/new范围和影响分类由`lastChangeInfo`提供。

## TDD与验证证据

- 编译RED：新增测试最初因`lastChangeInfo`、影响枚举和分类API不存在而无法编译。
- 运行RED：body edit首先通过；signature edit暴露真实最小差异应为`in -> floa`，因为旧、新类型共享末尾`t`，测试按byte diff契约修正而未放宽实现。
- 边界RED：分类器对同一结构重复分类并退化到module时保留旧声明类型和范围；显式清理元数据后接口套件恢复GREEN。
- fallback AST RED：语法错误快照保留last-good AST后，恢复编辑曾被旧AST错误分类为body；增加snapshot匹配守卫后保持module级。
- 聚焦GREEN：WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228的`zr_vm_language_server_lsp_interface_test`均为83/83。
- 最终回归：三套工具链分别运行相同十四目标矩阵，均为`run=14 failures=0`且无`Fail -`或`:FAIL:`标记；各自的`language_server_stdio_smoke`均为1/1。
- 稳定计数套件包括semantic query 16/16、compiler query diagnostics 16/16、parser 75/75、expression facts 28/28、type inference 118/118、dataflow 9/9和interface 83/83；其余矩阵目标全部通过。

## 未完成边界

- 本记录建立变更测量和分类事实，不表示partial reparse或L6完成；任何真实内容变化仍重建整文件AST并按现有路径执行语义分析。
- `DECLARATION_BODY`尚未接入owning function CFG/query cache的局部失效，`DECLARATION_SIGNATURE`和`MODULE`也尚未接入direct callers、ModuleIdentity和reverse dependency传播。
- stale version拒绝、cancellation、immutable snapshot race、历史快照上限、provider parity、p50/p95/p99和峰值内存预算仍待后续子里程碑。
- 冻结快照编译当前LSP变更时使用未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
