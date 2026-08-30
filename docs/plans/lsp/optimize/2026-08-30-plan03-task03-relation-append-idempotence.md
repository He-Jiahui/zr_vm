---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_identity.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_identity.h
tests:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.14: Relation Append Idempotence

## Goal

让公共 relation append 在重复 producer 调用时保持 snapshot 幂等，同时保留同一稳定
SymbolId/TypeId 对应的多个精确定义位置。去重必须消费完整结构化 relation identity，不得按
名称、显示文本或单一 SymbolId 合并。

## Contract

- 完全相同的 kind、source/target SymbolId、source/target TypeId、range presence、精确
  source/target range、external 标志、external origin URI 与 virtual declaration URI 只存一条。
- 重复 append 仍返回成功，使幂等 producer 不需要把“已存在”当作错误处理。
- 任一 endpoint range 或 URI 不同的 relation 均保留；同一 SymbolId 的多定义不会被错误折叠。
- 完整 identity 比较提取到独立内部模块；接近 1000 行的 relation 编排文件不新增比较协议职责。
- `RelationsOfSymbol` 的排序、borrowed snapshot lifetime、scope filtering 与 external
  fail-closed 合同不变。

## RED/GREEN

固定测试先 append 两次完全相同的 declaration-definition edge，再 append 同一 stable ids
但 target range 不同的第二定义。旧实现真实 exit 1，`20 Tests / 1 Failure`，唯一失败为
`Expected 2 Was 3`。relation 层加入完整 identity 比较后，同一测试为 `20/20`、真实 exit 0，
并断言两个保留结果的 target offset 分别为 20 和 30；同一测试也覆盖相同 external origin
URI 幂等、不同 origin URI 保留。

## Verification

- 固定 GCC/Ninja snapshot：relations `20/20`、semantic query `30/30`、symbols `21/21`、
  calls `11/11`、LSP semantic-query parity `15/15`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一五目标分别为 `20/20`、`30/30`、`21/21`、`11/11`、
  `15/15`，全部真实 exit 0。
- GCC/Clang LSP source-contract 均为 `70 PASS / 0 FAIL`，真实 exit 0。
- 两套构建日志均无 warning/error；`git diff --check` 在提交前复核。
- 本阶段未运行 MSVC、完整三工具链 16-target matrix 或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 17:46 +08:00。
- 状态：Task 3.14 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：relation 完整结构身份比较与独立 identity 模块；重复 edge 幂等 append；
  多定义 range/不同 external origin 保留；parser RED/GREEN；GCC/Clang 五目标固定快照验证；
  模块与主计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、source/binary/native 完整 relation
  parity、semantic-token canonical migration、MSVC 与完整 16-target matrix、三套 stdio smoke、
  Plan 03 Task 8 总门禁。
