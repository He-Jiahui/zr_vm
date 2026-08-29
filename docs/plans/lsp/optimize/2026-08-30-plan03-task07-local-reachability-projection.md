---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_local_semantic_query.c
doc_type: milestone-record
---

# Plan 03 Task 7.37: Local Reachability Projection

## Goal

让 local semantic query 在光标位于逻辑运算符时仍返回 parser 已发布的短路
reachability fact，同时保持 fact 的 canonical range 和 fail-closed 边界。

## Contract

- 先按请求点查询 reachability fact。
- 请求点未命中时，只能沿已解析 logical fact 的 `relatedNode` 查询右操作数范围。
- 不使用 identifier name、diagnostic message、类型文本、源码文本或 member-name
  fallback 创建 reachability fact。
- member-write reference 的 producer kind/range 不正确时，LSP 保持该事实为 producer
  阻塞，不在 consumer 侧改写 kind。

## RED/GREEN

短路 parser fact 的 range 位于不可达右操作数，而 local query 的请求点位于 `||` 运算符。
原实现只在请求点调用 `FindReachabilityAtPosition`，因此已存在的 logical fact 被返回但
reachability fact 为空。GREEN 使用该 logical fact 的结构化 `relatedNode` 位置再次查询，
不改变 parser facts。三工具链 local-query 的 short-circuit case 均通过。

## Verification

- GCC、Clang、MSVC focused builds 的真实 exit 均为 `0`。
- 三工具链 local semantic query process 仍真实 exit `1`，唯一剩余 focused failure 是
  parser producer 发布的 member-write fact kind/range 不符；short-circuit、ownership
  与其他 local query cases 均通过。
- 三工具链 interface process 仍真实 exit `1`，仅保留既有 class-member fixture failure。
- 三套 `stdio_smoke.js` 仍真实 exit `1`，在 generic fixture 缺少
  `short_circuit_unreachable` producer warning 处停止；没有把该 producer 缺口转成 LSP
  fallback。

## 状态与产出记录

- 完成时间：2026-08-30 07:29 +08:00。
- 状态：local short-circuit reachability projection focused GREEN；Plan 03 Task 7/Task 8
  仍未完成，global Plan 03 不声明 GREEN。
- 完成项目：logical fact `relatedNode` 到 canonical reachability range 的 LSP 投影、
  GCC/Clang/MSVC focused regression、interface 与 stdio smoke 回归审计。
- 未完成项目：member-write producer fact 修复、其余 producer/metadata ownership、
  semantic-token migration 及最终 16-target/stdio all-green gate。
