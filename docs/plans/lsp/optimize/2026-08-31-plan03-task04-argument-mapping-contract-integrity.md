---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
tests:
  - tests/parser/test_semantic_query_call_argument_mapping_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.23: Argument Mapping Contract Integrity

## Goal

让`CallAt`只投影与selected callable canonical parameter contract一致的non-empty argument
mapping，避免stale/corrupt snapshot用另一个有效TypeId、错误passing mode或自相矛盾的conversion
污染LSP consumer。

## Contract

- `parameterIndex`必须定位selected callable的canonical parameter contract。
- dense rows中同一parameter最多绑定一次；default/未提供parameter无需伪造row。
- `parameterTypeId`必须等于contract TypeId；non-value passing可使用canonical ref wrapper的pointee。
- mapping passing mode必须对应contract passing form；`ref readonly`在现有public enum中映射为`REF`。
- argument TypeId必须存在于同一semantic snapshot。
- TypeId相等时conversion只能为EXACT，不等时只能为IMPLICIT；UNKNOWN一律fail closed。
- 任何row违反合同都清空`SZrParserSemanticCallQuery`并返回false，不选择名称或文本fallback。

## RED/GREEN

RED复用source named reorder call的合法dense mapping，先把`arg0 -> param1(double)`的
`parameterTypeId`替换为`param0(int)`的另一个有效TypeId。旧query真实exit 1，
`25 Tests / 1 Failure`，唯一失败为`Expected FALSE Was TRUE`。同一测试随后还注入错误REF
passing mode和“相等TypeId + IMPLICIT”矛盾，约束三类损坏都必须清零输出。

第二个RED将第二条row改为同一parameter，并同步为自洽的TypeId/IMPLICIT conversion；旧query仍
真实exit 1、`25 Tests / 1 Failure`、`Expected FALSE Was TRUE`，证明逐row校验不足以保证binding
唯一性。

GREEN从selected callable的parameter contract校验row；value参数要求exact contract TypeId，
non-value参数仅额外接受canonical ref wrapper的pointee。argument canonical type存在性、passing
form、parameter唯一性与conversion一致性在同一门禁完成。合法source mapping保持通过。

## Verification

- GCC/Clang固定snapshot均通过calls `25/25`、semantic query `30/30`、relations `22/22`、
  symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、canonical consumers
  `21/21`，全部真实exit 0。
- GCC/Clang interface进程均真实exit 1，失败名称仍为fixed parent的同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 01:13 +08:00。
- 状态：Task 4.23 focused GREEN；Plan 03 Task 4/Task 7/Task 8总门禁仍进行中。
- 完成项目：parameter binding唯一性；parameter TypeId integrity；passing-form integrity；
  conversion exactness integrity；
  malformed snapshot fail-closed；GCC/Clang focused与interface marker复核；模块与计划记录。
- 未完成项目：source `in/ref/out` call-fact producer、receiver/member mapping、receiver `TypeId`、
  `.zro`/native callable mapping parity、Syntax05 imported property/declaration producer、MSVC、
  完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
