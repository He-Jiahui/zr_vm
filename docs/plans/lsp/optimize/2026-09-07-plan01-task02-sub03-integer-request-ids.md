---
related_code:
  - zr_vm_language_server/stdio/stdio_json_rpc.c
  - zr_vm_language_server/stdio/stdio_json_rpc.h
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 2 Sub03: Integer Request IDs

## 状态与产出记录

- 开始时间: 2026-09-07 04:44 +08:00
- 实际完成时间: 2026-09-07 04:47 +08:00
- 状态: 已完成
- 源码版本: 基于 `a2669678` 的当前工作树；本记录与实现由同一阶段提交固化
- 产出路径: `stdio_json_rpc.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项收紧 LSP request id 的数值形状。数字 id 必须有限、整数且处于
`+/-ZR_LSP_JSON_SAFE_INTEGER_MAX` 范围；字符串 id 的行为和数字/字符串类型隔离
保持不变。它只关闭 envelope 的 fractional-number 边界，不宣称 handler status/result
统一迁移或 Plan 01 Task 2 父级门禁已验收。

## RED

新增 `id: 1.5` 用例先运行于当前修复前 GCC server。server 接受该 request 并返回
成功 envelope，客户端随后因响应 id 不是预期的 `null` 而失败；这违反 LSP request id
的整数约束。

## 实现

`json_rpc_number_id_is_valid` 在安全范围判断前拒绝非有限数值，范围通过后再要求数值
等于其 `long long` 整数转换值。由于转换前已经限定在 JSON-safe 整数范围内，不会触发
超范围浮点到整数转换；非法 fractional id 在 envelope 阶段统一返回
`-32600 Invalid Request` 与 `id: null`。

## 验证命令及结果

```text
node --check tests/language_server/stdio_protocol_conformance.js
  passed

WSL GCC Debug
node tests/language_server/stdio_protocol_conformance.js \
  .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  34/34 passed, exit 0

WSL Clang Debug ASan/UBSan
node tests/language_server/stdio_protocol_conformance.js \
  .codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  34/34 passed, exit 0, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件和文档无空白错误。既有 safe numeric boundary、typed
ID、duplicate ID 和 cancellation 用例继续通过。

## 接受决定

接受 Plan 01 Task 2 Sub03。envelope 不再接受 fractional numeric request id；统一
handler status/result、所有方法参数契约和 Plan 01 父级门禁仍需后续验收。
