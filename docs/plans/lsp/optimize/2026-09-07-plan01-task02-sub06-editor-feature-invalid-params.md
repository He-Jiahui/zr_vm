---
related_code:
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 2 Sub06: Editor Feature Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 05:19 +08:00
- 实际完成时间: 2026-09-07 05:19 +08:00
- 状态: 已完成
- 源码版本: 基于 `3b458567` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_editor_features.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 editor-feature provider 将输入解析失败伪装成成功空数组的问题。
implementation、foldingRange、selectionRange、documentLink 和 codeLens 的缺失或
畸形 `textDocument`/`positions` 参数现在返回 `NULL`，沿现有 dispatcher status
路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。provider 查询
没有结果时仍返回合法空数组；分配失败和实际 provider 失败没有在本子项中重新分类。

## RED

新增 `invalid editor feature params` case，初始化后依次向五个方法发送空 object。
修复前 GCC server 对 implementation 返回带 `result: []` 的成功 envelope，协议断言
失败；其余 35 个 case 均通过。

## GREEN

共享 implementation location helper 与 foldingRange、selectionRange、documentLink、
codeLens handler 的输入解析失败分支现在返回 `NULL`。selectionRange 的空 positions
数组仍按既有空结果处理，只有缺失/错误数组或位置形状触发 InvalidParams。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  36/36 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  36/36 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受五个 editor-feature provider 的 malformed-params 分类子项。Task 2 的完整
统一 handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁
仍保持 pending。
