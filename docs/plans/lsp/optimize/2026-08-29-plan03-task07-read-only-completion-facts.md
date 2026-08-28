# Plan 03 Task 7.11 Read-Only Completion Facts

## 目标

- 保证 completion semantic-fact enrichment 只读消费 immutable parser snapshot。
- 缺失 optional fact 时省略 detail，不在请求期间运行 type inference。
- 用 runtime mutation guard 与 source contract 同时阻止回归。

## 完成项目

- 删除 `completion_fact_materialize_initializer` 及其 analyzer-internal
  dependency。
- completion detail 直接读取 expression、numeric、logical、ownership 和
  ownership-intrinsic facts；缺失 fact fail closed。
- runtime test 隐藏既有 initializer expression fact 后请求 completion，验证
  item 仍可返回且 `expressionFacts` 数量不增长。
- source contract 禁止 `InferExactExpressionType` 与 request-time materializer
  重新进入该 consumer。

## 验证

- GCC/Clang/MSVC focused inlay/completion semantic facts：`12/12`，真实 exit 0。
- GCC/Clang/MSVC LSP source contracts：`62 Pass / 0 Fail`，真实 exit 0。
- 三工具链 full interface 均为固定 `109 Pass / 4 Fail`，四个既有 marker
  完全相同、delta 0，因此不计 GREEN。
- Full stdio 仍受 Task 7.10 已记录的既有 generic short-circuit diagnostic
  缺失阻断，本项不将其计为通过证据。

## 状态与产出记录

- 完成时间：2026-08-29 01:02 +08:00。
- 状态：已完成。
- 完成项目：completion request-time inference 删除、snapshot mutation RED/GREEN、
  三工具链 focused runtime/source-contract 验证、固定 interface marker 复核。
