---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
doc_type: milestone-record
---

# Plan 03 Task 7.46: Analyzer Completion Surface Removal

## Goal

删除 canonical `VisibleSymbols` completion consumer 接管后已无生产调用的 analyzer
completion API 与实现闭包，继续收敛 LSP 自有 symbol-scope 语义。保留 canonical completion
projector、parser query 与独立 parity 测试，不把旧 analyzer 测试迁成新的兼容入口。

## Contract

- `ZrLanguageServer_SemanticAnalyzer_GetCompletions` 不再是公开或生产 API。
- lexical completion 继续只通过 `ZrParser_SemanticQuery_VisibleSymbols` 与
  `lsp_canonical_completion.c` 投影，不读取 analyzer symbol table。
- analyzer 中仅为旧入口存在的 symbol/native/import completion formatter、去重器和递归扫描
  全部删除；不保留不可达 helper 或容量常量。
- 混合 analyzer 测试保留 hover、type、diagnostic 与 symbol-scope 验证，只有旧 completion
  调用和断言被删除。

## RED/GREEN

canonical completion source-contract 增加 analyzer header/source 禁止项。旧代码固定快照真实
exit 1，精确报告两处 `ZrLanguageServer_SemanticAnalyzer_GetCompletions`。删除声明、实现和
helper 闭包后同一 source-contract 转 GREEN，仓库生产/测试中该名称只剩三处禁止文本。

## Verification

- 固定 `eb77fae + 5 code/test overlays` 的独立 WSL GCC/Ninja 快照完成 analyzer、
  source-contract、semantic parity 与 interface 四目标重链；interface仅作编译/链接验证。
- GCC source-contract 69/69、semantic parity 15/15，均真实 exit 0。
- 独立 Clang/Ninja 静态缓存完成同一三个运行目标重链；source-contract 69/69、semantic
  parity 15/15，均真实 exit 0。
- GCC/Clang analyzer 均为 65 Pass/2 Fail；失败仍精确为已登记的 closed-generic receiver
  与 owner-generic producer marker。移除的三项 PASS 恰好是只调用死 API 的纯 completion
  tests，未产生新 marker。
- `git diff --check`通过；本任务未执行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 15:47 +08:00。
- 状态：Task 7.46 focused GREEN；Plan 03 Task 7/Task 8总门禁仍进行中。
- 完成项目：零生产调用与helper闭包审计；source-contract RED/GREEN；公开API/生产实现删除；
  三项纯死测试及九项混合测试completion分支清理；失效命名映射删除；GCC/Clang固定快照
  重链与focused runtime验证；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、source/binary/native完整relation
  parity、其余symbol-table/analyzer第二套语义删除、MSVC与完整三工具链16-target matrix、
  三套stdio smoke和Plan 03 Task 8总门禁。
