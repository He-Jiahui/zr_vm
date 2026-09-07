---
plan_id: optimize
task: plan01-task06-sub07
status: completed
related_code:
  - tests/language_server/test_lsp_project_module_identity_edge_cases.h
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub07: Rename Canonical Type Assertions

## 状态与产出记录

- 开始时间: 2026-09-07 23:05 +08:00
- 实际完成时间: 2026-09-07 23:34 +08:00
- 状态: canonical 验收断言完成；父级完整门禁未完成
- 源码版本: `8bdb56de` 加共享 overlay
- 产出路径: project/stdio 验收断言和模块契约
- 剩余门槛: Linux CLI binary artifact、project 历史失败与内存门禁

## 失败与责任层

Sub06 后完整 MSVC stdio smoke 首次推进至第 3817 行，rename fixture 的
hover 断言查找 `float` 而失败。最小三文件复现显示重命名后 `cached` 已从
`Type: object` 恢复到 `Resolved Type: double`，definition 也准确指向
`modern.zr` 的 value 声明。旧 import 的 unresolved 诊断保持，符合当前文档
仍包含旧 import 的输入。

`ZrParser_SemanticDisplay_FormatType` 委托 canonical formatter，DOUBLE 显示为
`double`。历史 [super constructor argument mapping](2026-08-31-plan03-task04-super-constructor-argument-mapping.md)
已明确 source float 对应 canonical double。本项对齐已有类型契约，不更改类型
推断、依赖刷新或 public formatter。

验证将直接检查刷新前后的 canonical primitive kind，再校验准确 hover 类型段。
保留已有两个 importer 恰好各重分析一次、module record 迁移与定义位置断言。
日志前缀为 `.codex/lsp-optimize-validation/plan01-task06-sub07-`。

## 验证与产出

三工具链重建并运行 `zr_vm_language_server_lsp_project_features_test`。
本项 `LSP Source Module Identity Change Refreshes Old And New Importers`
均 PASS：canonical OBJECT 到 DOUBLE、两个 importer 各重分析一次、source
record 迁移和准确 hover 段均成立。完整 runner 各保留 10 个历史功能失败；
Clang 另有 19,160 bytes/481 allocations LSan，均真实 exit 1。

三工具链最小协议回放分别检查初始 object、重命名后 double、唯一 definition
为 modern.zr 第 2 行 value 名称范围、shutdown/exit 0，全部通过。MSVC
更新后的完整 `stdio_smoke.js <server> <cli>` exit 0，峰值 46,542,848 bytes。
GCC/Clang 原始完整 smoke 在本项之前的 binary fixture member_not_found
停止；同 CLI 最小复现的 plain/cast 均失败，详见 Sub06 记录。

GCC 构建 `/home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc`，Clang
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`，MSVC
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`。本项没有改动生产
查询或类型显示实现。实现边界与 snapshot 所有权见
[ModuleIdentity edge migration](../../../cli-and-tooling/lsp-module-identity-edge-migration.md)。
