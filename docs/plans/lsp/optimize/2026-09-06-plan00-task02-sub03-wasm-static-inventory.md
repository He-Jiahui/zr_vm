---
related_code:
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server/wasm/wasm_exports.h
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - tests/language_server/wasm_capability_inventory.js
implementation_files:
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - tests/CMakeLists.txt
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/wasm_capability_inventory.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
doc_type: milestone-record
---

# WASM Static Capability Inventory

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-06 01:00 +08:00 | 2026-09-06 04:12 +08:00 | completed (static inventory subitem; linked asset pending) | 对 CMake 导出列表、WASM C++ 定义/声明、bridge `ccall`、worker handler 和 semantic-token legend 建立可重复检查，并修复 Web legend 漂移。 | `wasm-static-contract-mapped`；30 个 runtime export、28 个 bridge call、22 条 worker route、13 个 token type 和 `declaration` modifier 均通过；extension unit 41/41。 |

## RED 与修复

新增 capability 回归首先以 RED 暴露 Web worker 的三处漂移：缺少
`decorator`/`metaMethod`、缺少 `declaration` modifier，以及未声明实际
支持的 native semantic full delta/range 形状。worker 只实现 full token
请求，因此修复只补齐 legend，并保留 `full: true`，不引入未实现的请求。

`serverCapabilities.test.js` 使用跨 realm `vm` 加载 worker；测试比较数组和
字段值而不是对象原型，避免把正确的结构误判为失败。修复后的单文件测试
10/10，通过完整 extension unit 41/41。

## 静态契约

`wasm_capability_inventory.js` 从 `zr_vm_language_server/CMakeLists.txt` 解析
`EXPORTED_FUNCTIONS_JSON`，并将其与 `wasm_exports.cpp/.h` 的定义/声明和
`wasm-bridge.ts` 的调用逐项比较；随后检查 worker 的 22 个公共路由都调用
已存在 bridge method，并指向已声明 export。当前输出为：

```json
{
  "status": "wasm-static-contract-mapped",
  "runtimeExports": 30,
  "bridgeCalls": 28,
  "workerRoutes": 22,
  "semanticTokenTypes": 13,
  "semanticTokenModifiers": ["declaration"],
  "linkedAssetChecked": false
}
```

当同时传入生成的 `.js` 和 `.wasm` 路径时，脚本会再使用
`WebAssembly.Module.exports` 比较真实链接导出表；没有资产时明确保持
`linkedAssetChecked: false`，不把源码字符串当成链接证据。CTest 已注册
无资产的静态模式：`language_server_wasm_capability_inventory`。

## Verification

- `node tests/language_server/wasm_capability_inventory.js`：通过，输出
  `wasm-static-contract-mapped`；脚本兼容 WSL Node 10 及 Windows Node 22。
- `node --test zr_vm_language_server_extension/test/*.test.js`：41/41。
- `node --check tests/language_server/wasm_capability_inventory.js`：通过。
- `git diff --check`：通过（提交前对本子项路径执行）。

尝试从同一隔离源码构建真实 WASM 时，Emscripten 在 415/610 个对象处报错：
overlay 中找不到另一活动会话新增的
`zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.h`。工作树中该
文件存在但未提交，且不属于本子项；因此没有把它复制或暂存，也没有宣称
链接资产已验收。

## Scope Boundary

本记录关闭 Web capability 静态映射和 token legend 漂移，不关闭 Plan 00。
真实 `.wasm` export table、worker 加载新资产后的端到端调用、控制/通知行为、
WASM error/result parity 和完整 semantic acceptance 仍由 Plan 05/后续任务验收。
