---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.6: Use-Site Type Display Alias Facts

## Goal

为需要source alias的consumer提供独立、snapshot-scoped display fact；canonical TypeId formatter
继续只输出结构 identity，不读取首个intern alias或AST文本。

## Contract

- fact键为 `(TypeId, exact FileRange)`；range包含offset、line/column与source identity。
- alias与source identity复制进semantic snapshot；query返回borrowed alias。
- 同键同alias发布幂等，同键不同alias拒绝；空alias、空source、未知TypeId拒绝。
- wrong TypeId、wrong source、shifted range与reset后的query均返回unavailable。
- canonical `FormatType`结果不因alias发布改变；producer与LSP consumer迁移不属于本子项。

## RED/GREEN

RED 先新增parser test调用publish/query API。旧代码在编译阶段报告implicit declaration，并在链接
阶段报告两个undefined reference，GCC target真实 exit 1。

GREEN 在semantic context增加独立alias fact array及init/reset/free生命周期，并实现exact-key
publish/query。测试同时证明同一use site查询`Index`、canonical文本仍为`int`、冲突/错identity
fail closed及reset清空。GCC/Clang semantic display均为 `11/11`、真实 exit 0。

## Verification

- GCC/Clang 固定 snapshot 均通过 display `11/11`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实 exit 0。
- canonical graph在 GCC/Clang均真实 exit 1，仅保留同一既有 legacy `pair():` syntax marker，
  delta 0且不计 GREEN。
- GCC/Clang interface进程均真实 exit 1，失败集合严格保持 fixed parent同一8个既有 producer
  marker，delta 0且不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 03:07 +08:00。
- 状态：Task 5.6 parser fact foundation GREEN；Plan 03 Task 4/Task 5/Task 7/Task 8 总门禁仍进行中。
- 完成项目：use-site type alias fact schema/API；snapshot copy与init/reset/free lifecycle；
  exact TypeId/source/range query；conflict/stale/reset fail-closed；GCC/Clang focused与expanded验证；
  graph/interface fixed-marker复核；模块、计划和子里程碑记录。
- 未完成项目：source alias producer、LSP alias consumer、owner全变体display gate、source
  receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、
  Plan 03 Task 8总门禁。
