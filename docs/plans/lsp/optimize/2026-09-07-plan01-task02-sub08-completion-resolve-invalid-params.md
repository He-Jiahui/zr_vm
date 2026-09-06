---
related_code:
  - zr_vm_language_server/stdio/stdio_completion.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub08: Completion Resolve Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 05:31 +08:00
- 实际完成时间: 2026-09-07 05:45 +08:00
- 状态: 已完成
- 源码版本: 基于 `be6ac326` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_completion.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `completionItem/resolve` 将 item 解析失败伪装成成功原样复制的问题。
空对象以及缺失或畸形 label、resolve data URI 或 position 现在返回 `NULL`，沿现有
dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。
合法参数但没有匹配 completion item 时仍返回既有成功 item，未在本子项中改变 provider
无结果语义。

## RED

新增 `invalid completion resolve params` case，初始化后向 `completionItem/resolve`
发送空 object。修复前 GCC server 原样复制 `{}` 并返回成功 envelope，协议断言失败；
其余 37 个 case 均通过。

## GREEN

completion resolve handler 现在要求 params 为 object，并要求 label、resolve data 中的
URI 和 position 都能解析；任一输入失败即返回 `NULL`。合法但未匹配 item 的 fallback
仍复制原始 params 并返回成功结果。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  38/38 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  38/38 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 `completionItem/resolve` 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
