# LSP 快照、工作区与诊断一致性实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `test-driven-development`, `support-first-regression-testing`, `zr-language-feature-design`, `evidence-driven-wsl-validation`, and `verification-before-completion` while executing this plan.

**Goal:** 统一 URI、文档快照、项目/provider 代际和诊断缓存，使请求只在其真实依赖变化时失效，并让 workspace 功能覆盖真实工作区而非“当前打开文档集合”。

**Architecture:** 建立不可变 `SZrLspSemanticSnapshot`，其中包含规范化 URI、document version/content hash、project generation、provider generation、compiler semantic generation 与 dependency document generations。所有诊断、workspace edit、semantic tokens 和跨文件查询都从 snapshot 读取，并用同一个 fingerprint 生成 resultId/fence。

**Tech Stack:** C11、parser semantic context、project manifest API、xxHash、LSP diagnostic pull model、Node.js protocol tests。

---

## Task 1：统一 file URI 与 native path

**Files:**
- Create: `zr_vm_language_server/include/zr_vm_language_server/lsp_uri.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_uri.c`
- Create: `tests/language_server/test_lsp_uri.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_position.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c`
- Modify: `zr_vm_language_server/stdio/stdio_document_file.c`

- [ ] RED 矩阵覆盖 Windows drive、UNC、POSIX、空格、`#`、`%`、非 ASCII、已编码 URI、大小写与分隔符、`vscode-test-web:`、`zr-decompiled:` 和非法转义。
- [ ] 提供唯一双向 API：

```c
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspUri_FileToNativePath(
        SZrString *uri, TZrChar *buffer, TZrSize bufferSize);
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_LspUri_FromNativePath(
        SZrState *state, const TZrChar *path);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspUri_Equivalent(
        SZrString *left, SZrString *right);
```

- [ ] FromNativePath 按 URI 规则百分号编码 UTF-8 bytes；FileToNativePath 只接受 `file:` scheme，不把 web/virtual URI 传给 fopen。
- [ ] 删除项目、导航和 stdio 的本地转换副本；ModuleIdentity 只保存 canonical URI/native path，不混入原始字符串比较。
- [x] 2026-09-06 Sub01：补齐 percent-decoded ASCII control-byte 边界（`%00`、`%01`、`%7F`），失败时清空 native path；GCC 17/17、Clang ASan+UBSan 17/17、MSVC 19/19。详见 [Sub01 record](2026-09-06-plan02-task01-sub01-decoded-control-bytes.md)。

## Task 2：建立工作区 folder 与项目集合

**Files:**
- Create: `zr_vm_language_server/src/zr_vm_language_server/project/lsp_workspace.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/project/lsp_workspace.c`
- Modify: `zr_vm_language_server/stdio/stdio_initialize.c`
- Modify: `zr_vm_language_server/stdio/stdio_requests.c`
- Modify: `zr_vm_language_server/stdio/stdio_workspace_files.c`
- Create: `tests/language_server/stdio_workspace_folders_smoke.js`

- [ ] initialize 按优先级读取 `workspaceFolders` → `rootUri` → `rootPath`，规范化并建立 workspace set。
- [ ] 实现 add/remove workspace folder；移除时释放只属于该 root 的 project/file index，但保留仍被打开文档或其他 root 引用的 overlay。
- [ ] `changeNotifications` 只有 handler、索引更新和测试全部存在时才重新声明。
- [ ] watched files/file operations 只处理已注册 root 或显式打开 overlay；拒绝通过非 file URI 触发本地读盘。
- [ ] 为 multi-root 中同名 `.zrp`、嵌套 root、选中项目被移除、project file rename 添加测试。

## Task 3：定义语义快照与依赖 fence

**Files:**
- Create: `zr_vm_language_server/include/zr_vm_language_server/lsp_semantic_snapshot.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c`
- Create: `tests/language_server/test_lsp_semantic_snapshot.c`
- Modify: `zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c`

- [ ] snapshot 至少包含：

```c
typedef struct SZrLspSemanticSnapshotIdentity {
    TZrUInt64 documentGeneration;
    TZrUInt64 projectGeneration;
    TZrUInt64 providerGeneration;
    TZrUInt64 semanticGeneration;
    TZrUInt64 dependencyFingerprint;
} SZrLspSemanticSnapshotIdentity;
```

- [ ] `Acquire` 固定 URI、内容、AST/last-good 状态、semantic context、project/provider view 和依赖集合；`Release` 释放引用，不暴露可变 `SZrFileVersion *`。
- [ ] dependency set 记录实际读取的模块/metadata；未读取文档的变化不得使请求 ContentModified，读取过的任一依赖变化必须使 fence 失败。
- [ ] request handler 在发送结果前验证 snapshot identity；失败返回 `-32801`，取消优先于 ContentModified。
- [ ] semantic token cache、workspace edit snapshot、diagnostic resultId 复用同一 identity/fingerprint，不再各自造 hash。

