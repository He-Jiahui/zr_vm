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

# Plan 03 Task 7.36: Local Ownership Projection

## Goal

让 local semantic query 在光标位于 ownership expression 的语法前缀时仍返回 parser 发布的
canonical ownership fact，同时保持 exactness 和 fail-closed 边界。

## Contract

- 先使用 expression fact 的 exact AST node 查询 ownership fact。
- node 查询不可用时，只在同一 expression fact 的 canonical range 内选择最窄 ownership range，
  同宽保留结构化事实顺序和 violation 优先级。
- 不使用 identifier name、diagnostic message、类型文本或 AST 文本配对来猜测 ownership。
- parser 没有 ownership fact 时保持 unavailable，不生成新的 LSP semantic fact。

## RED/GREEN

`return ref resource` 的查询点落在 `ref` 前缀，ownership fact 的 canonical range 覆盖内部
`resource`。原 local query 只把请求点传给 `FindOwnershipAtPosition`，因此遗漏已有 violation
fact。GREEN 增加 expression-node 优先和 expression-range 内结构化 ownership range 投影；
现有 local ownership violation case 通过，未改变 parser fact。

## Verification

- GCC、Clang、MSVC build real exit `0`，local semantic query process 均真实 exit `1`，
  但 ownership violation case 均 `Pass`。
- 三工具链剩余相同的两个 local-query failures：short-circuit reachability producer
  fact 缺失、member-write reference producer range/kind 不符；未增加 LSP fallback。
- 其他 Task 7.36 local query regression 与 query cache cases保持通过。

## 状态与产出记录

- 完成时间：2026-08-30 07:08 +08:00。
- 状态：已完成 local ownership projection 子里程碑；Plan 03 Task 7/Task 8 仍未完成，
  不声明全局 GREEN。
- 完成项目：expression node ownership lookup、canonical range 内最窄 fact 选择、三工具链
  focused regression 与 failure boundary 审计。
- 后续项目：等待 parser reachability/reference producer 收口，继续处理剩余 consumer 与
  最终 16-target/stdio gate。
