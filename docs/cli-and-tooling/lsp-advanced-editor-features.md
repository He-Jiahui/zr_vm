---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/CMakeLists.txt
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
plan_sources:
  - user: 2026-04-24 Zr LSP 现代能力对齐计划
tests:
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/stdio_smoke.js
  - tests/CMakeLists.txt
doc_type: module-detail
---

# LSP Advanced Editor Features

## 范围

这份文档记录 Zr language server 第一阶段现代 LSP 能力对齐：补齐编辑器高频交互能力，同时保持语义事实仍来自现有 `lsp_semantic_query`、project index、metadata provider 和 incremental parser。

本阶段实现的协议能力包括：

- `textDocument/codeAction`
- `textDocument/formatting`
- `textDocument/rangeFormatting`
- `textDocument/onTypeFormatting`
- `textDocument/foldingRange`
- `textDocument/selectionRange`
- `textDocument/documentLink`
- `textDocument/declaration`
- `textDocument/typeDefinition`
- `textDocument/implementation`
- `textDocument/codeLens`
- `textDocument/prepareCallHierarchy`
- `callHierarchy/incomingCalls`
- `callHierarchy/outgoingCalls`
- `textDocument/prepareTypeHierarchy`
- `typeHierarchy/supertypes`
- `typeHierarchy/subtypes`
- `textDocument/diagnostic`
- `workspace/diagnostic`
- `completionItem/resolve`

## 设计边界

`lsp_editor_features.c` / `lsp_code_actions.c` 是新的公共接口实现文件，避免继续扩大 `lsp_interface.c`。它们只负责编辑器形态的结果建模：

- formatting 生成 `SZrLspTextEdit`，当前使用基于 brace/block 的保守缩进。
- code action 使用 `SZrLspCodeAction`，第一阶段提供 `source.organizeImports` 和缺失分号 `quickfix`，返回最小 `TextEdit`。
- folding / selection range 从当前文档文本结构生成轻量结构范围，包含 block、连续 import/comment region，以及 word -> line -> block selection chain。
- document link 扫描 `%import("...")` 字面量、`.zrp` 的 `source` / `binary` / `entry` 路径，以及 native virtual declaration 里的 module link；import 优先复用 definition 查询结果作为跳转目标。
- declaration / typeDefinition / implementation 当前复用 definition 查询，保证和已有导航结果一致。
- code lens 当前为函数/类型提供引用计数，并为 `%test(...)` 提供可运行入口。
- call hierarchy 当前实现 prepare 和同文件直接 incoming/outgoing calls；type hierarchy 当前返回同文件直接继承/派生关系。
- pull diagnostics 复用现有 diagnostics 生成逻辑，不替代 `publishDiagnostics` 推送模型。
- completion resolve 通过 completion item `data` 复用原始 URI/position，按 label 回填 detail/documentation。

`stdio_editor_features.c` 是协议层 glue：

- 序列化 `TextEdit`、`WorkspaceEdit`、`CodeAction`、`FoldingRange`、`SelectionRange`、`DocumentLink`、`CodeLens` 和 pull diagnostic report。
- 处理新增 request，并在失败或空结果时返回符合 LSP 习惯的空数组/空 report。
- 保持 stdio request 分发文件只做 capability 广告和 method routing。

## Capability 广告

`initialize` 现在会额外声明：

- `completionProvider.resolveProvider = true`
- `codeActionProvider.codeActionKinds = ["quickfix", "source.organizeImports"]`
- `documentFormattingProvider = true`
- `documentRangeFormattingProvider = true`
- `documentOnTypeFormattingProvider.firstTriggerCharacter = "}"`
- `foldingRangeProvider = true`
- `selectionRangeProvider = true`
- `documentLinkProvider.resolveProvider = false`
- `declarationProvider = true`
- `typeDefinitionProvider = true`
- `implementationProvider = true`
- `codeLensProvider.resolveProvider = false`
- `callHierarchyProvider = true`
- `typeHierarchyProvider = true`
- `diagnosticProvider.interFileDependencies = true`
- `diagnosticProvider.workspaceDiagnostics = true`

VS Code desktop/native stdio 模式会自动消费这些 standard providers；extension 侧不需要为 formatting、links、CodeLens 或 organize imports 增加专有命令。

## 当前限制

- formatting 仍是保守文本缩进器，不是 AST pretty-printer；它不会重排表达式、参数列表或注释。
- code action 第一阶段稳定输出 organize imports 和缺失分号 quickfix；后续可按 diagnostics code/data 增加缺失 import、未解析成员等修复。
- declaration / typeDefinition / implementation 目前复用 definition 查询；后续如果 class/interface/extern 语义拆分，需要在统一语义查询层细化，不要在 stdio 层分叉。
- workspace diagnostic 会为当前增量解析器已知文档返回 full report；document pull diagnostics 复用现有单文档诊断。

## 验证证据

新增覆盖：

- `tests/language_server/test_lsp_advanced_editor_features.c`
  - formatting full-document edit
  - folding range 和 selection range
  - `%import(...)` document link
  - organize imports code action
  - reference count 和 `%test(...)` CodeLens command
  - call hierarchy / type hierarchy prepare item
- `tests/language_server/stdio_smoke.js`
  - initialize capability wire shape
  - formatting / folding / selection / documentLink / codeAction / CodeLens / hierarchy request arrays
  - declaration provider
  - pull diagnostics full report

2026-04-24 验证命令：

```sh
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTS=ON
cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_advanced_editor_features_test zr_vm_language_server_stdio -j 8
ctest --test-dir build/codex-wsl-gcc-debug -R 'language_server|language_server_stdio_smoke' --output-on-failure
cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=ON
cmake --build build/codex-wsl-clang-debug -j 8
ctest --test-dir build/codex-wsl-clang-debug -R 'language_server|language_server_stdio_smoke' --output-on-failure
```
