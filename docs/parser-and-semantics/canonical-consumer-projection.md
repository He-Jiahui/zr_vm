---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-29-plan03-task07-canonical-visible-symbol-completion.md
  - docs/plans/lsp/optimize/2026-08-30-plan03-task07-external-member-reference-identity.md
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_external_member_reference_identity_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/acceptance/2026-08-29-plan03-task07-canonical-visible-symbol-completion.md
  - tests/acceptance/2026-08-30-plan03-task07-external-member-reference-identity.md
doc_type: module-detail
---

# Canonical consumer projection

本文说明 syntax plan 01 M5 建立的规范消费边界。它以 M4 的 `ZRAF` version 1
artifact 和 parser `SemanticContext` 为输入，使 VM module、AOT、LSP、reflection、
debug 和 layout query 使用同一份结构身份，不从类型显示名恢复语义。

## Core artifact view

`ZrCore_CanonicalConsumer_Open()` 只接受 `ZR_ARTIFACT_KIND_ZRO`。它先调用统一
artifact reader，再取得 TypeDef、TypeRef、TypeSpec、Signature、Contract 和 Layout
section view，并建立 `SZrCanonicalConsumerProjection`。

打开成功表示以下条件同时成立：

- public TypeRef、TypeSpec、Signature、Layout 和 Contract identity 已通过 M4 reader 验证；
- root TypeRef 与 TypeSpec 指向逐字节相同的 structural signature；
- signature heap 的实际 hash 与 public signature hash 相同；
- root TypeDef 的 structural signature hash 与 public identity 相同；
- root layout 与 callable contract 都存在且精确匹配；
- function signature 的 receiver/ref-export/effect/parameter/scoped summary 与 ContractRow
  逐字段一致。

projection 和其中的 signature slice 都引用调用方提供的 artifact buffer。调用方必须让
buffer 的生命周期覆盖 projection 的全部查询，不能保存 AST pointer、runtime pointer 或
局部 flow fact 到该 view。

## Consumer APIs

| Consumer | Entry | Identity key |
|---|---|---|
| VM module | `ZrCore_Module_OpenCanonicalArtifact` | public identity + token/TypeId |
| AOT backend | `backend_aot_open_canonical_artifact` | 与 VM 相同的 core projection |
| Reflection | `ZrCore_Reflection_ResolveArtifactType` | TypeDef/TypeRef/TypeSpec token |
| Debug | `ZrCore_Debug_ResolveArtifactType` | canonical TypeId |
| Layout | `ZrCore_CanonicalConsumer_ResolveLayout` | exact type token |

VM 与 AOT adapter 都直接委托给 core open，不各自复制 hash、section 或错误处理逻辑。
reflection、debug 和 layout query 只遍历稳定 row，未知 token/TypeId 返回结构化错误，不接收
type name 参数，也没有名称猜测路径。

## Parser and LSP query

`SZrSemanticExpressionFact` 保存 canonical `typeId`；append 时若 producer 尚未填入，
semantic context 会从完整 inferred type 驻留 TypeId。已解析调用会从 overload resolution
产生的参数类型、passing mode 和返回类型重新驻留 closed function TypeId，而不是复用 open
generic declaration TypeId。

`ZrParser_SemanticQuery_CanonicalTypeAt()` 返回位置对应的 reference/expression TypeId。
`ZrParser_SemanticQuery_CallAt()` 只接受 canonical function node；同范围同时存在 LSP
symbol fact 与 compiler fact 时，优先选择带 compiler signature display 的 function fact，
不会把函数返回类型误当 callable type。

`SZrParserSemanticCallQuery` 直接投影 resolved target identity：

- `hasResolvedTarget` 是唯一有效性判据；
- 为真时，`targetSymbolId` 和 `targetDeclarationRange` 来自同一个 resolved call fact；
- 为假时，query 在入口清零这些字段，消费者不得按 member name 搜索或猜测声明。

