---
related_code:
  - tests/language_server/lsp_capability_inventory_probe.c
  - tests/language_server/lsp_native_inventory_contract.js
  - tests/language_server/lsp_native_inventory_mutations.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_capability_snapshot.js
  - tests/CMakeLists.txt
implementation_files:
  - tests/CMakeLists.txt
  - tests/language_server/lsp_capability_inventory_probe.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/lsp_native_inventory_contract.js
  - tests/language_server/lsp_native_inventory_mutations.js
doc_type: milestone-record
---

# Compiled Native Capability Inventory

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 21:34 +08:00 | 2026-09-05 22:35 +08:00 | completed (native inventory subitem; parent pending) | 从实际 registry、stdio initialize、native dispatcher 和 CTest 注册生成可复现清单，并对四个客户端协商 profile 做精确负向检查。 | registry 30 条、native route 43 条、metadata-only control 3 条、orphan 0；GCC/Clang/MSVC focused 14/14，四 profile 均为 `native-contract-mapped`。 |

## RED 与修复

最初的编译期 dispatch inventory 在四个 profile 都发现
`workspace/executeCommand` 没有 capability owner。该 orphan 由 Plan 00
Task 4 Sub06 移除；其余方法仍通过生产 dispatcher 的 method selection，
handler body 只在 probe 中替换为可观察的 contract stub，因此不会把测试桩
误认为生产 payload。

第一版 CTest 集成使用空的 `${CMAKE_CTEST_COMMAND}`，在只调用
`enable_testing()` 的配置中没有得到可执行路径。测试配置改为
`find_program(ZR_VM_CTEST_EXECUTABLE NAMES ctest REQUIRED)`，并将绝对路径
传给 Node runner。独立 mutation review 还复现了 boolean-only capability
option 接受 `{}` 的问题；contract 现在对 `rangesSupport`、`prepareProvider`、
`full.delta` 和 `workspaceDiagnostics` 做精确布尔检查，`willRenameFiles`
等对象形状仍按对象能力验证。

## Native Contract

`lsp_capability_inventory_probe.c` 编译实际 registry，并包含生产
`stdio_request_dispatch.c` 的 method-selection 逻辑。它输出 schema version、
全部 descriptor、semantic-token legend 和 native route，同时遍历 inline
completion/ranges formatting 的四种 gate 状态。Node contract 随后：

- 对 30 个 descriptor 检查 core/native/WASM metadata、协议版本、resolve
  runtime 和已注册 CTest ID；
- 将 initialize JSON 与完整 snapshot 做 deep equality，并验证 13 个 token
  type 和 `declaration` modifier；
- 将 43 条 native route 与实际 handler、optional gate 和 extension route
  一一对应；
- 把 `initialize`、`didChange` 和 workspace-folder notification 标为
  metadata-only control，避免未实现的通知探测被误报为通过；
- 对每个协商 profile 运行故意损坏的 metadata、snapshot、route、token、
  encoding 和 registration 变异，必须全部被拒绝。

四个 profile 的 mutation rejection 数量分别为：3.17 为 26、inline-only
为 26、ranges-only 为 27、both-3.18 为 27。所有 profile 的报告均为
`native-contract-mapped`，`failures` 和 `orphaned` 都是空数组。

## Verification

验证源码以已提交的 `c95e5387` 导出为基础，叠加本轮明确拥有的 34 个
LSP/protocol 路径以及已提交的能力撤回修复（截至 `2dfaa960`），同时删除
已撤回的 color/executeCommand 实现。提交时共享 `main` 为 `3d7b094f`；
隔离源码不包含其他会话尚未提交的 parser/core/semantic 修改。

使用隔离的 Debug 构建目录重新配置并生成 CTest 命令：

```text
GCC:   /usr/bin/ctest
Clang: /usr/bin/ctest
MSVC:  D:/Tools/development/cmake/bin/ctest.exe
```

三套工具链都重新构建了 stdio server、inventory probe 和相关测试目标，
然后运行同一组 14 个 focused CTest：

| Toolchain | Focused CTest | Direct inventory |
| --- | --- | --- |
| GCC 11.4 | 14/14, exit 0, 8.32 s | `native-contract-mapped` |
| Clang 14 | 14/14, exit 0, 8.59 s | `native-contract-mapped` |
| MSVC 19.44 | 14/14, exit 0, 18.68 s | `native-contract-mapped` |

JavaScript syntax checks for the inventory runner, contract, mutation checker,
snapshot and optional-capability smoke also pass. The generated CTest inventory
contains the absolute CTest path and the expected Debug configuration argument.

## Scope Boundary

This subitem proves the native registry/initialize/dispatch relationship. It does
not prove that the named WASM export is linked into a current asset, that the Web
worker registers the same route, or that control and notification handlers have
been behaviorally exercised. Those checks remain under
[Plan 05 native/Web parity](05-native-web-capability-parity.md). Complete
semantic payload acceptance remains under the editor and semantic plans.

The broad `language_server_stdio_smoke` still reaches its pre-existing generic
completion-detail failure, and the extra strict worker typecheck still reports
17 baseline/current diagnostics outside the extension tsconfig. Both are recorded
with owners and next plans in the [acceptance record](../../../../tests/acceptance/2026-09-05-lsp-native-capability-inventory.md).

No language syntax, compiler fact, or production semantic behavior changed in
this subitem.
