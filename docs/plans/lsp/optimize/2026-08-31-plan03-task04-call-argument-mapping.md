---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_argument_mapping_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.20: Call Argument Mapping

## Goal

让 resolved source free-call 在 parser canonical fact/query 层发布精确的
argument-to-parameter mapping 与 conversion，不让 LSP 从 AST、名称或 signature text 重建。

## Contract

- mapping 与 selected callable identity 保存在同一 `SZrSemanticReferenceFact`。
- 每行包含 argument/parameter index、精确 argument range、两侧 canonical `TypeId`、passing
  mode、named 标志以及 exact/implicit conversion。
- `CallAt.argumentMappings` 是 semantic snapshot 拥有的 borrowed array；跨 snapshot 保存必须复制。
- non-empty malformed payload 清零并 fail closed；spread 暂不发布近似 one-to-one mapping。
- 本片仅覆盖 source free-call；receiver/member、`.zro` 和 native descriptor parity 后续完成。

## RED/GREEN

新增 named reorder 与 `int` 到 `float` widening fixture。初始编译 RED 真实 exit 1：public fact
缺少 `SZrSemanticCallArgumentFact`/conversion enum，query 缺少 `argumentMappings`。首次 runtime
定位发现测试 position 命中了局部变量而非 call，修正 selector 后才作为有效功能门禁。

GREEN 由独立 producer 模块消费 resolved signature、source parameter names 与已发布的 exact
argument expression facts。named call 发布 `arg0 -> param1`、`arg1 -> param0` exact rows，widening
发布 `arg0 -> param0` implicit row。测试破坏 copied row 的 parameter index 后，`CallAt` 返回 false
并清空复用输出，证明 query 不接受不完整 payload。

## Verification

- 固定 GCC/Ninja snapshot：calls `25/25`、semantic query `30/30`、relations `22/22`、symbols
  `21/21`、semantic-query parity `15/15`、source-contract `70/70`、semantic-facts `15/15`，
  全部真实 exit 0。
- 同一字节 Clang/Ninja snapshot：同一七目标分别为 `25/25`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`、`15/15`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 00:03 +08:00。
- 状态：Task 4.20 focused GREEN；Plan 03 Task 4/Task 7/Task 8 总门禁仍进行中。
- 完成项目：source free-call structured mapping producer；fact deep-copy/reset lifetime；borrowed
  query projection；named reorder/exact/implicit conversion与malformed fail-closed TDD；GCC/Clang
  七目标及interface marker复核；模块与计划记录。
- 未完成项目：receiver/member mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  unresolved reason、Syntax05 imported declaration/property producer、MSVC、完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