source free call 与 receiver member call 都发布该身份。member SymbolId 按精确 declaration
AST node 注册/复用，declaration range 覆盖完整方法声明；imported/native member 没有 source
declaration 时仍可提供 callable TypeId，但不会伪造 resolved source target。

`ZrParser_SemanticQuery_VisibleSymbols()` 是 lexical completion 的唯一 source-symbol
入口。query 结果借用当前 semantic snapshot 的 SymbolId、TypeId、declaration AST、range、
display name 与 signature display；调用方只拥有输出 array storage，不能跨 snapshot 保存这些
pointer。`SZrParserSemanticSymbolQuery.kind` 直接投影 symbol record kind，LSP 不再通过名称、
声明文本或 symbol-table node 猜 completion kind。

scope-fact producer 处理 lexical shadowing、generic parameters、imports/aliases、receiver/static
边界以及 extern block 中的 function/delegate/struct/class/interface/enum declaration。结果按 scope
distance、declaration order、SymbolId 稳定排序；同名内层声明遮蔽外层声明。source function 的
`signatureDisplay` 由 `ZrParser_SemanticDisplay_CreateCallableSignature()` 从同一 SymbolId、closed
function TypeId 和精确 declaration AST identity 生成，参数名来自声明，passing/type/return 来自
canonical contract。缺少 SymbolId、declaration role/node 或 display name 的候选不会进入 LSP。

`ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols()` 仅将上述 borrowed query view
复制为 LSP-owned completion items。signatureDisplay 可用时直接作为 detail；否则只格式化 exact
TypeId；TypeId unavailable 时显示明确的 `cannot infer exact type`，不得回退到 LSP symbol table、
request-time inference 或 AST/name reconstruction。receiver/import completion 仍走各自 structured
query，并保持原 fail-closed 边界。

source-local hover 同样只消费 `ZrParser_SemanticQuery_SymbolAt()`。query 直接复制稳定 SymbolId、
TypeId、signature display、declaration AST identity 与 reference/declaration range；独立
`ZrLanguageServer_LspCanonicalHover_BuildSymbol()` projector 只格式化该 copied view。callable 优先
显示 canonical signature，其他符号只格式化 exact TypeId，TypeId unavailable 时明确显示
`cannot infer exact type`。hover range 始终使用 resolved reference fact range，不能退回请求点、
symbol-table lookup range 或 callee/name 扫描。

LSP AST 只保留非语义补充层：leading comment、结构化 extern-block source identity 与 symbol 上已
缓存的 FFI decorator metadata。extern type 的显示类别来自 canonical declaration AST（delegate、
struct、enum、interface），不是把所有 type 伪装为 class；这些补充不会创建或覆盖 SymbolId、
TypeId、signature 或 range。脱离 analyzer symbol table、reference tracker 与 AST 后，canonical
source hover 仍可用；缺少 exact SymbolAt fact 时直接 unavailable。

外部 metadata type-member reference/highlight 的 declaration identity 同样是权威匹配键。query
或候选任一侧声明 `hasDeclaration` 后，双方必须同时提供 declaration URI，并且 URI 与 range
逐字段相等；identity 不一致时直接 fail closed，不能再退回 module name、owner type name 或
member name。只有双方都明确没有 writable declaration identity 的 native/binary metadata
边界，才保留结构化 module/type/member contract 匹配。`includeDeclaration` 只控制是否把 query
自身声明投影到结果，不改变 usage candidate 的 identity 判定。

source class `new Type(...)` 与 struct `init Type(...)` 同样发布 CALL expression/reference facts。
producer 先消费已解析 prototype constructor；LSP bootstrap 尚未把 constructor member 填入
prototype 时，parser 仍通过 source TypeDef resolver 与 prototype `declarationNode` 取得精确
class/struct meta-function AST，并构造只在本次推断内存活的 member contract。该临时 contract
保留参数名、passing mode、默认参数、声明 range 与稳定 SymbolId，最后驻留 receiver effect 为
`NONE` 的 closed callable TypeId。`FormatCall` 因而输出 `@constructor(name: T): null`，而
`DeclarationOf(targetSymbolId)` 返回同一 constructor declaration；LSP 不按类型名或 AST member
配对补建该身份。

