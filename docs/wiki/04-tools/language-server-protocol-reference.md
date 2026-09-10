---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_diagnostic_store.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_uri.h
  - zr_vm_language_server/stdio/stdio_server.h
  - zr_vm_language_server/wasm/wasm_exports.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/plans/lsp/index.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_stable_slot_contract_cases.h
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
  - tests/cmake/zr_vm_lsp_wasm_response_tests.cmake
doc_type: tool-reference
---

# Language Server 协议与嵌入式接口参考

ZrVm Language Server 有两个 transport adapter：native stdio JSON-RPC 和 WASM request/response
导出。二者应投影同一份 canonical parser/semantic facts，而不是把编辑器功能实现成两套
名称查找器。能力清单集中在 `SZrLspCapabilityDescriptor`，每项明确 client capability path、
core entry point、native adapter、WASM export、最低 LSP 版本和测试 id。

本页描述当前实现的协议边界、生命周期、位置编码、能力差异和 C/WASM 内存约束。语言语义
本身见 [ZR 语言手册](../02-language/index.md)，诊断 payload 的长期约定见
[诊断生命周期参考](../06-reference/diagnostic-lifecycle-reference.md)。

## 1. 分层与职责

```text
LSP client / VS Code extension
  -> stdio JSON-RPC framing OR WASM C export
  -> transport adapter (URI, position codec, JSON)
  -> SZrLspContext
  -> incremental parser + semantic analyzer + workspace/project index
  -> canonical TypeId / SymbolId / Place / provider generation facts
  -> LSP payload (diagnostic, completion, hover, edit, hierarchy ...)
```

`SZrLspContext` 统一拥有 state、incremental parser、semantic analyzer map、semantic snapshot
cache/LRU、workspace/project index、已选 `.zrp`、取消检查回调以及 provider generation。客户
端不应保存其中任何内部对象指针。每次文档更新、项目切换或 native provider generation
变化都可能使旧 semantic snapshot 不再可用于新请求。

## 2. stdio 生命周期与 JSON-RPC framing

native adapter 读取标准 LSP `Content-Length` frame，并按 JSON-RPC request/notification 处理。
基本生命周期为：

```text
process start
  -> initialize request
  -> initialize result / capability negotiation
  -> initialized notification
  -> workspace folders and textDocument open/change/close
  -> query requests and diagnostic notifications/pull reports
  -> shutdown request
  -> exit notification / process end
```

`ZrLanguageServer_StdioServer_New` 接收 `SZrStdioServerOptions`，可指定 input stream 和用于
故障注入测试的 `EZrStdioServerFaultPoint`。创建成功后调用 `Start`；`Shutdown` 结束请求处理；
最后调用 `Free`。不要从 IDE 侧向标准输出写日志或任意文本，它会破坏 `Content-Length` frame；
诊断日志应使用约定的 stderr/trace 通道。

`$/cancelRequest` 只表示当前请求应尽快通过 `FZrLspRequestCancellationCheck` 停止；它不是
释放 shared workspace 或清空 semantic cache 的命令。一个正确的取消实现应丢弃该请求的
临时结果，同时保持已成功发布的 document revision 和 pull-diagnostic identity 一致。

## 3. Initialize、能力与 runtime 差异

capability registry 把实现分为 core 和 native-adapter 两层：

| 类别 | runtime | 例子 | 说明 |
| --- | --- | --- | --- |
| core, native + WASM | 两端均可投影 | document sync、completion、hover、definition、references、rename | 共用 `ZrLanguageServer_Lsp_*` 语义入口 |
| core, native + WASM | 两端均可投影 | symbols、inlay hint、semantic tokens、formatting、folding、selection range、document link、code lens、diagnostic | JSON/WASM 输出由适配层编码 |
| core, native only | stdio 侧 | signature help、implementation、call hierarchy、type hierarchy | registry 不向 WASM 假称已实现 |
| native adapter | stdio 侧 | position encoding、on-type formatting、linked editing、moniker、inline value、workspace folders | 由 JSON-RPC adapter 直接实现 |
| experimental native adapter | stdio 侧 | inline completion | 仅在协商能力允许时发布 |