## Task 4：修复文档同步验证

**Files:**
- Modify: `tests/language_server/stdio_document_sync_conformance.js`
- Modify: `zr_vm_language_server/stdio/stdio_documents.c`
- Modify: `zr_vm_language_server/stdio/stdio_document_content.c`
- Modify: `zr_vm_language_server/stdio/stdio_lsp_parse.c`
- Modify: `zr_vm_language_server/stdio/stdio_position_encoding.c`

- [ ] didOpen 必须包含 integer version 与 string text；重复 open 采用明确策略（拒绝并记录，或用完整文本重置），不能静默覆盖不一致状态。
- [ ] didChange 对未打开文档、空 changes、stale/same version、越界行列、逆序 range、落在 UTF-16 surrogate 中间的 character 标记该文档为 desynchronized；notification 不发送 JSON-RPC response，旧 snapshot 不变，后续语义请求 fail closed，直到收到无 range 的 full-content change 或 close/open 重建 overlay。
- [ ] 若提供 `rangeLength`，按协商的 position encoding 计算并严格比较；不匹配时不应用部分 change。
- [ ] 多个 contentChanges 必须按前一个 change 的结果顺序应用；任一失败则整个 notification 原子回滚。
- [ ] didClose 将 file URI 的 open overlay 切换回已索引磁盘/project snapshot；只有 untitled/virtual、已从工作区删除或明确未索引的文档才移除 parser/project 状态。Web bridge 使用同一规则。
- [ ] didSave 在 `includeText=false` 时只确认/刷新磁盘代际；若客户端仍发送 text，不能用同一 version 走普通 didChange 路径，也不能在 update 失败后发布伪装成新代际的 diagnostics。
- [ ] 合法的越界 position 请求可按具体 LSP method 返回 InvalidParams/null，但不得把非法 didChange 钳制到行尾。
- [ ] 使用 utf8proc 验证 code point，补齐 CR、LF、CRLF、astral plane、combining sequence 与无效 UTF-8 矩阵。

## Task 5：实现真实增量解析和失效

**Files:**
- Modify: `zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_declaration_index.c`
- Modify: `tests/language_server/test_incremental_parser.c`
- Create: `tests/language_server/test_lsp_incremental_equivalence.c`

- [ ] 先把当前行为命名为 full reparse；在真实 subtree/declaration reparse 完成前，telemetry/capability 不得声称内部已增量。
- [ ] change range 映射到最小可重解析 declaration/block；不可安全恢复时显式 fallback full parse。
- [ ] 复用节点必须同步 range/source/parent identity；token-equivalent fast path 继续要求长度、token value 与坐标完全相同。
- [ ] public contract hash 不变时只重算当前文件/声明；变化时沿 import graph 失效下游 semantic snapshot。
- [ ] differential test 对同一编辑序列比较 incremental 与 clean full parse 的 AST、diagnostics、symbols、TypeId/SymbolId relations 和 LSP JSON。

## Task 6：修复 pull/push diagnostics

**Files:**
- Create: `zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c`
- Modify: `zr_vm_language_server/stdio/stdio_diagnostics.c`
- Modify: `zr_vm_language_server_extension/src/browser/worker/server-worker.ts`
- Create: `tests/language_server/stdio_diagnostics_generation_smoke.js`
- Create: `zr_vm_language_server_extension/test/serverDiagnostics.test.js`

- [ ] resultId 由 snapshot identity + sorted structured diagnostic payload hash 生成；依赖模块变化而本文件文本不变时 resultId 必须变化。
- [ ] document diagnostic 的 invalid params 返回错误，不得伪造 full empty report。
- [ ] workspace diagnostics 遍历 workspace/project index，支持 `previousResultIds`、`workDoneToken`、`partialResultToken` 与 cancellation；不能只遍历打开文档。
- [ ] `interFileDependencies=true` 时，在依赖诊断变化后通过 `workspace/diagnostic/refresh` 或下一次 pull 返回新 resultId；明确 push 与 pull 的共存策略，避免同一代际重复发布。
- [ ] Web 与 native 使用同一 C diagnostic store/resultId，删除 TypeScript 自有 hash 算法。

## Task 7：验收门禁

- [ ] URI round-trip/negative matrix 通过 GCC、Clang、MSVC。
- [ ] 10000 个随机 UTF-8/UTF-16 edit 序列 differential test 无 snapshot 漂移。
- [ ] 两个无关 URI 的编辑不会使活动请求返回 ContentModified；直接/传递依赖编辑必定返回 ContentModified。
- [ ] multi-root workspace diagnostics 包含未打开源文件，并在 dependency/provider reload 后改变 resultId。
- [ ] 增量与 full parse 结果同构；记录 p50/p95/p99 和 fallback ratio，不以墙钟单值验收。
