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