class meta-function 的 `super(...)` 也发布相同 CALL/REFERENCE_CALL 合同。AST 保存精确
`super(...)` range，producer 从 resolved base constructor contract 驻留 closed callable
TypeId，并发布 stable SymbolId 与 whole declaration range。LSP lightweight prototype 缺
constructor member 时，analyzer 只复用 parser source-constructor materializer 建立
snapshot-local structured member；consumer 仍仅调用 `CallAt/FormatCall`，不在 request-time
按 base/member name、argument count 或 AST 文本重建 constructor。payload 或 resolved identity
缺失时 signature help unavailable。

call signature display 也从同一 closed callable TypeId 生成。参数名称来自声明 AST，passing、
`scoped`、ref access 与类型来自 canonical parameter contract；readonly receiver 显示
`const fn read(): int`，mutable receiver 显示 `fn write(...): R`，free/static callable 保持
现有无 receiver 前缀格式。LSP hover/signature consumer 只能显示该 query/fact，不再拼接
receiver 或按旧 passing mode 重建字符串。

generic declaration clause 来自 resolved function/member record 的结构化 generic parameter
数组，closed 参数与返回类型仍来自调用点 canonical TypeId。因此 source generic receiver
稳定显示 `fn shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>`；consumer 不读取
member name 或 AST 文本补写 `<...>`。

函数调用 AST range 覆盖 `(` 到 `)`，expression call fact 再合并 primary 起点与最后参数，
因此参数内部、参数间空白和空参数括号内都能命中同一 call query。LSP signature help 消费
该 query；hover 通过 canonical TypeId formatter 或同一 signature display 显示类型。恢复中的不完整源文件若没有
exact fact，可以使用既有 last-good/editor recovery path，但 recovery 不会写回或冒充
canonical fact。

compiler compatibility 失败时，调用方在清除 current error 前调用
`ZrParser_Compiler_PublishCurrentDiagnostic()`。publisher 把当前 structured diagnostic 的
severity、range、code、message、cause、suggestion、descriptor、related information 与 fixes
深拷贝成 persistent semantic diagnostic fact；普通 compiler error 使用原始 message 和
`compiler_error` code。`ZrParser_SemanticQuery_Diagnostics()` 每次从 persistent fact 重建输出，
所以重复查询不会丢失诊断，也不要求 LSP 按 message、signature 或 member name 重建事实。

`ZrParser_SemanticQuery_PublicContract()` 同时把 persistent diagnostic facts 和现有 query
diagnostics 视为 poisoned module。这样 compiler error 即使尚未经过 diagnostics query，也不会
产生看似可用的 schema-v1 public-contract hash。

LSP call hover 的文本和范围也来自同一 resolved call query。receiver fast path 与 free-call
signature fallback 都把 `query.reference->range` 转为 UTF-16 range；通用 local-symbol hover
只有在查询点确实落入 callee reference range 时才替换范围，避免把普通实参 hover 错误投影
为 callee。

## Compatibility boundary

M5 不把 legacy `SZrIo` 二进制解释成 `ZRAF`，也不允许 canonical API 回退到 legacy
name-based binding。当前 legacy artifact 执行路径仍属于正式语法切换前的兼容层；plan 06
执行迁移和切换时必须移除该兼容入口，而不是在本 projection 内长期维护双格式语义。

新增 consumer 时应复用 core projection 或 parser semantic query，并为成功、未知 identity、
hash mismatch 和 recovery 边界提供测试。禁止新增 `strcmp(typeName, ...)` 或具体 built-in
显示名分派。

## Verification

Syntax 02 M6 的 source/binary-import/VM/AOT/LSP 晋级证据记录在
`tests/acceptance/2026-07-20-syntax-02-m6-artifact-lsp-consumers.md`。三工具链都运行同一
canonical consumer 与 LSP 矩阵；custom LSP runner 同时检查 process exit 与 `Fail -`，不能
把 exit 0 单独解释为 GREEN。
