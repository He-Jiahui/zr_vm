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

# Plan 01 Task 2 Sub07: Editing Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 05:20 +08:00
- 实际完成时间: 2026-09-07 05:31 +08:00
- 状态: 已完成
- 源码版本: 基于 `3b95edbd` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_editing.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 editing provider 将输入解析失败伪装成成功空数组的问题。formatting、
onTypeFormatting 和 codeAction 在缺失或畸形 `textDocument` 参数时现在返回 `NULL`，
沿现有 dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC
`-32602`。provider 查询没有结果时仍返回合法空数组；分配失败和实际 provider
失败没有在本子项中重新分类。

## RED

新增 `invalid editing params` case，初始化后向 formatting、onTypeFormatting 和
codeAction 发送空 object。修复前 GCC server 对 formatting 返回带 `result: []` 的
成功 envelope，协议断言失败；其余 36 个 case 均通过。`rangesFormatting` 当前不在
能力矩阵中，仍由 capability gate 返回 `-32601 Method not found`，因此不纳入这个
方法级参数子项。

## GREEN

formatting、onTypeFormatting 和 codeAction handler 的 `textDocument` 输入解析失败
分支现在返回 `NULL`。dispatcher 将该返回值转换为 InvalidParams；合法请求在 provider
无结果时仍返回空数组。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  37/37 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  37/37 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受三个 editing provider 的 malformed-params 分类子项。Task 2 的完整统一 handler
status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持 pending。
