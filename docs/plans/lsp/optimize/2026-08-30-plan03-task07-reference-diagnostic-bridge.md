---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_interface.c
doc_type: milestone-record
---

# Plan 03 Task 7.35: Reference Diagnostic Bridge

## Goal

让 LSP typecheck 继续消费 parser/compiler 发布的 structured diagnostic fact。exact expression
inference 失败时，已发布的诊断不得再被泛化的 `cannot infer exact type` 覆盖或重复包装。

## Contract

- compiler current error 通过既有 `PublishCurrentCompilerQueryDiagnostic` 发布到 persistent
  semantic diagnostic facts。
- analyzer 只消费该 fact，并在发布成功后清理 current compiler error；不按 message、类型文本、
  member name 或 AST 配对重建诊断。
- primary-call typecheck 对成功返回后残留的 current compiler diagnostic 使用同一 publisher。
- parser fact 缺失仍保持既有 fail-closed 行为；本任务不扩大 producer、metadata 或 semantic-token
  ownership。

## RED/GREEN

reference callable fixture `inspect(value: scoped ref readonly int)` 被错误调用时，parser
已生成 `ref parameter requires the 'ref' argument marker`。此前 analyzer 清除/消费该错误后
继续报告 generic inference failure，导致 LSP interface case 失败。GREEN 在
`semantic_analyzer_typecheck.c` 让 inference helper 返回“已发布诊断”状态，并在变量绑定、
显式返回和 primary-call 路径抑制重复 generic diagnostic；原始 structured diagnostic 仍由
query facts 提供。

## Verification

- GCC、Clang、MSVC interface reference-call case 均通过，真实 interface 进程仍因既有
  class-member fixture 返回非零。
- 三工具链 16-target 构建均真实 exit `0`；运行结果保持一致：semantic query、compiler
  diagnostics、facts、canonical consumers、semantic query diagnostics、incremental parser、
  UTF-16 ranges、source contracts 与 expression facts 通过；其余既有 graph/analyzer/local/
  project/interface/feature-matrix failures 未被本任务掩盖。
- GCC/Clang/MSVC `stdio_smoke.js` 均真实 exit `1`，统一停止于缺少
  `short_circuit_unreachable` warning；三套 CLI `--version` 均真实 exit `0`。

## 状态与产出记录

- 完成时间：2026-08-30 06:56 +08:00。
- 状态：已完成 reference diagnostic bridge 子里程碑；Plan 03 Task 7/Task 8 仍未完成，
  不声明全局 GREEN。
- 完成项目：exact inference diagnostic publish/consume bridge、重复 generic diagnostic
  抑制、primary-call current-error drain、三工具链 focused parity 与完整门禁重放。
- 后续项目：等待 Syntax05 producer/metadata release，修复 short-circuit diagnostic producer
  与既有 closed-generic/borrow-return/class-member/local/project markers，再重跑同一 16-target
  matrix 与三套 stdio smoke。
