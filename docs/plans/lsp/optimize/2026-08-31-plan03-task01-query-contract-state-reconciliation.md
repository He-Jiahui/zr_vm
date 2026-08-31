---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
tests:
  - tests/parser/test_semantic_query_contract.c
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-24-plan03-task01-query-purity.md
doc_type: milestone-record
---

# Plan 03 Task 1.1: Query Contract State Reconciliation

## Goal

修复主计划Task1 checkbox与既有完成record之间的状态漂移，不重复实现已完成的query contract。

## Audit Result

`2026-08-24-plan03-task01-query-purity.md`已经记录Task1完成，并冻结以下合同：

- TypeAt/CanonicalTypeAt/CallAt/Definition/Declaration/References/Diagnostics/PropertyAt的
  source/binary/native parity。
- pointer结果为semantic snapshot borrowed view，跨snapshot只复制stable ids/ranges。
- read-only query不物化compiler/analyzer状态，重复调用保持稳定。
- UNKNOWN/APPROXIMATE exactness fail closed，不允许LSP text reconstruction。

主计划仍保留四个未勾选项，属于记录漂移，不是实现缺口。本项将四项改为完成。

## Verification

- 当前GCC/Clang独立snapshot的`zr_vm_semantic_query_contract_test`均`4/4`、真实exit 0。
- 当前GCC/Clang的`zr_vm_language_server_semantic_query_parity_test`均`15/15`、真实exit 0。
- 原Task1 record保留当时GCC/Clang/MSVC三工具链证据；本对账项未重跑MSVC。
- 未运行完整16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 08:03 +08:00。
- 状态：Task 1状态对账完成；Plan 03总体仍进行中。
- 完成项目：四个Task1 checkbox对齐；borrowed ownership、query purity、repeat stability、exactness
  fail-closed和source/binary/native parity证据复核；独立状态记录。
- 未完成项目：Task4 receiver/member与binary/native producer；Task7 consumer迁移；Task8
  MSVC/完整16-target matrix/stdio smoke及总验收。
