---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_import_chain.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.49: Dead Semantic Text Helper Removal

## Goal

删除全仓零调用且编译器已报告 unused 的 property AST 文本提取与 import-chain string 文本
提取 helper，继续清除 LSP 第二套语义留下的不可达文本入口。保留仍被活跃结构化 producer/
consumer 使用的通用 typecheck 与 import-chain 逻辑。

## Contract

- `semantic_member_property_text` 不再从 member AST 提取 property spelling。
- `semantic_identifier_node_text` 随唯一调用者删除，不保留不可达 identifier AST 文本入口。
- `semantic_import_chain_string_text` 不再作为未调用的 `SZrString` 文本转换入口。
- 活跃 import-chain resolution 继续消费 structured module/member binding；typecheck 继续消费
  parser/compiler query facts，本阶段不增加 name/token/type-text fallback。

## RED/GREEN

source-contract 首轮增加 property/import-chain helper 禁止项，旧固定快照真实 exit 1并精确报告
两项。删除后重链暴露 `semantic_identifier_node_text` 成为尾随 `-Wunused-function`；follow-up
source-contract 再精确 RED 一项，删除后最终转 GREEN。全仓三个名称只剩禁止文本。

## Verification

- 固定 `c575d8a + 3 code/test overlays` 的 WSL GCC/Ninja 快照完成 LSP static library、
  source-contract、semantic parity、semantic analyzer 与 interface 五目标重链；三个 helper
  的 GCC unused warning 全部消失。
- GCC source-contract 69/69、semantic parity 15/15，均真实 exit 0；analyzer 真实 exit 1，
  仍只有 closed-generic receiver 与 owner-generic 两个已登记 producer marker。
- 独立 Clang/Ninja 静态缓存完成同一五目标重链；source-contract 69/69、semantic parity
  15/15，均真实 exit 0；analyzer 与 GCC 保持同一两个 marker。
- GCC interface 真实 exit 1，失败测试名称与 Task 7.48 parent 的 8 个已登记 producer
  marker 完全一致，delta 0；该目标不计本任务 GREEN。
- `git diff --check` 通过；本任务未执行 MSVC、完整三工具链 16-target matrix 或三套
  stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 16:28 +08:00。
- 状态：Task 7.49 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：零调用/unused warning 审计；两轮 source-contract RED/GREEN；三个 dead text
  helper 删除；GCC/Clang 固定快照重链、focused runtime 与 marker 复核；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、source/binary/native 完整
  relation parity、其余 analyzer/symbol-table 第二套语义删除、MSVC 与完整三工具链
  16-target matrix、三套 stdio smoke 和 Plan 03 Task 8 总门禁。
