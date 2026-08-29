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
- interface 三套均真实 exit 1，均保留 `111 Pass / 2 Fail`：class-member navigation
  既有失败，以及 reference-call diagnostic query fact 缺失。该 diagnostic 失败在
  `ZrParser_SemanticQuery_Diagnostics()` 中没有对应 persistent fact，不由 LSP 按消息或
  signature 名称补建。
- project 三套测试进程真实 exit 0，但每套包含 `51 Pass / 9 Fail`；失败集中在
  imported/native/descriptor metadata、refresh generation 与 pooling guard contract，
  因而不能把 exit 0 单独解释成通过。
- GCC semantic analyzer 真实 exit 1，`68 Pass / 2 Fail`；失败为 closed-generic
  receiver prototype 与 borrow-return ownership fact，属于 parser/producer 边界。固定
  Clang/MSVC LSP 快照没有该独立 analyzer binary。
- GCC/Clang/MSVC full `stdio_smoke.js` 均真实 exit 1，并在 generic fixture 处停止：
  缺少 `short_circuit_unreachable` warning（`tests/language_server/stdio_smoke.js:2003`）。
  这不是 stdio transport、shutdown 或 CLI 启动失败。三套 CLI 版本检查仍通过。

## Ownership Boundary

结构化 semantic diagnostics 的 LSP 路径已确认只执行 query materialization 与协议字段投影。
semantic tokens 仍由 Syntax05 Task4 持有，保留的 symbol-table/metadata fallback 未被扩大；
跨项目 import/member identity 和 reference-call diagnostic 的 producer 缺口必须在 parser、
metadata 或 Syntax05 归属路径收口后重跑。本记录不以 LSP 兼容逻辑掩盖这些缺口。

## 状态与产出记录

- 完成时间：2026-08-30 05:56 +08:00。
- 状态：最终门禁审计完成，但 Plan 03 Task 7/Task 8 仍未完成；不声明本阶段 GREEN
  或完成。
- 完成项目：三工具链 parity/source-contract/inlay/CLI 复核；interface/project/analyzer
  失败 marker 复核；三套 stdio smoke 真实退出码复核；producer 与 LSP ownership 分层。
- 阻塞项目：Syntax05 producer/metadata 路径释放；reference-call persistent diagnostic、
  closed-generic/borrow-return facts、cross-project metadata identity 与 semantic-token
  canonical migration；之后重跑同一 16-target 计划矩阵和三套 smoke。
