# Plan 03 Task 7.13 Read-Only Local Semantic Query

## 目标

- 让 local expression/hover query 只读消费 immutable parser snapshot。
- 删除 request-time inference、inferred-type registration、symbol-table lookup
  与 position type-resolution 回填。
- 通过 fixed baseline marker 对照证明删除 fallback 不扩大既有 producer 缺口。

## 完成项目

- 删除 `local_query_materialize_expression_fact`、should-materialize 判定和
  二次 fact collection。
- `ExpressionAt` 只执行一次 reference/expression/numeric/logical/reachability/
  ownership/intrinsic fact collection。
- runtime test 隐藏 exact expression fact 后请求 local query，验证 fact 数量
  不增长。
- source contract 禁止 inference、inferred-type registration、symbol-table
  lookup 和 position type resolver 回填。

## 验证

- GCC/Clang/MSVC source contracts：`64 Pass / 0 Fail`，真实 exit 0。
- 三工具链 expanded GREEN：logical `3`、computed-member hover `1`、expression
  hover `11`、reachability `11`、numeric `4` 与 source-contract target，共
  `7/7` 真实 exit 0。
- 三工具链 local-query：`30 Pass / 3` 个既有 marker；fixed baseline 加入新
  RED 时为相同 3 个 marker加 immutable case，GREEN 后仅 immutable case 消失。
- 三工具链 local-hover：`10 Pass / 2` 个既有 test marker，fixed baseline 与
  overlay 集合一致。
- 三工具链 full interface：固定 `109 Pass / 4 Fail`，marker delta 0，不计
  GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 01:27 +08:00。
- 状态：已完成。
- 完成项目：local semantic request mutation 删除、fixed-baseline RED/GREEN、
  三工具链 expanded consumer gate 与既有 marker delta 复核。
