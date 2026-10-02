---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_property_contract.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
tests:
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_relations.c
doc_type: module-detail
---

# Semantic Context 与 Facts 的调用契约

本模块保存同一次语义分析的身份、事实和原生查询视图，使 compiler、LSP、debug 和 REPL 能消费分析器已经发布的证据。类型和符号注册表、canonical 类型图、词法可见性、表达式及数据流事实属于同一快照；它们各自保留不同含义，不能仅凭相同整数或相同名字互相替代。

## 创建、复用与释放

`CompilerState_Init` 为 type environment 建立共享 context（`zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c:29`）。`SZrHirModule` 只关联 AST root 与 context，拥有自己的原生包装，不接管两者。LSP 的分析状态也借用 compiler state 中的 context。

调用方的生命周期顺序是：保持 VM state/global 和 AST/source 有效，完成分析及事实发布，在同一快照内查询，结束借用，再 Reset 或 Free。`CompilerState_Free` 负责最终释放 context（`zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c:734`）。`ZrParser_SemanticContext_Reset` 复用顶层原生缓冲，结束旧记录、旧 ID 和嵌套载荷的有效期；缓冲地址未改变也不能保留旧记录身份（`zr_vm_parser/src/zr_vm_parser/semantic.c:190`）。

Reset 会清除 provider generation 与 URI resolver 配置。需要延续宿主配置的 `compile_script` 显式保存并恢复这些配置，再重建 HIR 包装（`zr_vm_parser/src/zr_vm_parser/compiler.c:713`）。调用方不能把这种恢复当成 Reset 自身的保证。

查询诊断缓存还拥有每条 structured diagnostic 的 related-information 和 fixes 数组。Context Reset 清除缓存身份；诊断重新物化则先释放旧嵌套载荷、清空旧列表，在物化结束后更新 root/epoch 身份。两条路径由不同入口管理，不能假设每次重新物化都先把缓存身份清零（`zr_vm_parser/src/zr_vm_parser/semantic.c:106`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c:1007`）。

## 身份、注册与发布

| 入口或身份域 | 调用目的与限制 |
| --- | --- |
| TypeId / SymbolId / OverloadSetId / ScopeId | 对应当前 context 中各自的注册表；同为 UInt32 不提供静态防混用，0 是这些域的无效值。 |
| LifetimeRegionId | 关联静态所有权和清理计划；不是运行时槽、资源句柄或 CFG block ID。 |
| Reserve 系列 | 预留下一次发布的身份，不建立记录、验证关联或回滚计数；Reset 后序号重新使用。 |
| RegisterInferredType | 按 canonical 身份保留结构类型投影，清除不应进入类型身份的值级范围、布尔和长度事实；重复身份不更新旧元数据。 |
| RegisterNamedType | 通过 FromName 获得 canonical 身份，再登记 object 投影；FromName 可能解析已有身份，不能承诺每个名字都新建 nominal 类型。 |
| RegisterSymbolWithId | 使用调用方提供的身份发布符号，不推进 nextSymbolId，也不完整校验 typeId / overloadSetId；调用方须避免编号冲突。 |
| PublishScopeFact | 保留生产者的 parentScopeId，并验证非零父作用域存在；仅本条 id 被重新分配，ownerSymbolId 与 kind 没有完整验证。 |
| PublishVisibleSymbolFact | 使用已存在的 scopeId / symbolId 与非零 declarationOrder；签名和 source 借用，仅 externalOriginUri 经字符串工厂重建。 |
| PublishCanonicalTypeSymbol | 关联类型投影与 TYPE 声明符号；两次追加不构成事务，失败不能推断所有前缀均已撤回。 |
| GetOrCreateOverloadSet / AddOverloadMember | 名称选择重载集合，成员使用另一域的 SymbolId；成员追加不自动证明符号及 callable 关联完整。 |

注册与发布的基本边界见 `zr_vm_parser/src/zr_vm_parser/semantic.c:349`、`zr_vm_parser/src/zr_vm_parser/semantic.c:489`、`zr_vm_parser/src/zr_vm_parser/semantic.c:564`、`zr_vm_parser/src/zr_vm_parser/semantic.c:584`。各个 Find 返回当前数组内的借用地址；扩容、Reset、Free 或对应载荷替换会结束借用。

`SZrSemanticPropertyContract` 连接属性、accessor、value parameter、接收者效果和访问约束。基础发布要求属性符号存在、分类及类型匹配，至少一个 accessor ID 非零，并核对 setter/initializer 的 value parameter 存在性、PARAMETER 分类和类型。此阶段不证明 accessor 符号存在。后续 relations 发布对 accessor 要求 FUNCTION 分类、非零 callable ID 和符号 typeId 相等，也不能扩大为完整 canonical 节点和所有关联验证（`zr_vm_parser/src/zr_vm_parser/semantic/semantic_property_contract.c:33`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c:271`）。

