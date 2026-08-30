---
related_code:
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.5: Nominal Display Identity Integrity

## Goal

让 nominal interning 与 canonical formatting共用非空 type-name合法域；不得创建有效 TypeId后
成功输出空 label，也不得授权 LSP 从 token/name fallback补齐。

## Contract

- `ZrParser_CanonicalType_InternNominal` 要求 name非空，空 name不保留 TypeId。
- 空 module identity仍表示未限定 nominal，不与空 name混淆。
- formatter在输出前再次验证 stored name非空，损坏 snapshot返回 false并清空 buffer。
- alias与canonical identity仍必须由后续显式 display fact分离，不从源码猜测缺失名称。

## RED/GREEN

RED 直接调用公共 nominal interner创建空 name。旧实现返回有效 TypeId `1`，GCC focused真实
exit 1、`10 Tests / 1 Failure`、`Expected 0 Was 1`。同一测试还把合法 nominal snapshot的
stored name破坏为空，要求 formatter fail closed。

GREEN 在 interner reservation前拒绝空 name，并在 nominal formatter重复验证 snapshot。
GCC/Clang semantic display均为 `10/10`、真实 exit 0。

## Verification

- GCC/Clang 固定 snapshot 均通过 display `10/10`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实 exit 0。
- canonical graph在 GCC/Clang均真实 exit 1，仅保留同一既有 legacy `pair():` syntax marker，
  delta 0且不计 GREEN。
- GCC/Clang interface进程均真实 exit 1，失败集合严格保持 fixed parent同一8个既有 producer
  marker，delta 0且不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 02:54 +08:00。
- 状态：Task 5.5 focused GREEN；Plan 03 Task 4/Task 5/Task 7/Task 8 总门禁仍进行中。
- 完成项目：nominal interner empty-name gate；nominal formatter corrupt-snapshot gate；
  formatter buffer fail-closed；GCC/Clang focused与expanded验证；graph/interface fixed-marker复核；
  模块、计划和子里程碑记录。
- 未完成项目：owner全变体与use-site alias display fact、source receiver/member argument mapping、
  receiver `TypeId`、`.zro`/native callable mapping parity、Task 5 consumer收口、Syntax05 imported
  property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