registry 对每一项还记录最低 LSP major/minor。普通 capability 当前从 LSP 3.17 起提供；
inline completion 的 minimum minor 是 18 且标为 experimental。客户端只能依据 initialize
response 中实际发布的字段发请求，不能因为另一个 runtime 有同名 C 函数就认为该 transport
已支持它。

completion 与 code action 在 native runtime 具备 resolve materialization；WASM 的 base
provider 仍可使用，但没有被宣称拥有相同的 resolve 过程。这个差异应当反映在网页和
editor client capability 检查中。

## 4. Document、URI 与位置

### 4.1 文档更新

内部 C API 的更新入口为：

```c
TZrBool ZrLanguageServer_Lsp_UpdateDocument(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize version);
```

`uri` 必须先经过 LSP URI 规范化；adapter 将 file URI 转换为当前平台路径和 canonical
workspace identity。`version` 代表客户端 document revision，不是 provider generation、
artifact schema 或 source hash。收到 change 后，incremental parser 只重建受影响的 token/
declaration/scope 区域，然后 semantic analyzer 重新投影相关 facts。

LSP position 的 line/character 均从零开始：

```c
typedef struct SZrLspPosition {
    TZrInt32 line;
    TZrInt32 character;
} SZrLspPosition;
```

native stdio server 默认 UTF-16 position encoding，并在 `initialize` 中从 client 的
`general.positionEncodings` 协商 UTF-8。一个请求从 position 到 source byte offset 的转换
必须使用这次协商固定的 codec；不能把 UTF-16 character 直接当成 UTF-8 byte offset，
否则含非 ASCII 文本的 hover、rename、fix range 会错位。

### 4.2 workspace 与 `.zrp` 选择

多项目目录中，client 可调用 `ZrLanguageServer_LspContext_SetClientSelectedZrpUri` 选择一个
`.zrp` URI，用于消除 project discover 歧义；传入 `ZR_NULL` 清除选择。这个选择只影响
workspace resolver 的 target，不改变 source document 的 module declaration。项目摘要可
由 `ZrLanguageServer_Lsp_GetProjectModules` 获取，并在完成后通过
`ZrLanguageServer_Lsp_FreeProjectModules` 释放。

## 5. 查询 API 与所有权

`lsp_interface.h` 提供核心 payload API。常用族如下：

| 需求 | C API | 返回对象 | 对应释放 API |
| --- | --- | --- | --- |
| diagnostics | `Lsp_GetDiagnostics` | `SZrArray` of `SZrLspDiagnostic` | `Lsp_FreeDiagnostics` |
| completion | `Lsp_GetCompletion` | completion item array | 由结果数组 owner 释放 |
| hover / rich hover | `Lsp_GetHover` / `Lsp_GetRichHover` | 指针 payload | `Lsp_FreeRichHover`；signature 有专用 free |
| definition / declaration / references | `Lsp_GetDefinition`、`GetDeclaration`、`FindReferences` | location array | 结果数组 owner 释放 |
| rename / formatting / code actions | `Lsp_Rename`、`GetFormatting`、`GetCodeActions` | text-edit/action array | `Lsp_FreeTextEdits` / `Lsp_FreeCodeActions` |
| tokens / inlay / highlights | `GetSemanticTokens`、`GetInlayHints`、`GetDocumentHighlights` | arrays | feature-specific free |
| hierarchy | call/type prepare and incoming/outgoing/super/sub queries | hierarchy item/call arrays | `Lsp_FreeHierarchyItems` / `Lsp_FreeHierarchyCalls` |

返回 `TZrBool` 的 `false` 不应被 client 翻译为“没有结果”。它可能表示请求参数、URI、
revision、解析、allocation 或 cancellation 问题；adapter 应生成明确 JSON-RPC/LSP error 或
空而有效的 feature result，取决于请求类型和已有 diagnostic。空 array 才表示一个成功但
没有匹配项的查询。

所有 result array、`SZrString *`、hover section、fix text、location URI 都由 LSP/state
allocator 管理。不要使用 libc `free()` 释放，也不要在下一次 `UpdateDocument` 后继续保留
其中的 semantic identity。若需要历史 revision，使用
`ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot`，并把它视为只读、带版本的快照。

