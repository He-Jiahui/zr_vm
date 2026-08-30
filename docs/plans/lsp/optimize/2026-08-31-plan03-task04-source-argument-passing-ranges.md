---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_argument_mapping_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.24: Source Argument Passing Ranges

## Goal

让 source `in`/`ref`/`out` 调用从同一 canonical call fact 返回 passing mode、TypeId、
`hasCallInfo` 与完整 argument range；LSP 不扫描 `ref`/`out` 文本补齐范围。

## Contract

- passing mode 来自 selected `SZrResolvedCallSignature.parameterPassingModes`。
- `in` 调用无需 marker，argument range 等于 argument expression range。
- `ref`/`out` 显式 marker 使用 parser 保存的 `SZrCallArgumentSyntax.markerLocation` 与 argument
  AST range 合并；不从 source bytes 查找 keyword。
- named label 不属于 argument expression range。
- mapping 两侧 canonical `TypeId` 必须有效，query继续执行 Task 4.23 的 callable-contract
  integrity 门禁。

## RED/GREEN

新增 source `inspect(in)`、`touch(ref)`、`fill(out)` query case。初始 producer已发布
`hasCallInfo`、passing mode和TypeId，但 `touch(ref value)` 的 mapping range从裸`value`开始：
GCC calls真实exit 1，`26 Tests / 1 Failure`，`Expected 236 Was 240`。

GREEN只修改 parser mapping producer：读取现有结构化`argumentMarkers`，当marker非NONE时合并
marker与argument范围。`ref value`和`out value`覆盖完整语法，`in`与既有named reorder/conversion
范围不变；未增加LSP fallback。

## Verification

- GCC/Clang固定独立snapshot均通过 parser/display/calls/query/relations/symbols/parity/
  source-contract/facts/canonical/type-inference
  `74/22/26/30/22/21/15/70/15/21/124`，真实exit 0。
- 两套 type-inference 串行运行，均为`124/124`。
- GCC/Clang interface均真实exit 1，失败集合精确保持fixed parent同一8个既有producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 07:40 +08:00。
- 状态：Task 4.24 focused GREEN；Plan 03 Task 4/Task 7/Task 8总门禁仍进行中。
- 完成项目：source `in/ref/out` call-info验收；structured marker range投影；passing mode与
  canonical TypeId冻结；GCC/Clang expanded gate；interface fixed-marker复核；parser module与计划记录。
- 未完成项目：receiver/member mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  unresolved reason、Syntax05 imported declaration/property producer、MSVC、完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
