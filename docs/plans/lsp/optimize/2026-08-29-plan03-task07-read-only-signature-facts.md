# Plan 03 Task 7.12 Read-Only Signature Facts

## 目标

- 保证 signature argument fact documentation 只读消费 immutable snapshot。
- 缺失 optional argument fact 时省略文档，不在请求期间运行 type inference。
- 以 runtime mutation guard 与 source contract 阻止回归。

## 完成项目

- 删除 `signature_fact_materialize_argument` 及 analyzer-internal dependency。
- signature parameter documentation 直接读取已发布 expression、numeric、
  logical、ownership 与 ownership-intrinsic facts。
- runtime test 隐藏首个 argument expression fact 后请求 signature help，验证
  help 仍返回且 `expressionFacts` 数量不增长。
- source contract 禁止 `InferExactExpressionType` 与 request-time materializer。

## 验证

- GCC/Clang/MSVC focused inlay/completion/signature semantic facts：`13/13`，
  真实 exit 0。
- GCC/Clang/MSVC LSP source contracts：`63 Pass / 0 Fail`，真实 exit 0。
- 三工具链 full interface 均为固定 `109 Pass / 4 Fail`，四个既有 marker
  完全相同、delta 0，因此不计 GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 01:11 +08:00。
- 状态：已完成。
- 完成项目：signature request-time inference 删除、snapshot mutation RED/GREEN、
  三工具链 focused runtime/source-contract 验证、固定 interface marker 复核。