## 6. Diagnostic payload 和 pull identity

`SZrLspDiagnostic` 不是只有 message：

| 字段 | 含义 |
| --- | --- |
| `range` | 已按协商 position encoding 转换的编辑范围 |
| `severity` | LSP 1 Error、2 Warning、3 Information、4 Hint |
| `code` / `descriptorId` | 稳定 diagnostic 身份，供 suppress、文档和测试使用 |
| `message` / `codeDescriptionHref` | 可呈现的文本和可选说明链接 |
| `relatedInformation` | 跨位置原因链 |
| `fixes` | title、edit range/text、applicability |
| `noFixReason` | 没有安全 fix 的明确原因 |

push 和 pull diagnostic 使用同一份结构化 payload。pull mode 的 result id 由
`ZrLanguageServer_LspDiagnosticStore_BuildResultId` 从 immutable semantic snapshot 与完整
diagnostic payload 构造，缓冲上限为 `ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX`。客户端应把 result
id 当作缓存验证器：只有同一 URI、同一文档 revision、同一 semantic/provider generation
和同一诊断内容才可复用；不能跨 provider reload 或 `.zrp` 切换复用旧 id。

## 7. 语义 freshness 与 native declaration

LSP 的 completion/hover/signature/definition 不从 raw token 文本猜类型，而是读取 canonical
query facts。每个跨文件结果必须保留 module/provider provenance；同名 workspace declaration
不能覆盖官方 provider 的 type/function identity。native descriptor 的虚拟声明文档可通过
`ZrLanguageServer_Lsp_GetNativeDeclarationDocument` 取得，它是 descriptor projection，
不是磁盘上必然存在的 `.zr` 文件。

provider generation、semantic snapshot version、document content generation 是不同轴：

```text
document edit          -> content generation changes
provider plugin reload -> provider generation changes
semantic rebuild       -> snapshot/version changes
```

任一轴不匹配都要求重新查询或使缓存失效。尤其不要把 C `SZrSymbol *` 或 AST 内部指针存到
另一个 JSON-RPC 请求后再访问。

## 8. WASM 接口

WASM adapter 通过显式 buffer/JSON 返回值运行，不读取 stdin/stdout。典型生命周期：

```c
void *context = wasm_ZrLspContextNew();
const char *json = wasm_ZrLspUpdateDocument(context, uri, uriLen, text, textLen, version);
/* Copy/consume json, then call wasm_free((void *)json). */
wasm_ZrLspContextFree(context);
```

`wasm_malloc` / `wasm_free` 是跨 WASM 边界的分配入口。`wasm_ZrLspUpdateDocument`、
`wasm_ZrLspCloseDocument`、`wasm_ZrLspGetDiagnostics`、`wasm_ZrLspGetDiagnosticReport`、
completion、hover、definition、references、rename、symbols、inlay hints、semantic tokens、
formatting、code action、folding、selection range、document link、code lens 等返回的 JSON
字符串都需要调用 `wasm_free`。输入 URI/content/new name 都是 UTF-8 指针加长度，不要求
NUL 终止；宿主仍须保证调用期间地址有效。

WASM 没有把 stdio-only signature help、implementation/call/type hierarchy 或 native-only
resolve 假装成支持。如果网页或 extension 要展示支持矩阵，应从 capability registry 而不是
仅从 `wasm_exports.h` 中“有一个函数”推导功能状态。

## 9. C 嵌入式最小示例

```c
SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
SZrArray diagnostics = {0};

if (context != ZR_NULL &&
    ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, source, sourceLength, 1U) &&
    ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
    /* Serialize/copy diagnostics before releasing their result array. */
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
}

ZrLanguageServer_LspContext_Free(state, context);
```

实际 adapter 还必须处理 URI creation、source encoding、cancellation、result array 初始化和
JSON conversion。不要将这段代码当作独立 language-server 主函数；stdio framing、global/state
生命周期和 registry/bootstrap 仍需由项目的 stdio server 或受支持 embedding path 管理。
