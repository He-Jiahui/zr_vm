# Native 与 Web/WASM 能力一致性实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `test-driven-development`, `webapp-testing`, `evidence-driven-wsl-validation`, `modularize-large-files`, and `verification-before-completion` while executing this plan.

**Goal:** 让 native stdio 与 Web/WASM 使用同一 capability registry、core request contract、snapshot/error semantics 和版本信息；运行时差异必须显式记录为受测试的降级，而不是静默漂移。

**Architecture:** core 层返回 transport-neutral result/status；native cJSON adapter 与 WASM JSON export 只序列化。TypeScript worker 从生成的 capability manifest 构造 InitializeResult，并将错误映射为 ResponseError。公共 capability 通过同一 JSON golden corpus 比较。

**Tech Stack:** C/WASM、Emscripten、TypeScript、vscode-languageserver 9.x、vscode-languageclient 8.x、VS Code desktop/web smoke。

---

## Task 1：固定协议版本与生成 capability manifest

**Files:**
- Modify: `zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h`
- Create: `zr_vm_language_server/tools/generate_lsp_capabilities.c`
- Create: `zr_vm_language_server_extension/src/browser/worker/generated-capabilities.json`
- Modify: `zr_vm_language_server/stdio/stdio_initialize.c`
- Modify: `zr_vm_language_server_extension/src/browser/worker/server-worker.ts`
- Test: `tests/language_server/test_lsp_capability_registry.c`
- Test: `zr_vm_language_server_extension/test/serverCapabilities.test.js`

- [ ] 稳定目标为 LSP 3.17；inlineCompletion、rangesFormatting 等 3.18 项标为 experimental，并要求 client capability 明确存在。
- [ ] registry 生成 native/WASM capability manifest；TypeScript 不再手写另一套 token legend 和 provider 列表。
- [ ] initialize 保存规范化 client capabilities：position encodings、markup kinds、completion item features、hierarchical symbols、semantic tokens、workspace edit、file operations、diagnostic、folding、inlay/resolve、experimental 3.18。
- [ ] 输出只包含客户端支持且当前 runtime 实现/测试存在的字段；未识别 client capability 采用 3.17 默认行为。
- [ ] server name/version 从单一 build/version header 生成，与 extension package version/release metadata 对齐。

## Task 2：共享 request/result/error contract

**Files:**
- Create: `zr_vm_language_server/include/zr_vm_language_server/lsp_request.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_request.c`
- Modify: `zr_vm_language_server/stdio/stdio_request_dispatch.c`
- Modify: `zr_vm_language_server/wasm/wasm_exports.cpp`
- Modify: `zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts`
- Modify: `zr_vm_language_server_extension/src/browser/worker/server-worker.ts`

- [ ] core result 使用计划 01 的 status + structured error data；WASM payload 保留 JSON-RPC error code，不只有 `success/error string`。
- [x] worker 的 `responseData(response, [])` fallback 不得把 InternalError、InvalidParams、Cancelled、ContentModified 转成“无结果”；使用 `ResponseError` 返回客户端。完成记录见 [Plan 05 Task 2 error contract](2026-09-06-plan05-task02-error-contract.md)。
- [x] “合法无结果”仅对应成功 null/empty；bridge parse failure、null pointer、stale snapshot 和 OOM 均为错误。当前 bridge 对缺少错误码的旧 payload 使用 InternalError，native WASM export 为每个错误 payload 写入 JSON-RPC code。
- [ ] rename/code action/workspace edit 从 core 返回 versioned edit plan；删除 Web `buildWorkspaceEdit(Location[])` 的无快照重建。
- [ ] native 和 WASM 对同一 request fixture 输出忽略字段顺序后相同的 JSON result/error。

## Task 3：补齐 Web 文档与工作区生命周期

**Files:**
- Create: `zr_vm_language_server_extension/src/browser/worker/workspace-state.ts`
- Modify: `zr_vm_language_server_extension/src/browser/worker/server-worker.ts`
- Modify: `zr_vm_language_server_extension/src/browser.ts`
- Modify: `zr_vm_language_server/wasm/wasm_exports.cpp`
- Create: `zr_vm_language_server_extension/test/serverWorkspaceState.test.js`
- Test: `zr_vm_language_server_extension/test/smoke/browserRunner.js`

