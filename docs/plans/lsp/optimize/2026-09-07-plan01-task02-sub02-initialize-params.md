---
related_code:
  - zr_vm_language_server/stdio/stdio_requests.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub02: Initialize Params Shape

## 状态与产出记录

- 开始时间: 2026-09-07 03:31 +08:00
- 实际完成时间: 2026-09-07 03:41 +08:00
- 状态: 已完成
- 源码版本: 基于 `e10465d2` 的当前工作树；本记录与实现由同一阶段提交固化
- 产出路径: `stdio_requests.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项关闭 `initialize` 方法级 `params` 形状边界。它只约束
`initialize`，不宣称所有请求 handler 已迁移到统一的 status/result contract。

## RED

新增数组参数用例先运行于旧 MSVC Debug server。旧实现接受
`{"jsonrpc":"2.0","id":"array-init","method":"initialize","params":[]}`，
并返回成功的 initialize result；该行为违反 LSP initialize 参数必须为对象的
方法级约束，也会在错误输入下推进生命周期。

## 实现

`handle_request_message` 在 `initialize` 分支调用生命周期转换前要求 `params`
是 JSON object。缺失字段、JSON `null`、标量和数组均返回
`-32602 InvalidParams`。initialize handler 返回空结果时现在映射为
`-32603 InternalError`，避免把构造失败误报为成功的 JSON `null`。

协议回归将缺失、标量、`null` 和数组参数放入同一组，保持请求 ID、响应 envelope
和错误码的精确断言。

## 验证命令及结果

工具链:

- GCC 11.4.0 Debug: `.codex/build-lsp-opt-gcc`
- MSVC 19.44.35228.0 Debug: `.codex/lsp-optimize-validation/msvc`（RED 对照）

```text
node --check tests/language_server/stdio_protocol_conformance.js
  passed

WSL node tests/language_server/stdio_protocol_conformance.js
    .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  31/31 passed, including missing/scalar/null/array initialize params

WSL .codex/build-lsp-opt-gcc/bin/test_stdio_server_lifecycle
  Pass - stdio server lifecycle
```

`git diff --check` 对本子项源文件和文档无空白错误。完整 Plan 01 的
envelope、handler status、frame、取消、ContentModified 和 teardown 门禁仍保持
pending。

## 接受决定

接受 Plan 01 Task 2 Sub02。非法 `initialize` 参数在生命周期转换前得到精确
`InvalidParams`，协议驱动和生命周期专项通过；其他请求方法的统一参数状态契约
仍需后续子项完成。
