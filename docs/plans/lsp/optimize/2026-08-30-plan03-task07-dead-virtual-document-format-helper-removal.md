---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.50: Dead Virtual Document Format Helper Removal

## Goal

删除 native virtual-document builder 中全仓零调用的 varargs format helper，继续收敛 LSP
consumer/adapter 层不可达 surface。保留仍用于 native/imported declaration projection 的
structured descriptor renderer，不提前迁移计划明确等待 producer parity 的 constructor adapter。

## Contract

- `virtual_builder_append_format` 不再作为不可达格式化入口存在。
- 仅由该 helper 使用的 `<stdarg.h>` 依赖一并删除。
- 活跃 renderer 继续通过 `virtual_builder_append_text`、identifier range recorder 与 structured
  native descriptor metadata 生成 virtual document；不新增 name/type-text fallback。
- native/imported constructor structured metadata adapter 保持不变，等待 parser producer parity。

## RED/GREEN

source-contract 增加 dead format helper 禁止项，固定旧生产代码真实 exit 1并精确报告
`virtual_builder_append_format` 一项。删除 helper 与独占 include 后同一测试转 GREEN；全仓该名称
只剩 source-contract 禁止文本。

## Verification

- 固定 `eaad830 + 2 code/test overlays` 的 WSL GCC/Ninja 快照完成 language-server static
  library、source-contract 与 interface 目标重链；source-contract 70/70，真实 exit 0。
- GCC interface 中 native network leaf、native declaration、native console descriptor、UTF-16
  virtual range 与 native import definition 五个 virtual-document/navigation case 全部 PASS。
- GCC interface 真实 exit 1，失败测试名称与 Task 7.47 parent 的 8 个已登记 producer marker
  完全一致，delta 0；该目标不计本任务 GREEN。
- 独立 Clang/Ninja 静态缓存完成 language-server static library 与 source-contract 重链；
  source-contract 70/70，真实 exit 0。
- `git diff --check` 通过；本任务未执行 MSVC、完整三工具链 16-target matrix 或三套 stdio
  smoke。

## 状态与产出记录

- 完成时间：2026-08-30 16:44 +08:00。
- 状态：Task 7.50 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：零调用 helper 审计；source-contract RED/GREEN；dead varargs format helper 与独占
  include 删除；GCC/Clang 固定快照重链；GCC virtual-document cases 与 interface marker 复核；
  计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、native/imported constructor
  producer parity、source/binary/native 完整 relation parity、其余 analyzer/symbol-table 第二套
  语义删除、MSVC 与完整三工具链 16-target matrix、三套 stdio smoke 和 Plan 03 Task 8 总门禁。
