---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_diagnostic_store.h
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/stdio_diagnostics_generation_smoke.js
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server_extension/test/serverDiagnostics.test.js
doc_type: milestone-record
---

# Plan 02 Task 6: Pull/Push Diagnostics

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 18:45 +08:00 | 已完成 | 统一 native/Web pull diagnostics identity、workspace 索引枚举、invalid-params、依赖代际失效和版本感知 push 去重。 | GCC、Clang、fresh MSVC 的 generation smoke 与 stdio smoke 均以真实 exit 0 完成；浏览器 bridge 编译和静态测试通过，WASM export objects 编译并含两个 diagnostics export。 |

## Delivered Contract

- `ZrLanguageServer_LspDiagnosticStore_BuildResultId` 是 native 与 WASM
  共同使用的唯一 resultId 生成器。它包含 document/project/provider/semantic/
  dependency snapshot identity，并对完整 structured diagnostics payload 做稳定排序
  哈希；range、severity、descriptor/code、message、related information 与 fixes 都在
  payload 内。非语义文档从 file content generation 构造 document identity，不能退化为
  空 resultId。
- `textDocument/diagnostic` 缺少或无效 `textDocument.uri` 时返回 JSON-RPC
  `InvalidParams (-32602)`，不会伪造 `full` 空报告。匹配 `previousResultId` 时才返回
  `unchanged`。
- `workspace/diagnostic` 从 project index 收集所有已索引 source URI，并去重合并打开
  overlay；未打开文件以 `version: null` 返回。每一个长循环都检查请求取消，已有
  `previousResultIds`、work-done 与 partial-result plumbing 保持生效。
- 依赖/provider/project identity 进入 resultId，因此 provider 更新即使未改变 importer
  本文，也会令 importer 的下一次 pull 返回新 identity。push cache 仅在 resultId 与
  打开文档版本都相同的时候抑制重复通知，因而 pull 和 push 可以并存而不互相吞掉新代际。
- Browser worker 删除自有 text hash 与本地 workspace 枚举。它通过 WASM bridge 调用
  `wasm_ZrLspGetDiagnosticReport` 和
  `wasm_ZrLspGetWorkspaceDiagnosticReports`，与 native 共用 C diagnostic store。

## Validation

- GCC Debug shared 和 Clang Debug shared 都构建 `zr_vm_language_server_stdio`，随后以
  真实 process exit 0 运行 `stdio_diagnostics_generation_smoke.js` 和完整
  `stdio_smoke.js`。后者报告 warm diagnostics、100-file workspace diagnostics 和
  process peak-memory 指标。
- MSVC 19.44.35228 fresh Debug shared directory
  `.codex/build-lsp-task6-msvc-fresh` 构建 stdio、CLI 与 descriptor plugin；两个
  diagnostics smoke 均 exit 0。此前增量缓存的初始化 access violation 经 fresh full
  rebuild 消除，不作为通过证据。
- `npm run compile` 与
  `node --test test/serverDiagnostics.test.js` 均 exit 0，静态测试确认 worker 不含
  TypeScript `hashText`/`createDiagnosticResultId`，并以 `{ resultId, version }` 管理
  push cache。
- Emscripten 对 `lsp_diagnostic_store.c` 和 `wasm_exports.cpp` 的当前对象编译 exit 0；
  `llvm-nm` 确认两个 WASM diagnostic export。完整 WASM module link 与 browser
  end-to-end smoke 仍由 Task 7 的全局 WASM gate 负责：当前受限环境的全图 Debug link
  超过单次十分钟执行窗口，不能视为该验收已通过。

## Scope Boundary

此记录只完成 Plan 02 Task 6 的实现与 focused cross-toolchain gate。Task 7 仍负责
统一提交基线上的完整 WASM/browser runtime、URI/position differential、10,000 edit
sequence 与 multi-root acceptance matrix。