**TODO: ID 耗尽策略。** 五个 context 计数器没有 UInt32 耗尽检查；合法使用次数的上限和公开 API 的拒绝策略仍需明确。核查入口为 Reserve 系列及宿主长生命周期快照的使用方式（`zr_vm_parser/src/zr_vm_parser/semantic.c:240`）。

## 事实载荷的所有权

Facts 初始化 expression、reference、numeric、reachability、logical、ownership、ownership intrinsic、receiver guard 与 diagnostic 九类数组，并委托 relations 子系统管理关系事实（`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:465`）。

| 载荷 | 原生所有权或借用关系 |
| --- | --- |
| Expression.inferredType | context 拷贝并清理其原生嵌套载荷；同一非空 node 的重新发布替换旧载荷。typeName 和 constant string 等 VM 指针仍按各自借用契约处理。 |
| Reference.definitionRanges / argument mappings | 拷贝容器，由 context Reset/Free 清理；记录与 source/AST 的身份仍属于原快照。 |
| Numeric range segments | 复制指定范围容器；查询只选择一条已有事实，不建立新的联合范围。 |
| Expression.callTargetName / memberName / diagnosticMessage / diagnosticCode | 在 context.state 中重建 VM 字符串；原生 context 所有权不能证明这些副本已成为 GC 根。 |
| Reference.signatureDisplay / externalOwnerIdentity | 在 context.state 中重建 VM 字符串；reference.name 保持借用。 |
| Ownership.diagnosticMessage | 非空输入被重建，显式克隆失败会拒绝发布；副本仍受相同 GC 根限制。 |
| 其它 AST、name、range.source 指针 | 按相应结构契约借用，调用方维持其原对象和源码的有效期。 |

具体复制边界见 `zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:602`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:638`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:799`。不能把“拷贝结构”解释为接管所有指针，或把保持原输入有根解释为保持独立 VM 副本有根。

## 发布与查询的证据含义

生产者决定事实分类、已知值、resolved 状态和各类 presence 位。Find 入口通常仅定位已经发布的记录；返回非空不额外认证类型身份、可达状态、所有权安全或 lowering 能力。没有命中时，上层按各自查询契约处理缺失证据。

- 位置查询只匹配 position.start，采用同源闭区间。事实 range 与 query 各自至少一端有非零 offset 时用 offset；否则用行列。候选宽度始终用 offset 差，行列回退不会自动获得可靠的跨度顺序（`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:51`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:106`）。
- 普通 ReferenceAtPosition 先偏好起点命中，再比较跨度和 kind。更宽的起点命中可以胜过更窄的内部命中；不能把所有 reference finder 概括为全局选择最窄范围。按 kind 查询的入口只在所需 kind 内选择（`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:894`）。
- Expression、Logical 的位置查询在同宽时采用后项；Ownership 的 violation 仅在同宽时优先；Reachability 先按原因评分。这些偏好服务不同 consumer，不能互换（`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:869`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:1070`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:1126`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:1170`）。
- NumericByNode 返回一条已有事实，按 exactness、分段信息、signed 范围及宽度、unsigned 范围存在性选取；这些信息相同时才偏好 mayOverflow=true，完全平局保留前项。它不合并所有候选的范围或风险，返回 mayOverflow=false 不能证明其它候选也安全（`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:1007`）。
- 调用参数映射的 conversion 分类不独立证明兼容性检查发生。共享实参映射发布者由 validateCompatibility 控制该检查，普通调用事实生产者传入 false；consumer 不能把 EXACT / IMPLICIT 标签当作普遍验证证书（`zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c:163`、`zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c:373`）。
- Reference 的 resolved 声明/写入可为自身建立 definition seed；后续投影使用已发布顺序中的既有定义，并非 CFG 求解。只有已有 known/ID/分类等字段提供的证据才能使用，不能从记录存在性推断完整数据流证明。

