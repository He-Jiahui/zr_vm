---
related_code:
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 4 Sub03: Progress Token Identity

## 状态与产出记录

- 开始时间: 2026-09-07 03:48 +08:00
- 实际完成时间: 2026-09-07 03:57 +08:00
- 状态: 已完成
- 源码版本: 基于 `e0737fa8` 的当前工作树；本记录与实现由同一阶段提交固化
- 产出路径: `stdio_requests.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项关闭 progress token 的 numeric JSON identity 边界。它只覆盖 token 验证和
notification 回写，不宣称完整 request registry、取消、ContentModified 或统一
progress sink 已完成。

## RED

新增正负安全边界用例先运行于旧 MSVC Debug server。`workDoneToken` 使用精确
`9007199254740991` 时，旧实现通过 cJSON number reference 输出为
`9007199254740990`；这会让客户端收到与请求 token 不同的 progress stream。

## 实现

`stdio_request_progress_token_duplicate` 对字符串 token 做深复制，对数字 token
使用 `%.17g` 创建 raw cJSON 节点，再由 progress envelope 接管所有权。现有验证
继续要求数字 token 有限、整数且位于 `+/-ZR_LSP_JSON_SAFE_INTEGER_MAX`；超界或
错误类型在 progress begin 前返回 `-32602`，不会发送 begin/end notification。

## 验证命令及结果

工具链:

- GCC 11.4.0 Debug: `.codex/build-lsp-opt-gcc`
- MSVC 19.44.35228.0 Debug: `.codex/lsp-optimize-validation/msvc`（RED 对照）

```text
WSL node tests/language_server/stdio_protocol_conformance.js
    .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  33/33 passed, including positive/negative safe workDoneToken boundaries,
  partial-result token rejection and all partial-result streams
```

The old MSVC binary remains a RED reference for the numeric boundary; the current
GCC executable was rebuilt from the working tree before the direct run. `git diff --check`
对本子项源文件和文档无空白错误。

## 接受决定

接受 Plan 01 Task 4 Sub03。Progress numeric token 的安全边界在 notification 中
保持精确 JSON identity，非法 token 不会启动 progress；Task 4 的其余父级门禁仍
保持 pending。
