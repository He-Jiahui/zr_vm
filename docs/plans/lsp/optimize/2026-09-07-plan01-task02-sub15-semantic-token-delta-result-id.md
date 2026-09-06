---
related_code:
  - zr_vm_language_server/stdio/stdio_semantic_tokens.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub15: Semantic Token Delta Result Id

## 状态与产出记录

- 开始时间: 2026-09-07 07:15 +08:00
- 实际完成时间: 2026-09-07 07:31 +08:00
- 状态: 已完成
- 源码版本: 基于 `eb18afb0` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_semantic_tokens.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `textDocument/semanticTokens/full/delta` 将缺失或畸形
`previousResultId` 伪装成可计算 delta 的问题。handler 现在要求该字段为字符串，
缺失、JSON `null`、数字和数组返回 `NULL`，沿现有 dispatcher status 路径映射为
`ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。合法 result id 继续消费 token
snapshot，返回 unchanged 或 minimal delta；full/range 的既有 URI/range 契约不变。

## RED

新增 `invalid semantic token delta result id` case，使用有效 `textDocument.uri` 分别
发送缺失、`null`、数字和数组 `previousResultId`。修复前 GCC server 对缺失值返回成功
envelope，协议断言失败；其余 44 个 case 均通过。

## GREEN

`handle_semantic_tokens_full_delta_request` 在获取 URI 后、snapshot/token provider 计算前
验证 `previousResultId` 的字符串形状并传递已验证文本；合法 delta identity 的缓存查找、
unchanged 判断、最小 edit 和 resultId 更新逻辑保持原实现。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  45/45 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  45/45 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 semantic token delta result-id 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
