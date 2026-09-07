---
related_code:
  - zr_vm_language_server/stdio/stdio_editing.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub20: Code Action Context

## 状态与产出记录

- 开始时间: 2026-09-07 09:35 +08:00
- 实际完成时间: 2026-09-07 09:55 +08:00
- 状态: 已完成
- 源码版本: 基于 `130e1969` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_editing.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `textDocument/codeAction` 忽略必需 `context` 结构的问题。handler 现在
要求 `context` 为 object、`diagnostics` 为 object array；可选 `only` 出现时必须为
string array。缺失或畸形值在 snapshot/provider 工作前返回 `NULL`，沿现有 dispatcher
status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。合法
quickfix、organize-import 和 `only` 过滤语义保持不变。

## RED

新增已打开文档上的 code action cases，分别发送缺失、`null`、标量、数组 context，
缺失或畸形 diagnostics，以及标量/非字符串 only。修复前 GCC server 对缺失 context
返回成功 envelope，协议断言失败；其余 49 个 case 均通过。

## GREEN

`handle_code_action_request` 在 URI 解析后检查 context、diagnostics 条目和 only 条目，
再捕获 document snapshot、解析 range 并调用 provider；合法路径继续由
`serialize_code_actions_array` 应用 `only` kind filter。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  50/50 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  50/50 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 code action context 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
