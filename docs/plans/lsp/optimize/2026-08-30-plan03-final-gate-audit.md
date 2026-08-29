---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: audit-record
---

# Plan 03 Final Gate Audit

## Scope

本记录按 Plan 03 Task 8 对当前可用的三工具链快照执行只读最终门禁。验证同时记录测试
进程真实退出码与 `Pass -`/`Fail -` marker；任何一个不一致都不宣称 GREEN。未修改
Syntax05-owned parser producer、semantic-token producer 或 project metadata producer。

## Verification

- GCC 与 Clang semantic-query parity 均为 `14/14`、真实 exit 0；MSVC parity 为 `14/14`、
  真实 exit 0。
- 三套 source-contract 目标真实 exit 0；三套 inlay 目标为 `13/13`；三套 CLI
  `--version` 真实 exit 0。expression/symbol focused targets也真实 exit 0。
- interface 三套均真实 exit 1；reference-call diagnostic 用例已通过，剩余为既有
  class-member fixture 失败。该 diagnostic bridge 通过 analyzer 对 parser/compiler current
  error 调用既有 structured publisher，未由 LSP 按消息、signature 名称或成员名补建事实。
- local semantic query 三套均真实 exit 1；Task 7.36 的 ownership violation case 与
  Task 7.37 的 short-circuit reachability case 已通过，剩余为 member-write reference
  producer fact 的 kind/range 缺失/不符。
- project 三套测试进程真实 exit 0，但每套包含 `51 Pass / 9 Fail`；失败集中在
  imported/native/descriptor metadata、refresh generation 与 pooling guard contract，
  因而不能把 exit 0 单独解释成通过。
- GCC semantic analyzer 真实 exit 1，`68 Pass / 2 Fail`；失败为 closed-generic
  receiver prototype 与 borrow-return ownership fact，属于 parser/producer 边界。固定
  Clang/MSVC LSP 快照没有该独立 analyzer binary。
- GCC/Clang/MSVC full `stdio_smoke.js`（Task 7.37 重跑）均真实 exit 1，并在 generic
  fixture 处停止：
  缺少 `short_circuit_unreachable` warning（`tests/language_server/stdio_smoke.js:2003`）。
  这不是 stdio transport、shutdown 或 CLI 启动失败。三套 CLI 版本检查仍通过。
- 2026-08-30 07:44 +08:00 的有效 16-target 回放在 GCC、Clang、MSVC 均保持
  `10 PASS / 6 FAIL`，且每个测试进程的真实退出码与 marker 一致。三套 CLI
  `--version` 也分别真实 exit 0（GCC GNU 11.4.0、Clang 14.0.0、MSVC
  19.44.35228.0）。通过项包括 semantic query、compiler diagnostics、semantic facts、
  canonical consumers、LSP diagnostic query、incremental parser、project features、
  UTF-16 ranges、source contracts 与 expression facts。失败项在三套工具链一致：canonical
  type graph 的 tuple AST producer、semantic analyzer 的既有 producer markers、interface
  的 class-member fixture、local query/hover 的 member-write producer fact、language
  feature matrix 的 imported-type binding，以及 project pooling guard marker。Task 7.37
  修复的 short-circuit local case 在该回放中保持通过；stdio smoke 仍因同一
  `short_circuit_unreachable` producer warning 缺失而真实 exit 1。

## Ownership Boundary

结构化 semantic diagnostics 的 LSP 路径已确认只执行 query materialization 与协议字段投影。
semantic tokens 仍由 Syntax05 Task4 持有，保留的 symbol-table/metadata fallback 未被扩大；
跨项目 import/member identity 和 reference-call diagnostic 的 producer 缺口必须在 parser、
metadata 或 Syntax05 归属路径收口后重跑。本记录不以 LSP 兼容逻辑掩盖这些缺口。

## 状态与产出记录

- 完成时间：2026-08-30 07:44 +08:00（Task 7.37 focused 实现完成于 07:29，最终矩阵与
  CLI 回放于 07:44 完成）。
- 状态：最终门禁审计已更新至 Task 7.37；Task 7/Task 8 仍未完成，不声明本阶段
  GREEN 或完成。
- 完成项目：三工具链 parity/source-contract/inlay/CLI 复核；Task 7.35 reference-call
  structured diagnostic bridge、Task 7.36 ownership range projection 与 Task 7.37
  short-circuit reachability projection 的 GCC/Clang/MSVC 回归；interface/project/analyzer
 失败 marker 复核；三套 stdio smoke 与 16-target 矩阵真实退出码复核；三套 CLI
  `--version` 复核；producer 与 LSP ownership 分层。
- 阻塞项目：Syntax05 producer/metadata 路径释放；short-circuit/member-write、
  closed-generic/borrow-return facts、cross-project metadata identity、semantic-token
  canonical migration 与既有 `short_circuit_unreachable` producer fixture；之后重跑同一
  16-target 矩阵和三套 smoke。
