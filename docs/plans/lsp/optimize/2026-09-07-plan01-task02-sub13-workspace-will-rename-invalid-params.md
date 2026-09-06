---
related_code:
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub13: Workspace Will Rename Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 06:34 +08:00
- 实际完成时间: 2026-09-07 06:48 +08:00
- 状态: 已完成
- 源码版本: 基于 `3be84df2` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_workspace_files.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 `workspace/willRenameFiles` 将缺失或畸形 params、files 和文件 URI 伪装成
成功 JSON `null` 的问题。handler 现在要求 params 为 object、`files` 为 array，且每个
文件项为 object 并提供字符串 `oldUri` / `newUri`；解析失败返回 `NULL`，沿现有 dispatcher
status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。合法空 files、
同 URI 无编辑以及真实 workspace edit 仍保留原有结果语义。

## RED

新增 `invalid workspace will rename params` case，初始化后分别发送缺失、`null`、标量、
数组 params，缺失/null/scalar files 以及 malformed file item。修复前 GCC server 对缺失
params 返回成功 envelope，协议断言失败；其余 42 个 case 均通过。

## GREEN

`handle_will_rename_files_request` 在遍历前拒绝非 object params 或非数组 files，并在每个
文件项读取 URI 前验证 object、`oldUri` 和 `newUri` 字符串形状。有效请求继续执行已有
source-rename planning、snapshot validation 和 no-edit `null` 返回。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  43/43 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  43/43 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 `workspace/willRenameFiles` 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
