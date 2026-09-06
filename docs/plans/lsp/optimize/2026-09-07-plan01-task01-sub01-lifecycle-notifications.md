---
related_code:
  - zr_vm_language_server/stdio/stdio_lifecycle.c
  - zr_vm_language_server/stdio/stdio_lifecycle.h
  - zr_vm_language_server/stdio/stdio_requests.c
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/test_stdio_server_lifecycle.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 1 Sub01: Lifecycle Notification Gates

## 状态与产出记录

- 开始时间: 2026-09-07 03:48 +08:00
- 实际完成时间: 2026-09-07 03:56 +08:00
- 状态: 已完成
- 源码版本: 基于 `e0737fa8` 的当前工作树；本记录与实现由同一阶段提交固化
- 产出路径: `stdio_requests.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项补齐生命周期外的 control notification 门禁，并为 shutdown/exit 顺序补充
可重复的协议证据。它只关闭 notification routing 的边界，不宣称 Plan 01 Task 1
及其跨工具链父级门禁已全部验收。

## RED

新增用例先运行于旧 MSVC Debug server。`$/setTrace` 在 `initialize` 前被接受，
随后 `workspace/symbol` 产生了 inbound/outbound trace；`shutdown` 后再次设置
`verbose` 也让被忽略的 notification 写入 trace。这违反了除 `exit` 外的
notification 状态约束。

## 实现

`handle_notification_message` 保留 `exit` 的全状态处理，并要求 `$/setTrace`
先通过 `ZrLanguageServer_StdioLifecycle_CanProcessRequest`。因此 `NEW` 和
`SHUTDOWN` 状态不会改变 trace 设置；`initialized` 仍只由 lifecycle 模块接受
`INITIALIZING` 到 `RUNNING` 的转换。协议回归新增 shutdown 前拒绝、shutdown 后
成功 exit，以及生命周期外 control notification 的 side-effect 断言。

## 验证命令及结果

工具链:

- GCC 11.4.0 Debug: `.codex/build-lsp-opt-gcc`
- MSVC 19.44.35228.0 Debug: `.codex/lsp-optimize-validation/msvc`（RED 对照）

```text
node --check tests/language_server/stdio_protocol_conformance.js
  passed

WSL node tests/language_server/stdio_protocol_conformance.js
    .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  33/33 passed

WSL .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_server_lifecycle_test
  Pass - stdio server lifecycle
```

旧 MSVC 对照的 control-notification 用例失败于预初始化 trace side effect；其余
生命周期顺序用例在修复前后均保持对应退出码断言。`git diff --check` 对本子项
源文件和文档无空白错误。

## 接受决定

接受 Plan 01 Task 1 Sub01。`$/setTrace` 不再绕过 lifecycle 状态，shutdown/exit
顺序在当前 GCC 上有 33-case 协议证据；Task 1 的其余状态审计和 Plan 01 父级门禁
仍需后续验收。
