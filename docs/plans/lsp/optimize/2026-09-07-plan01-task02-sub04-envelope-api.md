---
related_code:
  - zr_vm_language_server/stdio/stdio_json_rpc.c
  - zr_vm_language_server/stdio/stdio_json_rpc.h
  - tests/language_server/test_stdio_server_lifecycle.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub04: JSON-RPC Envelope API

## 状态与产出记录

- 开始时间: 2026-09-07 04:53 +08:00
- 实际完成时间: 2026-09-07 04:57 +08:00
- 状态: 已完成
- 源码版本: 基于 `e61b2fed` 的当前工作树；本记录与回归测试由同一阶段提交固化
- 产出路径: `tests/language_server/test_stdio_server_lifecycle.c`、模块文档、计划索引与本记录

本子项为 `ZrLanguageServer_StdioJsonRpc_ParseEnvelope` 增加直接 C 回归，锁定
顶层消息、jsonrpc 版本、request id、params 形状和 request/notification 分类。
它只验证 envelope 模块边界，不宣称 handler status/result 已统一或 Plan 01 Task 2
父级门禁已验收。

## 覆盖边界

测试确认：

- 数组/标量顶层消息以及缺失或错误 `jsonrpc` 版本返回 `INVALID_REQUEST`；
- bool/object/array id 返回 `INVALID_REQUEST` 和空 error id，合法 request id 在错误时保留；
- scalar params 返回 `INVALID_PARAMS` 并保留 request id；
- object/array params 被接受，method、params 和 typed id 原样投影到 envelope；
- 缺失 id 被分类为无响应 notification，显式 JSON `null` id 保持为 request。

## 验证命令及结果

```text
wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  passed
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure -R "^language_server_stdio_server_lifecycle$"'
  1/1 passed

wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  passed
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --output-on-failure -R "^language_server_stdio_server_lifecycle$"'
  1/1 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件和文档无空白错误。GCC/Clang 的现有 34-case
protocol conformance 也已在同一阶段通过。

## 接受决定

接受 Plan 01 Task 2 Sub04。envelope API 的分类和 ID/params 投影现在有直接 C 回归；
统一 handler status/result、方法级参数契约和 Plan 01 父级门禁仍需后续验收。
