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

# Plan 01 Task 2 Sub18: Code Action Range

## 状态与产出记录

- 开始时间: 2026-09-07 08:35 +08:00
- 实际完成时间: 2026-09-07 08:55 +08:00
- 状态: 已完成
- 源码版本: 基于 `31ba24eb` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_editing.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `textDocument/codeAction` 忽略 `range` 解析结果并以零值 range
继续请求 provider 的问题。handler 现在要求 canonical `range` 通过
`parse_range_for_uri`，缺失、`null`、标量、数组或逆序值返回 `NULL`，沿现有
dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。
合法 code action 继续使用请求 range、workspace edit snapshot 和现有 quickfix 语义。

## RED

新增已打开文档上的 `textDocument/codeAction` case，分别发送 `null`、标量、数组和
逆序 range，context 保持合法空 diagnostics。修复前 GCC server 对 `null range` 返回
成功 envelope，协议断言失败；其余 47 个 case 均通过。一次后续回放还观察到 GCC
共享库在并发构建窗口短暂为 `file too short`；重建目标后该环境问题消失。

## GREEN

`handle_code_action_request` 在 capture document snapshot 后、provider 查询前检查
`parse_range_for_uri` 返回值，失败立即返回 `NULL`；成功路径的 action serialization
和 snapshot validation 不变。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  48/48 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  48/48 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 code action range 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
