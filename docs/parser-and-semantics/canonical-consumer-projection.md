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
- root layout 与 callable contract 都存在且精确匹配。

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

函数调用 AST range 覆盖 `(` 到 `)`，expression call fact 再合并 primary 起点与最后参数，
因此参数内部、参数间空白和空参数括号内都能命中同一 call query。LSP signature help 先消费
该 query；hover 只通过 canonical TypeId formatter 显示类型。恢复中的不完整源文件若没有
exact fact，可以使用既有 last-good/editor recovery path，但 recovery 不会写回或冒充
canonical fact。

## Compatibility boundary

M5 不把 legacy `SZrIo` 二进制解释成 `ZRAF`，也不允许 canonical API 回退到 legacy
name-based binding。当前 legacy artifact 执行路径仍属于正式语法切换前的兼容层；plan 06
执行迁移和切换时必须移除该兼容入口，而不是在本 projection 内长期维护双格式语义。

新增 consumer 时应复用 core projection 或 parser semantic query，并为成功、未知 identity、
hash mismatch 和 recovery 边界提供测试。禁止新增 `strcmp(typeName, ...)` 或具体 built-in
显示名分派。
