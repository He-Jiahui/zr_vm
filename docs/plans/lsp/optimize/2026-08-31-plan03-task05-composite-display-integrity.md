---
related_code:
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.3: Composite Display Integrity

## Goal

让 generic parameter/instance、array 与 union 的 canonical formatter 对齐 intern 合法域；
损坏 snapshot 不得被格式化为看似有效的 identity。

## Contract

- generic parameter 必须携带非 invalid owner SymbolId。
- generic instance argument list 必须非空；未知 argument kind 继续 fail closed。
- array rank 必须大于0，storage kind 必须是 managed、inline 或 native。
- union variant list 必须非空，definition/variant TypeId 继续递归验证。
- 任何 composite shape 校验失败都清空 formatter buffer，不授权 LSP fallback。

## RED/GREEN

RED 从合法 interned nodes 依次注入 invalid generic owner、空 generic arguments、rank 0、unknown
array storage 与空 union variants。旧 formatter在首个 invalid owner 即返回成功，GCC focused真实
exit 1、`8 Tests / 1 Failure`、`Expected FALSE Was TRUE`。

GREEN 在各 formatter branch 复用 intern 的合法域。完整测试执行全部五种损坏形态，GCC/Clang
semantic display 均为 `8/8`；正常 const/type generic、array 与 union 文本保持不变。

## Verification

- GCC/Clang 固定 snapshot 均通过 display `8/8`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实 exit 0。
- canonical graph 在 GCC/Clang 均为 `18 Pass / 1` 个相同既有 legacy `pair():` syntax marker；
  该目标不计 GREEN。
- GCC/Clang interface 进程均真实 exit 1，失败集合严格保持 fixed parent 同一8个 producer
  marker，delta 0且不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 02:27 +08:00。
- 状态：Task 5.3 focused GREEN；Plan 03 Task 4/Task 5/Task 7/Task 8 总门禁仍进行中。
- 完成项目：generic owner integrity；generic argument cardinality；array rank/storage integrity；
  union variant cardinality；formatter buffer fail-closed；GCC/Clang expanded、type-inference与
  interface marker复核；模块、计划和子里程碑记录。
- 未完成项目：owner/function全变体display gate、source/use-site alias display fact、source
  receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Task 5 consumer收口、Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
