---
plan_id: lsp-03-robustness
record_id: 2026-07-19-strict-document-version-rejection
status: completed
completed_at: 2026-07-19 21:12 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: strict-document-version-rejection
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
related_tests:
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/stdio_smoke.js
---

# Strict Document Version Rejection

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-07-19 21:12 +08:00 | 已完成 | 已打开document的严格version单调门禁；相同与stale version在text分配、change classification、lexer/parser与semantic request之前拒绝；快照与语义指标不变断言；当前HEAD上的GCC/Clang/MSVC十四目标矩阵、incremental parser和stdio/CLI JSON-RPC冒烟 |

## 已完成契约

- 已存在document只接受`newVersion > currentVersion`；相同version和更旧version立即返回`ZR_FALSE`。
- version检查位于内容相等比较、old/new变更范围计算、token等价lexer、text block分配、parse和semantic analysis之前。
- 拒绝后保持document version、text block指针、content generation、AST指针、dirty状态与semantic analyzer request/execution/cache-hit计数不变。
- 首次建立document时不存在旧版本，因此仍允许workspace/on-disk路径使用version 0建立初始快照。

## TDD与验证证据

- RED：实现前，incremental parser用例报告`Rejected version mutated parser state`，LSP公共更新用例报告`Rejected version reached snapshot or semantic work`。
- GREEN：在version 5快照上，相同内容/version 5和变化内容/version 4均被拒绝，且快照标识与语义指标全部不变。
- WSL GCC 11.4、WSL Clang 14和Windows MSVC 19.44.35228的定向套件均为incremental parser 7/7、LSP interface 87/87，无`Fail -`标记。
- 最终回归以当前`HEAD=894af85`及本阶段overlay运行；三套工具链分别通过相同十四语义/LSP目标及incremental parser，所有可执行目标exit code为0且无`Fail -`标记；`language_server_stdio_smoke`均1/1。

## 未完成边界

- 本记录只关闭已存在document的非单调version输入，不表示L6或整体LSP鲁棒性计划完成。
- 还未实现owning function CFG/query cache最小失效、direct caller与ModuleIdentity依赖传播、partial reparse或cached token inventory。
- cancellation、immutable snapshot race、stale response suppression、历史快照上限、provider parity、延迟百分位与峰值内存门槛仍待后续子里程碑。
- 隔离源树继续把未提交core profiling helper作为外部baseline overlay；该无关core改动不属于本记录。
