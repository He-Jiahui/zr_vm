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
  - language_server_stdio_optional_capabilities_smoke
doc_type: plan-record
---

# Plan 01 Task 2 Sub21: Ranges Formatting Params

## 状态与产出记录

- 开始时间: 2026-09-07 08:16 +08:00
- 实际完成时间: 2026-09-07 08:30 +08:00
- 状态: 已完成
- 源码版本: 基于 `59901684` 的当前工作树；实现与回归测试提交 `367e9b5a`
- 产出路径: `stdio_editing.c`、`stdio_protocol_conformance.js`、模块文档、计划 checkbox 与本记录

本子项修复协商启用的 `textDocument/rangesFormatting` 将缺失或畸形参数静默
降级为空编辑数组的问题。handler 现在要求 object params、可解析的
`textDocument.uri` 和 `ranges` array；每个 range 都必须通过 canonical range parser。
任一必需字段缺失、类型错误、逆序 range 或混合数组中的单个 range 无法解析时返回
`NULL`，沿 dispatcher status 路径映射为 `ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC
`-32602`。合法空 ranges 和 provider 无编辑结果仍返回成功空数组；未协商该可选能力时
继续由 capability gate 返回 `-32601 Method not found`。

## RED

新增带 `textDocument.rangeFormatting.rangesSupport` 客户端能力的 protocol case，
覆盖缺失、`null`、标量、数组 params，缺失 URI/ranges，非数组 ranges，以及 null、
标量、数组、逆序和有效 range 后接无效条目的 ranges。修复前 GCC server 对缺失
params 返回成功空数组，协议断言失败；其余 50 个 case 均通过。

## GREEN

`handle_ranges_formatting_request` 在 URI 和 ranges array 校验失败时返回 `NULL`，并
在遍历中遇到任一 range 解析失败时释放已创建的 result 后返回 `NULL`。完整 ranges
provider 仍按既有顺序聚合各 range 的 text edits；空 ranges 不调用 provider，返回合法
空数组。

## 验证命令及结果

```text
node --check tests/language_server/stdio_protocol_conformance.js
  passed

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  51/51 passed, exit 0

node tests/language_server/stdio_protocol_conformance.js \
  .codex/lsp-optimize-validation/msvc-current/bin/zr_vm_language_server_stdio.exe
  51/51 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /tmp/zr-lsp-plan01-sub21-clang-20260907/bin/zr_vm_language_server_stdio
  51/51 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure \
  -R "^language_server_stdio_(protocol_conformance|server_lifecycle|optional_capabilities_smoke)$"
  3/3 passed

ctest --test-dir .codex/lsp-optimize-validation/msvc-current --output-on-failure \
  -R "^language_server_stdio_(protocol_conformance|server_lifecycle|optional_capabilities_smoke)$"
  3/3 passed

ctest --test-dir /tmp/zr-lsp-plan01-sub21-clang-20260907 --output-on-failure \
  -R "^language_server_stdio_(protocol_conformance|server_lifecycle|optional_capabilities_smoke)$"
  3/3 passed, no sanitizer diagnostic
```

`git diff --check` 对本子项源文件、测试和文档无空白错误。

共享 `.codex/lsp-optimize-validation/clang-asan-current` 在一次重跑中发生
`libzr_vm_language_server.so: file too short`，其失败不计为通过证据。最终显式使用
`wsl.exe -d Ubuntu-22.04 -- bash -lc` 从当前工作树创建独立
`/tmp/zr-lsp-plan01-sub21-clang-20260907`，保留 `-fsanitize=address,undefined`、
`-fno-omit-frame-pointer` 和 `-no-pie`，完成全依赖构建及上述 51/51、3/3 回放。
三工具链构建均包含当前未提交依赖，属于本子项兼容验证，不替代全计划同一已提交版本
的最终验收。

## 接受决定

接受协商启用的 ranges formatting malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
