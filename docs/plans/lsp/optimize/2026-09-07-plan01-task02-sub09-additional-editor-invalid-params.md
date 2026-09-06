---
related_code:
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_moniker.c
  - zr_vm_language_server/stdio/stdio_linked_editing.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub09: Additional Editor Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 05:45 +08:00
- 实际完成时间: 2026-09-07 05:53 +08:00
- 状态: 已完成
- 源码版本: 基于 `ceac37f7` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_inline_value.c`、`stdio_moniker.c`、`stdio_linked_editing.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复三个始终声明的 editor provider 将请求解析失败伪装成成功 no-result 的问题。
inlineValue、moniker 和 linkedEditingRange 在缺失或畸形 URI、position 或 range 参数
时现在返回 `NULL`，沿现有 dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS`
和 JSON-RPC `-32602`。扫描或语义查询没有结果时仍分别返回合法空数组或 `null`，不在
本子项中改变 provider no-result 语义。

## RED

新增 `invalid additional editor params` case，初始化后向 inlineValue、moniker 和
linkedEditingRange 发送空 object。修复前 GCC server 对 inlineValue 返回带 `result: []`
的成功 envelope，协议断言失败；其余 38 个 case 均通过。

## GREEN

三个 handler 的请求解析失败分支现在返回 `NULL`。inlineValue 的快照获取、moniker 的
词扫描和 linkedEditingRange 的语义/文本范围计算在输入合法时保持原有空数组或 `null`
结果。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  39/39 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  39/39 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受三个 additional editor provider 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
