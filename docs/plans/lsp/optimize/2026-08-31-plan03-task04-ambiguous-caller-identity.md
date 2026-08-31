---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_calls.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_caller_identity_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.26: Ambiguous Caller Identity

## Goal

让call-edge caller只来自唯一的最窄function scope identity；冲突scope不得按append order任意
选择caller。

## Contract

- 严格更窄的function scope优先于外层scope。
- 同一owner SymbolId的等宽重复scope保持有效。
- 等宽scope的owner SymbolId不同即视为canonical producer冲突，caller fail closed为invalid，
  resolution为`CALLER_UNAVAILABLE`。
- resolved target仍可被`IncomingCalls`查询；冲突owner均不得获得`OutgoingCalls`结果。
- 不按scope name、AST、source text或publication order恢复caller。

## RED/GREEN

- RED：原28项全通过；新增equal-range/different-owner case错误归给first caller，calls
  `29 Tests / 1 Failure`、真实exit 1。
- GREEN：最窄scope选择记录owner冲突；GCC/Clang focused均`29/29`、真实exit 0。

## Verification

- 固定GCC snapshot：relations/query/symbols/calls/parity/source-contract
  `23/30/21/29/15/70`，全部真实exit 0。
- 固定Clang snapshot：同一六目标`23/30/21/29/15/70`，全部真实exit 0。
- 本子里程碑未运行MSVC、完整16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 08:30 +08:00。
- 状态：Task 4.26 focused GREEN；Plan 03总体仍进行中。
- 完成项目：ambiguous caller RED；唯一最窄scope identity；incoming/outgoing fail-closed回归；
  GCC/Clang六目标验证；module contract；Task4 caller-index checkbox完成。
- 未完成项目：CallAt receiver TypeId/member mapping；binary/native callable parity；Task7其余
  consumer；Task8 MSVC/完整16-target matrix/stdio smoke及总验收。
