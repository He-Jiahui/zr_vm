---
related_code:
  - zr_vm_language_server/stdio/stdio_navigation.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub16: References Context

## 状态与产出记录

- 开始时间: 2026-09-07 07:35 +08:00
- 实际完成时间: 2026-09-07 07:55 +08:00
- 状态: 已完成
- 源码版本: 基于 `b16b22b0` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_navigation.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `textDocument/references` 将缺失或畸形 `context` 静默降级为
`includeDeclaration=false` 的问题。handler 现在要求 `context` 为 object 且
`includeDeclaration` 为 boolean；缺失、`null`、标量、空 object 或数值字段返回
`NULL`，沿现有 dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和
JSON-RPC `-32602`。合法 `{includeDeclaration:true/false}` 继续保留 references
provider 和 partial-result 语义。

## RED

新增 `invalid references context` case，使用有效 URI/position 分别发送缺失、`null`、
标量、空 object 和数值 `includeDeclaration`。修复前 GCC server 对缺失 context 返回
成功 envelope，协议断言失败；其余 45 个 case 均通过。

## GREEN

`handle_references_request` 在 provider 查询前验证 `context` object 和
`includeDeclaration` boolean。有效 reference requests 继续使用 canonical locations、
cancellation 和 partial-result path。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  46/46 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  46/46 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 references context 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
