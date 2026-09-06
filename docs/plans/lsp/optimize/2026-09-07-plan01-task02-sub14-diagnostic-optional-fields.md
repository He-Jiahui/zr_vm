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

# Plan 01 Task 2 Sub14: Diagnostic Optional Fields

## 状态与产出记录

- 开始时间: 2026-09-07 06:52 +08:00
- 实际完成时间: 2026-09-07 07:08 +08:00
- 状态: 已完成
- 源码版本: 基于 `27b3c259` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_diagnostics.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 diagnostics 可选字段被静默忽略的问题。`textDocument/diagnostic` 的
`previousResultId` 和 `workspace/diagnostic` 的 `identifier` 若出现必须是字符串；
`previousResultIds` 必须是数组，且每个条目必须是含字符串 `uri` / `value` 的 object。
畸形值返回 `NULL`，沿现有 dispatcher status 路径映射为
`ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。省略字段、合法空数组和既有
full/unchanged report 语义保持不变。

## RED

新增 `invalid diagnostic optional params` case，覆盖 text document 的数值/null
`previousResultId`、workspace 的数值 `identifier`、非数组 `previousResultIds` 和含坏
条目的数组。修复前 GCC server 将 workspace 数值 identifier 忽略并返回成功 envelope，
协议断言失败；其余 43 个 case 均通过。

## GREEN

`stdio_diagnostics.c` 新增可选字符串字段和 workspace previous-result-id 数组验证，
并在 provider 诊断计算前执行。匹配 resultId 的 unchanged report、空 previous array、
workspace progress 与现有 snapshot/cancellation 路径继续复用原实现。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  44/44 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  44/44 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 diagnostics optional-field 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
