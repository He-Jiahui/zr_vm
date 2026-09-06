---
related_code:
  - zr_vm_language_server/stdio/stdio_semantic_tokens.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 2 Sub10: Semantic Token Invalid Params

## 状态与产出记录

- 开始时间: 2026-09-07 05:53 +08:00
- 实际完成时间: 2026-09-07 06:01 +08:00
- 状态: 已完成
- 源码版本: 基于 `4c3a3f01` 的当前工作树；实现与回归测试由同一阶段提交固化
- 产出路径: `stdio_semantic_tokens.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 semantic token range handler 将 URI/range 解析失败伪装成成功 `null` 的
问题。full、full/delta 已沿 `get_uri_from_text_document` 直接将缺失文档参数映射为
InvalidParams；range 现在同样返回 `NULL`，沿现有 dispatcher status 路径映射为
`ZR_LSP_HANDLER_INVALID_PARAMS` 和 JSON-RPC `-32602`。合法请求的 full、delta 和 range
响应结构以及 provider 无结果路径保持不变。

## RED

新增 `invalid semantic token params` case，初始化后向 semantic tokens full、full/delta
和 range 发送空 object。修复前 GCC server 对 range 返回带 `result: null` 的成功 envelope，
协议断言失败；其余 39 个 case 均通过。

## GREEN

`handle_semantic_tokens_range_request` 的 URI/range 解析失败分支现在返回 `NULL`；full 和
full/delta 的已有 URI 校验继续返回 `NULL`。semantic token provider 的合法 no-result
响应仍使用既有 `null` 或结构化空数据。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  40/40 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  40/40 passed, exit 0, no ASan/UBSan diagnostic

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed

ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "^language_server_stdio_(protocol_conformance|server_lifecycle)$"
  2/2 passed, no sanitizer diagnostic
```

Clang CTest 的首次 mounted-path replay 在 `invalid-hierarchy-initialize` 出现一次
启动超时；隔离重跑后完整 2/2 通过，协议单独回放也通过 40/40。

`git diff --check` 对本子项源文件、测试和文档无空白错误。

## 接受决定

接受 semantic token providers 的 malformed-params 分类子项。Task 2 的完整统一
handler status/result、其他 provider 的方法级 params 契约和 Plan 01 父级门禁仍保持
pending。
