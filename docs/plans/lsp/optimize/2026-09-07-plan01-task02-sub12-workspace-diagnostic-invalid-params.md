---
related_code:
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub12: Workspace Diagnostic Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 06:20 +08:00
- 实际完成时间: 2026-09-07 06:32 +08:00
- 状态: 已完成
- 源码版本: 基于 `5a076655` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_diagnostics.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `workspace/diagnostic` 将缺失或畸形 params 伪装成成功空 workspace report
的问题。handler 现在要求 params 为 object，缺失、JSON `null`、标量和数组输入返回
`NULL`，沿现有 dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC
`-32602`。合法空 object、`previousResultIds` 以及 work-done/partial-result token 的
workspace report 和 progress 行为保持不变。

## RED

新增 `invalid workspace diagnostic params` case，初始化后分别发送缺失、`null`、标量和
数组 params。修复前 GCC server 对缺失 params 返回成功 envelope，协议断言失败；其余 41
个 case 均通过。

## GREEN

`handle_workspace_diagnostic_request` 在读取 `previousResultIds` 前拒绝非 object params，
因此错误只通过统一 dispatcher status/result 路径发布为 InvalidParams；object params
仍进入原有 workspace URI 遍历、report serialization、cancellation 和 partial-result
处理。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  42/42 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  42/42 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 `workspace/diagnostic` 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
