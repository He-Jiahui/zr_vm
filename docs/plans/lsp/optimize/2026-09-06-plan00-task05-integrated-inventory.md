---
plan_source: docs/plans/lsp/optimize/00-baseline-and-contract.md
scope: task-5-integrated-capability-inventory
tests:
  - language_server_stdio_protocol_inventory
  - language_server_wasm_capability_inventory
doc_type: acceptance-record
---

# Plan 00 Task 5: Integrated Capability Inventory

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-06 09:05 +08:00 | 2026-09-06 09:15 +08:00 | completed (static integrated inventory; linked asset pending) | `stdio_protocol_inventory.js` 现在在 native registry/initialize/dispatch/CTest 映射后，同一进程调用 WASM 静态 capability inventory，并输出统一的 machine-readable 子报告。 | GCC、Clang、MSVC 的 `language_server_stdio_protocol_inventory` 和 `language_server_wasm_capability_inventory` 均为 1/1；独立 runner 输出 `integrated-contract-mapped`，四个 native 协商 profile 均无 orphan/overclaim，WASM 静态报告为 30/28/22/13；`linkedAssetChecked: false`。 |

## 集成契约

`stdio_protocol_inventory.js` 继续使用编译出的 capability probe、生产 stdio
server 和配置的 CTest 元数据验证四个 native 协商 profile。完成 native 检查后，
runner 使用当前 Node 解释器调用 `wasm_capability_inventory.js`，把静态 CMake
导出、WASM C++ 定义/声明、bridge 调用、worker 路由和 semantic-token legend
结果嵌入同一 JSON 输出：

```json
{
  "status": "integrated-contract-mapped",
  "wasm": {
    "status": "wasm-static-contract-mapped",
    "runtimeExports": 30,
    "bridgeCalls": 28,
    "workerRoutes": 22,
    "semanticTokenTypes": 13,
    "linkedAssetChecked": false
  }
}
```

`status` 只有在 native profile 和 WASM 静态映射均通过时才为
`integrated-contract-mapped`；任一 native profile 失败时为
`integrated-contract-failed`。当生成的 `.js`/`.wasm` 路径尚未提供时，报告
明确保留 `linkedAssetChecked: false`，不会把源码映射当作链接资产证据。

## Verification

```text
node --check tests/language_server/stdio_protocol_inventory.js
node tests/language_server/stdio_protocol_inventory.js \
  <stdio-server> <inventory-probe> <build-directory> <absolute-ctest> Debug
ctest --test-dir <build-directory> --output-on-failure \
  -R "language_server_(stdio_protocol_inventory|wasm_capability_inventory)"
```

当前 GCC Debug 构建的直接 runner 报告四个 profile 的 mutation rejection
为 26/26/27/27，native registry 30 条、native routes 43 条、metadata-only
control 3 个、orphan 0；WASM 子报告为 30 runtime exports、28 bridge calls、
22 worker routes 和 13 token types。GCC、Clang、MSVC 的 integrated inventory
CTest 各 1/1，三个构建的独立 WASM 静态 CTest 也各 1/1。

## 范围边界

本记录关闭 Plan 00 Task 5 的静态集成 inventory 子项，并保留所有失败责任的
独立 owner。真实 `.wasm` 链接导出表、worker 加载生成资产、三种运行方式的
行为 parity、control/notification 语义和完整 semantic acceptance 仍未验收；
因此 Plan 00 及后续阶段不因本记录整体晋级。
