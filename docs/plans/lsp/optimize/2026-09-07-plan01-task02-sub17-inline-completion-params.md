---
related_code:
  - zr_vm_language_server/stdio/stdio_inline_completion.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub17: Inline Completion Params

## 状态与产出记录

- 开始时间: 2026-09-07 08:05 +08:00
- 实际完成时间: 2026-09-07 08:25 +08:00
- 状态: 已完成
- 源码版本: 基于 `4b9c237c` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_inline_completion.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复协商启用的 `textDocument/inlineCompletion` 将 malformed params
静默降级为空数组的问题。handler 现在在 `textDocument` 或 `position` 解析失败
时返回 `NULL`，沿现有 dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS`
和 JSON-RPC `-32602`；合法 keyword-prefix 补全、comment/string code-span 过滤和
空结果语义保持不变。

## RED

新增带 `textDocument.inlineCompletion` 客户端能力的 protocol case，分别发送缺失、
`null`、标量、数组和缺失 `textDocument` 的 params。修复前 GCC server 对缺失 params
返回成功空数组，协议断言失败；其余 46 个 case 均通过。

## GREEN

`handle_inline_completion_request` 在 `get_uri_and_position` 失败时返回 `NULL`，由
dispatcher 生成 InvalidParams error envelope；内容快照、keyword-prefix 和
非代码 span 过滤路径不变。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  47/47 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  47/47 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 inline completion malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