## 静态可达的问题

本批保留三个问题族；以下均为完整可达路径的静态审查结论，尚无分配失败注入或动态 GC 复现。

**BUG: 顶层 types / scopeFacts 初始分配失败未传播。** 有效 VM state 下，宿主分配器仅使对应初始请求失败，后续请求成功，Array_Init 仍留下正容量和有效标记；New/Reset 返回可用外观的 context。合法类型登记或模块 scope 发布随后写空 head。这不依赖伪造 context（`zr_vm_parser/src/zr_vm_parser/semantic.c:43`）。

**BUG: 重载 members 初始分配失败仍发布集合。** 有效名称和 context 下，单次 members 初始请求失败后，集合仍取得非零 ID；合法符号发布链随后 AddOverloadMember 写入空 head。集合创建成功的返回值不能作为 members 缓冲可用的证明（`zr_vm_parser/src/zr_vm_parser/semantic.c:720`）。

**BUG: NATIVE_CONTEXT_INTERNAL_VM_CLONE_UNROOTED。** 保持 VM/context、借用 AST/source 和原始输入有根，不能保护只存入原生 context 的独立长串副本。默认增量 GC 模式下，128 字节及以上的 URI 或诊断文本可以独立重建；成功完成同线程 FullGC、没有补充宿主 trace 的合法路径中，context 不在已核查根集合里，副本可被回收。URI 路径需两个同 SymbolId 的 import 候选，随后身份比较在内部读取 URI；Ownership 路径通过 FindOwnership 和实际 REPL 输出读取独立 message，首次失效读取位于 repl_string_text。两条消费路径共用根缺失问题族（`zr_vm_parser/src/zr_vm_parser/semantic.c:23`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c:438`、`zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c:441`、`zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.c:94`）。

其它 Expression / Reference 文本共享没有自动 context GC 根的 helper 边界，但不据此宣称其每条 consumer 路径均已证明同一故障。短串、其它 GC 模式、额外宿主 trace、停止或未完成的 FullGC 也不属于上述已经闭合的触发条件。

## 失败语义

各 bool 入口只承诺实现明确检查的失败。Array_Init / Copy / Push 并非统一报告分配失败，重复发布与前缀状态也不普遍回滚。调用方不得把一次 true 或非零 ID 扩大为完整 OOM 安全、跨数组事务或所有关联有效的证明。原有共享数组问题仍按其所属模块的完整证据处理，本批注释不改变运行行为。

## 相关模块

- [Semantic Fact Layer](semantic-fact-layer.md)：各 producer、consumer 与现有验证案例。
- [Semantic Query Public Contracts](semantic-query-public-contracts.md)：上层查询入口的结果归属、选择策略及输出限制。
- [Semantic Scope Fact Ownership](semantic-scope-fact-ownership.md)：词法作用域和静态所有权载荷的生产者协议。

## 审查与验收状态

本模块的144条宏观注释及310个审查单元经过调用方阅读和独立契约复核。250项字段契约、证据锚点、可达问题证明与字节一致性分别记录；函数数量和哈希一致不能替代语义审查。47份既有台账仅迁移359个指向同一语句的引用，不重新认证其它审查结论。实际构建与既有失败的逐例比较见[当日验收记录](../../tests/acceptance/comment-review-20261001.md)。
