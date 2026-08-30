---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.53: Dead Virtual Declaration Query Removal

## Goal

删除 native virtual-document 层全仓无调用的 module-link/type declaration name query，继续收敛
metadata adapter 的死公开 surface。保留实际被 navigation/metadata provider 使用的 type-member
与 position-based declaration query。

## Contract

- `ZrLanguageServer_LspVirtualDocuments_FindModuleLinkDeclaration` 不再声明或实现。
- `ZrLanguageServer_LspVirtualDocuments_FindTypeDeclaration` 不再声明或实现。
- `FindTypeMemberDeclaration`、`FindDeclarationAtPosition` 与 `ModuleEntryRange` 保持不变。
- virtual-document text/range仍由 structured native descriptor records生成；本阶段不新增
  module/type name fallback，也不改变 native/imported constructor adapter。

## RED/GREEN

现有 virtual-document source-contract 同时增加两个 API 的 source/header禁止项；固定旧生产
代码真实exit 1并精确报告四项。删除两个连续 wrapper及头声明后同一测试转GREEN；全仓两个
名称只剩四个source-contract禁止文本。

## Verification

- 固定 `2ad5abb + 3 code/test overlays` 的WSL GCC/Ninja快照完成language-server static
  library、source-contract与interface目标重链；source-contract 70/70，真实exit 0。
- GCC interface中的native network leaf、native declaration、native console descriptor、
  UTF-16 virtual range与native import definition五个case全部PASS。
- GCC interface真实exit 1，与固定parent保持同一8个已登记producer marker，delta 0；该目标
  不计本任务GREEN。
- 独立Clang/Ninja缓存完成language-server static library与source-contract重链；
  source-contract 70/70，真实exit 0。
- Clang重链报告metadata provider五个既有unused helper；该文件由Syntax05 Task4 exact-own，
  本任务未修改或暂存。
- `git diff --check`通过；本任务未执行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 17:14 +08:00。
- 状态：Task 7.53 focused GREEN；Plan 03 Task 7/Task 8总门禁仍进行中。
- 完成项目：无调用virtual declaration API审计；source-contract RED/GREEN；两个query wrapper
  及头声明删除；GCC/Clang fixed snapshot重链；GCC virtual-document case与interface marker
  复核；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer与metadata provider dead helper收口、
  native/imported constructor producer parity、source/binary/native完整relation parity、其余
  analyzer/symbol-table第二套语义删除、MSVC与完整三工具链16-target matrix、三套stdio smoke和
  Plan 03 Task 8总门禁。
