---
plan_id: optimize
task: plan01-task03-sub03
status: completed
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/stdio_frame_reader.h
  - zr_vm_language_server/stdio/stdio_frame_reader.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 3 Sub03: Frame Limits and Failure Classification

## 状态与产出记录

- 开始时间: 2026-09-07 04:31 +08:00
- 实际完成时间: 2026-09-07 04:34 +08:00
- 状态: 已完成
- 源码版本: `6eaa6019` 及其父级 frame reader 实现提交
- 产出路径: `conf.h`、`stdio_frame_reader.c`、`stdio_transport.c`、协议回归、模块文档与本记录

本子项重验 bounded frame reader 的完整边界。生产默认值集中在
`ZR_LSP_MAX_HEADER_BYTES`（8 KiB）、`ZR_LSP_MAX_HEADER_COUNT`（32）和
`ZR_LSP_MAX_MESSAGE_BYTES`（16 MiB）；`SZrStdioFrameReaderLimits` 允许测试传入
更小值，既有 bounded-frame 记录保留了该注入契约。reader 在 payload 分配前使用
`strtoull`、`errno`、end pointer 和 `SIZE_MAX - 1` 检查 `Content-Length`，并验证
`contentLength + 1` 的分配边界。

## 失败分类

协议 driver 覆盖过长 header、缺失/重复/负数/后缀/溢出 Content-Length、NUL、错误
换行、非 UTF-8 或无值 charset、截断 payload、超大 payload 和过多 header。reader
分别报告 `MALFORMED_HEADER`、`PAYLOAD_TRUNCATED`、`TOO_LARGE`、`IO_ERROR` 或干净
`EOF`；可解析但 JSON 语法错误的 payload 仍在 dispatch 层返回 `-32700`。

## 验证命令及结果

```text
node -e "require('./tests/language_server/stdio_protocol_conformance').protocolCases().length"
  33

WSL GCC Debug
node tests/language_server/stdio_protocol_conformance.js \
  .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  33/33 passed, exit 0

WSL Clang Debug ASan/UBSan focused
oversize frame closes with failure
malformed frames close with classified failure
  2/2 passed, exit 0, no sanitizer diagnostic

MSVC Debug
node tests/language_server/stdio_protocol_conformance.js \
  .codex/lsp-optimize-validation/msvc-current/bin/zr_vm_language_server_stdio.exe
  33/33 passed, exit 0
```

## 接受决定

接受 Plan 01 Task 3 Sub03。默认 frame limits、严格 Content-Length 数值边界和失败
分类已在当前三工具链 replay 中确认；Plan 01 的生命周期、teardown 和更高层父级门禁
仍保持 pending。
