---
related_code:
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_display_owner_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.16: Owner Variant Display Acceptance

## Goal

用semantic display public API冻结全部canonical owner variants，关闭Task 5中ref/owner/readonly
display覆盖的最后验收缺口。

## Contract

- `ZR_CANONICAL_OWNER_UNIQUE`输出`Unique<int>`。
- `ZR_CANONICAL_OWNER_SHARED`输出`Shared<int>`。
- `ZR_CANONICAL_OWNER_WEAK`输出`Weak<int>`。
- `ZR_CANONICAL_OWNER_ATOMIC_SHARED`输出`AtomicShared<int>`。
- 四种输出均由同一inner TypeId和semantic display API生成，不经LSP或source text formatter。
- invalid owner仍由既有composite integrity测试fail closed。

## Characterization

新增独立owner cases header，避免继续扩大接近1000行的主semantic display test。测试对同一canonical
`int` TypeId依次intern四种owner并调用`ZrParser_SemanticDisplay_FormatType`。GCC/Clang首轮均为
`22 Tests / 0 Failures`，现有production已满足计划合同，因此本项不引入无意义代码改动。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `22/22`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity/source-contract真实exit 0、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 06:16 +08:00。
- 状态：Task 5.16 owner variant display acceptance GREEN；Task 5仅剩LSP旧名称映射删除，Plan 03
  Task 7/Task 8总门禁仍进行中。
- 完成项目：Unique/Shared/Weak/AtomicShared semantic display gate；large-test模块化；GCC/Clang
  focused及expanded验证；graph/interface fixed-marker复核；主计划checkbox、模块说明和子里程碑记录。
- 未完成项目：LSP `semantic_type_prototypes.c`旧名称映射删除、consumer迁移、source
  receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