- [ ] worker 校验 open/change/close version；增量 change 使用共享的 UTF-16/UTF-8 position contract，失败不覆盖本地 map。
- [ ] initialize 应用 `zrSelectedProjectUri`；实现 `zr/selectedProject`、workspaceFolders、didChangeWatchedFiles/file operations 对应 WASM exports。
- [ ] Web 无法直接读本地磁盘时，client 将 workspace 文件内容/变更以受限协议同步给 worker；明确大小、scheme、root 和缓存限制。
- [ ] workspace diagnostics/symbols/references 使用 workspace index，不能只遍历 open documents。
- [ ] shutdown 后拒绝新请求；exit code/worker close 与 LSP lifecycle 一致；`shutdownRequested` 不再是未使用变量。

## Task 4：按优先级补齐公共 provider

**Files:**
- Modify: `zr_vm_language_server/CMakeLists.txt`
- Modify: `zr_vm_language_server/wasm/wasm_exports.cpp`
- Modify: `zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts`
- Modify: `zr_vm_language_server_extension/src/browser/worker/server-worker.ts`
- Test: `zr_vm_language_server_extension/test/smoke/browserRunner.js`

- [ ] 第一批（核心编辑流）：completion resolve、signature help、semantic tokens range/delta、diagnostic refresh、versioned rename/code actions。
- [ ] 第二批（canonical relation 已完成后）：declaration/typeDefinition/implementation、call hierarchy、type hierarchy。
- [ ] 第三批：linked editing、moniker、inline value、document color、on-type formatting。
- [ ] inlineCompletion/rangesFormatting 作为 3.18 optional，独立 capability gate。
- [ ] 每批新增 C export、bridge method、worker handler、registry entry、native/WASM golden 和 browser smoke；任何一项缺失则不声明。

## Task 5：清理 VS Code mode 与版本体验

**Files:**
- Modify: `zr_vm_language_server_extension/package.json`
- Modify: `zr_vm_language_server_extension/src/extension.ts`
- Modify: `zr_vm_language_server_extension/src/browser.ts`
- Modify: `zr_vm_language_server_extension/test/extensionContributions.test.js`
- Modify: `zr_vm_language_server_extension/test/languageClientLifecycle.test.js`

- [ ] 决定并实现 desktop `mode=web`：若支持，在 desktop extension 启动 worker；若不支持，从 desktop 可选配置与描述中移除该值。不能继续允许选择后只弹“此构建不可用”。
- [ ] `auto` 在 desktop 选择 native，在 VS Code Web 选择 WASM；错误信息包含 runtime、server version 和可操作原因。
- [ ] extension/server/asset version 由同一 release metadata 校验；打包测试拒绝 `0.0.6` extension 携带 `0.0.1` serverInfo。
- [ ] restart race、broken transport、worker startup timeout、graceful shutdown 全部保留现有 lifecycle tests 并增加 Web 对称用例。

## Task 6：建立原生/Web 金样差分测试

**Files:**
- Create: `tests/language_server/fixtures/protocol/*.json`
- Create: `tests/language_server/compare_native_wasm_protocol.js`
- Modify: `tests/CMakeLists.txt`
- Modify: `zr_vm_language_server_extension/scripts/run-web-smoke.js`

- [ ] corpus 覆盖 initialize capability variants、document lifecycle、completion/hover/signature、navigation、rename、diagnostics、semantic tokens、formatting、hierarchy、project/provider reload 与错误。
- [ ] runner 向 native stdio 和 WASM worker 发送同一序列，规范化 server name、timing、resultId 后比较结构。
- [ ] 对 runtime-specific capability 使用 manifest 中明确 waiver；waiver 包含 owner、reason、expiry milestone，不能散落在测试条件里。
- [ ] Web smoke 必须实际加载新构建的 `.wasm/.js/worker.js`，校验 bundle hash，避免测试旧资产。

## Task 7：验收门禁

- [ ] `npm run test:unit`、`npx tsc -p . --noEmit` 通过。
- [ ] `npm run build:wasm` 与 `npm run test:e2e:web` 通过；desktop smoke 通过 native 和（若支持）web mode。
- [ ] public capability corpus native/WASM 0 diff；runtime waiver 只剩经过批准的 filesystem/debug 限制。
- [ ] worker 不再包含 identity resolve handlers、错误到空结果 fallback、独立 diagnostic hash 或手写 capability legend。
- [ ] package/version/asset layout tests 证明发布包与 serverInfo 一致。
