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

# Plan 01 Task 2 Sub19: Code Action Resolve Params

## 状态与产出记录

- 开始时间: 2026-09-07 09:05 +08:00
- 实际完成时间: 2026-09-07 09:25 +08:00
- 状态: 已完成
- 源码版本: 基于 `94bf0c38` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_editing.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `codeAction/resolve` 将缺失或畸形 action item 当作 stale action
返回 disabled object 的问题。handler 现在要求 params 为 object 且包含完整、可解析的
snapshot data；形状无效时返回 `NULL`，沿现有 dispatcher status 路径映射为
`ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。合法 current snapshot 继续
返回 action，合法但过期的 snapshot 继续保留 disabled-action 语义。

## RED

新增缺失、`null`、标量、数组、空 item、空 data 和 `null data` 的 resolve cases。
修复前 GCC server 对缺失 params 返回成功 envelope，协议断言失败；其余 48 个 case
均通过。

## GREEN

`handle_code_action_resolve_request` 先检查 server、params object 和
`parse_code_action_document_snapshot`，只把通过 token 结构校验的 item 交给 snapshot
validation；validation 失败仍调用 `disable_stale_code_action`，保持 stale action 的
安全降级。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  49/49 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  49/49 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 code action resolve malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
