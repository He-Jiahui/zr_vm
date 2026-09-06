---
related_code:
  - zr_vm_language_server/stdio/stdio_json_rpc.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_request_registry.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 4 Sub02: Safe Numeric Request IDs

## 状态与产出记录

- 开始时间: 2026-09-07 02:30 +08:00
- 实际完成时间: 2026-09-07 03:05 +08:00
- 状态: 已完成
- 源码版本: 基于 `12ea51fe` 的当前工作树；本记录与实现由同一阶段提交固化
- 产出路径: `stdio_json_rpc.c`、`stdio_transport.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项修复 JSON-RPC 数字 request ID 的精度和可接受范围。它只关闭数字
ID 的安全边界与响应回写契约，不宣称完成 Task 4 的重复活动 ID、取消、进度
或 ContentModified 父任务。

## RED

新增协议用例先运行于旧 MSVC Debug server。原始 JSON request 使用精确的
安全上界 `9007199254740991`，响应中的 ID 变成 `9007199254740990`；旧实现
也接受超出安全范围的 `9007199254740992`。根因是 cJSON 将数字保存为
`double`，默认整数输出路径使用缓存的整数值，响应回写不能保持 JSON 数字
的可区分值。

## 实现

`stdio_json_rpc.c` 现在只接受有限且位于
`+/-ZR_LSP_JSON_SAFE_INTEGER_MAX`（`9007199254740991`）范围内的数字 ID。
越界数字在 envelope 阶段返回 InvalidRequest；其错误响应使用 `id: null`。
`stdio_transport.c` 对合法数字 ID 使用 `%.17g` 构造 cJSON raw 节点，保留
足够的有效数字以完成 double round-trip；字符串 ID 和数字 ID 仍由现有
request registry 按不同 kind 存储和比较。

## 契约

- 安全范围内的有限数字 ID 可以作为 request ID，并按数值身份回显。
- `9007199254740991` 必须回显为同一个 JSON 数值；`9007199254740992`
  以及对应的负向越界值返回 `-32600 Invalid Request` 与 `id: null`。
- 数字 `1` 与字符串 `"1"` 继续是不同的 request identity。

## 验证命令及结果

工具链:

- GCC 11.4.0 Debug: `.codex/build-lsp-opt-gcc`
- Clang 14.0.0 ASan/UBSan、`-no-pie`: `.codex/lsp-optimize-validation/clang-asan-current`
- MSVC 19.44.35228.0 Debug: `.codex/lsp-optimize-validation/msvc`

```text
node --check tests/language_server/stdio_protocol_conformance.js
  passed

node tests/language_server/stdio_protocol_conformance.js
    .codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
  31/31 passed

WSL node tests/language_server/stdio_protocol_conformance.js
    .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  31/31 passed

ctest --test-dir .codex/build-lsp-opt-gcc --output-on-failure
    -R ^language_server_stdio_protocol_conformance$
  1/1 passed

ctest --test-dir .codex/lsp-optimize-validation/clang-asan-current
    --output-on-failure -R ^language_server_stdio_protocol_conformance$
  1/1 passed; no ASan/UBSan diagnostics
```

The MSVC overlay was rebuilt after copying the two C implementation files and
the current protocol driver. The GCC and Clang binaries were rebuilt from the
current source tree before their direct/CTest runs.

## 接受决定

接受 Plan 01 Task 4 Sub02。安全边界数字 ID 能够精确关联响应，越界数字不会
进入请求处理或 registry，协议驱动在三工具链通过 31/31。Plan 01 Task 4
及 Plan 01 整体仍因其余 request registry、取消、ContentModified、frame 和
teardown 门槛保持进行中。
