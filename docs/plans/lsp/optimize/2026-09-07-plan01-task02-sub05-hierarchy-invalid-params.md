---
related_code:
  - zr_vm_language_server/stdio/stdio_hierarchy.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub05: Hierarchy Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 05:42 +08:00
- 实际完成时间: 2026-09-07 05:55 +08:00
- 状态: 已完成
- 源码版本: 当前 `main` 的修复工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_hierarchy.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 hierarchy handler 将参数解析失败伪装成成功空数组的问题。缺失或
畸形的 `textDocument/position`、`item` 现在返回 `NULL`，沿现有 dispatcher status
路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。语义查询本身
没有结果时仍返回合法空数组；handler ABI 和 capability inventory 不变，因此本
子项不宣称所有 provider 已完成统一 status/result 迁移。

## RED

新增 `invalid hierarchy params` case，依次发送 call hierarchy/type hierarchy 的
prepare、incoming、outgoing、supertypes 和 subtypes malformed params。修复前 GCC
server 对首个 prepare request 返回带 `result: []` 的成功 envelope，协议断言失败；
其余原有 34 个 case 均通过。

## GREEN

`handle_prepare_hierarchy_request` 在 `get_uri_and_position` 失败时，五个 item-based
handler 在 `parse_hierarchy_item` 失败时均返回 `NULL`。dispatcher 已有的 NULL-to-status
边界随后发送精确 `-32602 Invalid params`，而 provider 查询失败分支仍保留空数组结果。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  35/35 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  35/35 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 hierarchy provider 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、所有方法级 params 契约和 Plan 01 父级门禁仍保持 pending。
