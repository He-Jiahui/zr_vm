---
related_code:
  - zr_vm_language_server/stdio/stdio_navigation.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub11: Workspace Symbol Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 06:01 +08:00
- 实际完成时间: 2026-09-07 06:11 +08:00
- 状态: 已完成
- 源码版本: 基于 `df07e2f3` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_navigation.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `workspace/symbol` 将缺失 query 伪装成合法空查询的问题。handler 现在
要求 params 为 object 且 `query` 为字符串，缺失或畸形输入返回 `NULL`，沿现有
dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。
合法的空字符串 query 仍表示可执行的全量查询，provider 无结果仍返回合法空数组。

## RED

新增 `invalid workspace symbol params` case，初始化后向 `workspace/symbol` 发送空
object。修复前 GCC server 将缺失 query 当成空字符串并返回成功 envelope，协议断言
失败；其余 40 个 case 均通过。

## GREEN

`handle_workspace_symbols_request` 现在拒绝非 object params、缺失 query、非字符串
query 以及无法取得字符串值的 query。字符串（包括 `""`）继续创建查询并走原有
workspace symbol provider。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  41/41 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  41/41 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

Clang 的独立 protocol replay 与 CTest 均在重新生成完整共享库后通过；一次并行构建
期间的临时 `file too short` 产物不作为代码失败证据。

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 `workspace/symbol` 的 malformed-params 分类子项。Task 2 的完整统一 handler
status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持 pending。
