---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/include/zr_vm_language_server/symbol_table.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_semantic_snapshot.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_uri.h
  - zr_vm_language_server/stdio/stdio_server.h
  - zr_vm_language_server/wasm/wasm_exports.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/lsp/index.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_stable_slot_contract_cases.h
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
  - tests/cmake/zr_vm_lsp_wasm_response_tests.cmake
doc_type: module-detail
---

# Language Server

## 传输和生命周期

stdio 服务器使用 LSP JSON-RPC：读取 `Content-Length` frame，解析 request/notification，
按 `initialize` -> `initialized` -> workspace/document 操作 -> `shutdown` -> `exit` 管理状态。
`$/cancelRequest` 只取消当前请求，不释放共享 workspace snapshot。WASM 入口采用显式 request
buffer/response JSON，不假定 POSIX stdin/stdout。

URI 先通过 `ZrLanguageServer_LspUri_*` 规范化为平台路径；UTF-8/UTF-16 position codec 在
`initialize` 能力协商中固定。diagnostic 的 source 是 `zr`，每条包含 severity、range、
code、cause、suggestion、relatedInformation 和可选 safe fix。

## 语义管线

文档打开或变更时，incremental parser 复用 token/declaration index，只重解析受影响 scope；
`SZrSemanticAnalyzer` 再把 AST 投影到 parser 的 canonical SymbolId/TypeId/PlaceId 和
provider generation。缓存按 document/project/provider/semantic generation 失效，不能使用
旧 AST 的裸指针。跨文件引用使用 semantic snapshot provenance，避免同名 workspace 类型
冒充官方 provider。

## 能力

当前能力包括 diagnostics（push/pull）、completion、hover、signature help、definition、
references、document/workspace symbols、highlight、inlay hints、semantic tokens（full/delta/
range）、prepare/rename、code actions、document/workspace edits、call/type hierarchy、
CodeLens（测试/运行）和调试入口。completion/hover/signature 都从 canonical query facts
生成；未知类型不伪造 `Declared at`，但可给出原因和建议。

## C API 示例

```c
SZrSemanticAnalyzer *a = ZrLanguageServer_SemanticAnalyzer_New(state);
ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(a, ZR_TRUE);
ZrLanguageServer_SemanticAnalyzer_Analyze(state, a, ast);
ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(state, a, &diagnostics);
ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, a, position, &hover);
ZrLanguageServer_SemanticAnalyzer_Free(state, a);
```

`SZrSymbol` 同时保存 source range、references、access modifier、semantic ids 和 property
contract；符号表提供按名称、位置和 semantic id 查找。调用方必须按 state allocator 释放
diagnostic/hover 数组，不能把 analyzer 内部缓存对象带出 revision。
