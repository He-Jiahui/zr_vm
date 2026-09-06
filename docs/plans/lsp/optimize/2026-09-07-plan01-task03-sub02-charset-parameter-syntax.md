---
related_code:
  - zr_vm_language_server/stdio/stdio_frame_reader.c
  - zr_vm_language_server/stdio/stdio_frame_reader.h
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 3 Sub02: Charset Parameter Syntax

## 状态与产出记录

- 开始时间: 2026-09-07 04:00 +08:00
- 实际完成时间: 2026-09-07 04:06 +08:00
- 状态: 已完成
- 源码版本: 基于 `adcaa64f` 的当前工作树；本记录与实现由同一阶段提交固化
- 产出路径: `stdio_frame_reader.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项补齐 `Content-Type` 中显式 `charset` 参数的缺值边界。它只关闭
header parameter syntax，不宣称 Task 3 的全部大小限制、transport 或父级门禁已
完成。

## RED

新增 malformed-frame 用例先运行于当前 GCC Debug server。输入
`Content-Type: application/vscode-jsonrpc; charset` 未被识别为错误，reader
继续读取 payload 并在协议驱动中缺少预期的 `MALFORMED_HEADER` stderr 分类。

## 实现

`frame_reader_content_type_is_utf8` 在没有 `=` 的参数中识别显式 `charset`，
并立即返回 false；带值的字符集仍只接受 `utf-8`/`utf8`（含引号），未知参数
保持可忽略。reader 因此在 payload 分配前返回 `ZR_STDIO_FRAME_READ_MALFORMED_HEADER`。

## 验证命令及结果

工具链:

- GCC 11.4.0 Debug: `.codex/build-lsp-opt-gcc`
- Clang 14.0.0 ASan/UBSan、`-no-pie`: `.codex/lsp-optimize-validation/clang-asan-current`

```text
WSL node tests/language_server/stdio_protocol_conformance.js
    .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  33/33 passed, including charset-without-value classification

WSL ext4 Clang ASan/UBSan replay of the same protocol driver
  33/33 passed; no sanitizer diagnostics

GCC/Clang zr_vm_language_server_stdio_server_lifecycle_test
  Pass - stdio server lifecycle
```

`node --check tests/language_server/stdio_protocol_conformance.js` 和
`git diff --check` 均通过。挂载盘上的 Clang 直接运行仍受已记录的响应超时边界
影响，接受证据来自 ext4 copy。

## 接受决定

接受 Plan 01 Task 3 Sub02。无值的显式 `charset` 不再绕过 header validation，
未知扩展参数兼容性保留；Task 3 其余限制和 Plan 01 父级门禁仍保持 pending。
