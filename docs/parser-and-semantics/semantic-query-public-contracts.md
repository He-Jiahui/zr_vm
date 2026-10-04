---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_imports.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - tests/parser/test_numeric_branch_assignment_dataflow.c
  - tests/parser/test_numeric_branch_refinement.c
  - tests/parser/test_numeric_loop_assignment_dataflow.c
  - tests/parser/test_numeric_loop_constant_condition_dataflow.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/code-review/coverage/zr_vm_parser_semantic_query_public_header.tsv
tests:
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/debug/test_debug_evaluate_result_transport_cases.h
  - tests/acceptance/comment-review-20261001.md
  - tests/parser/test_numeric_branch_assignment_dataflow.c
  - tests/parser/test_numeric_branch_refinement.c
  - tests/parser/test_numeric_loop_assignment_dataflow.c
  - tests/parser/test_numeric_loop_constant_condition_dataflow.c
doc_type: module-detail
---

# 公开语义查询的调用契约

## 查询在调用链中的职责

这组接口让 compiler、LSP 与 debug 消费已发布的 canonical 类型、引用、调用、可见性、关系、属性和诊断事实。位置查询不会解析源码或补齐分析；调用方先准备同代事实，再选用符合功能需要的查询。导航消费身份与范围，补全消费 visible facts，signature/hover 消费调用契约，项目失效判定消费 public 摘要。查询成功不替代这些消费者自身的字段、精确性和外部身份校验。

`MODULE` 与 NULL scope 都不按节点裁剪。`NODE` 借用非空 AST root，以其 location 和 source 做范围过滤；不同查询检查位置、事实起点或完整范围的方式有差异。它不遍历 AST，也不执行局部语义分析。Scope factory 允许保存 NULL root，各 API 的后续处理不同：CallEdgesAt 显式拒绝，OutgoingCalls/IncomingCalls 对有效 ID 返回 true 空集合。edge 查询对未知 scope kind 也可得到合法空集合，不能把所有接口统一描述为错误 enum 必定 false。

## 快照身份、所有权与输出

TypeId/SymbolId 是当前 context 的身份，Reset 会重新从首值分配。跨代保留数字须配合模块/文档身份与版本，在新 context 中重新核验；相同数字不能证明对象相同。LSP call hierarchy 的 item 保存文档 version 并在后续请求重查 symbol，是此边界的实际消费者。

fact/AST/argumentMappings 视图借用 context 或来源 AST；字符串、provider/module identity、URI 与 `FileRange.source` 也可能借用实际 owner 或 GC root。结构和值数组的字节复制不复制这些指向的资源。消费期间保持同代 context、AST 和实际 owner/root 有效，不跨 Reset/Free/事实改写继续使用旧视图。`TODO:` 沿 compiler/LSP 快照取得、释放与 GC 注册核实每类返回字符串的保活链，不能以 context 存活推断全部 pointee 都已注册 root。

`TODO:` debug 的 CanonicalTypeAt 调用来自每次求值的临时 prepared context；求值器复制数值 TypeId 后释放该 context，协议仍将其与暂停 stateId 一起发送。暂停代际并不自行证明不同求值 context 的 TypeId 可以互相解析。当前传输用例只验证非零数字，仓内 TS/JS 未检索到该字段消费者；需沿未来协议消费核实它仅作单次结果元数据，还是承担跨请求类型身份，不在此宣称已发生错误 lookup。

输出数组首次须 Construct/全零，或按精确元素宽度合法 Init；复用时宽度匹配，allocator 与 context 的 global 兼容。缓冲区由调用方 Free，元素内借用指针不归数组所有。`TypeAt` 的 InferredType 深拷贝缓冲另由调用方释放，typeName 仍借用；输出不得与输入事实重叠或持有未释放旧缓冲。普通值输出通常由入口初始化，`TypeAt` 的早期 false 保持旧值，`CanonicalTypeAt` 的 false 可保留所选 fact 指针；只能按具体 API 契约消费。

## 按功能选择接口

| 接口 | 实际功能与调用限制 |
|---|---|
| ExactnessAllowsProjection | EXACT 投影门槛；不验证快照/ID。CallAt 也用它选优，不因此拒绝所有近似调用。 |
| Scope_Module / Scope_Node | 设置范围过滤器；不获取 context/AST 所有权，NULL 输出 no-op。 |
| TypeAt / CanonicalTypeAt | 前者复制 EXACT 表达式的旧 InferredType；后者优先有效 reference 类型且不要求 isResolved，EXACT expression 作后备。false 不能推断事实指针全空。 |
| FactsAt | 六类独立可选视图，任一非空即 true；不保证同节点、全部 resolved 或 EXACT。 |
| DefinitionOf / DeclarationOf | 单一 READ reaching definition 或声明回退；按 SymbolId 查询最窄 resolved declaration。返回借用 fact。 |
| DefinitionsOf / ReferencesOf | 多可达定义与匹配引用的借用 fact 指针数组；至少一项才 true。References 包含声明或未解析角色，消费者另过滤。 |
| CallAt / FormatCall | CallAt 的 true 可缺 resolved target 或为 APPROXIMATE；FormatCall 要求可投影且 reference/type 一致。false 的格式缓冲可能含截断文本，不能展示。 |
| CallEdgesAt / OutgoingCalls / IncomingCalls | 按 callsite、caller 或 target 查已发布边，合法空集合返回 true；NODE 按 callsite 过滤。值内 source 仍借用。 |
| CallCandidatesAt | EXACT/resolved target 的已登记 overload-set 成员投影；不重新选重载或筛 viable candidates，缺 selected member/任一成员无效拒绝整组，至少一项才 true。 |
| SymbolAt | resolved reference + 有效 SymbolId 的符号值；typeId 可无效、record 缺席时 kind/node 仍零，import URI 冲突拒绝。 |
| ImportOriginAt | NOT_APPLICABLE / INVALID / RESOLVED 三态：按 import literal 来源范围与唯一一致关系解析，不能按 alias 拼写猜目标。 |
| DeclaredSymbols | resolved declaration 与 record 的 ID/type/node 精确配对后投影，按 source 位置排序去重；有效事实下 true 可以空。 |
| ExternalReferences | 仅完整 owner/token/hash/target kind 的 resolved 外部身份；generation 可零，typeId 不参与门槛；没有合格项返回 false。 |
| VisibleSymbols | 只消费 scope/visible facts，处理可用位置、类别开关、static receiver、命名空间遮蔽和 overload-set 保留；至少一项才 true。 |
| RelationsOfSymbol / ImplementationsOf | 任一 endpoint 身份匹配的直接关系，或指向目标的 IMPLEMENTATION/OVERRIDE；不展开传递闭包，至少一项才 true。 |
| BaseTypesOf / DerivedTypesOf | BASE_TYPE 的 source 是 derived、target 是 base；直接边查询无 scope 参数，不展开传递闭包。 |
| MaterializeDiagnostics / Diagnostics | 先发布/解析事实，再替换一个 scope 缓存，再只读借用；Diagnostics 未物化可 true/空，事实变化不会自动更新。 |
| PublicContract | 同 context 的 typeEnvironment 与完整 SCRIPT、空诊断及可读 canonical 类型；schema v1 包括普通顶层函数和显式 public mutable 变量，hash/count 配模块身份比较，不是完整 ABI。 |
| PropertyAt / PropertyBySymbolId | 位置引用身份优先，再 selection/declaration 范围；或任一 property/accessor/value 参数 ID 查统一契约。按值复制不证明 accessor callable 关系全部验证。 |
| FindUnionDeclarationByTypeName | LSP switch 穷尽性从 compiler 当前 script/extern AST 找 union；去首个泛型实参文本可创建 GC 字符串，返回借用 AST。 |

各数组清空时点独立：DefinitionsOf 在 context/facts/scope gate 后清长，ReferencesOf 在有效 context/facts 后、invalid ID 拒绝前清长；DeclaredSymbols/VisibleSymbols 先准备输出再检查事实/位置；ExternalReferences/relations 在 context/facts gate 后才准备。实施元素宽度检查的数组接口在清长前拒绝不匹配宽度；ReferencesOf 不检查复用宽度，调用方必须满足同宽前提。`TODO:` 沿其调用方数组复用与其它 prepare 策略核实此差异的设计意图，当前不宣称合法调用已违反前提。false 不能通用解释为“旧结果已清空”。

## 局部诊断投影与数值断言边界

`semantic_query.c` 物化已发布事实的诊断：UNREACHABLE 发布 WARNING；READ 且有 definite-assignment 状态时，UNINIT 发布 ERROR，MAYBE_INIT 发布 WARNING。这个 read 谓词不要求 `isResolved`。数值诊断只以 `mayOverflow` 为门槛并发布 WARNING，不表示每次求值必然溢出，也不重新运行范围推导。消费者仍须按各查询的快照和缓存契约读取结果。

四个 numeric dataflow/refinement 测试文件的源码断言须分别解释：仅检查 min/max 的用例只证明区间包络；显式检查 `rangeSegmentCount` 与每段端点的用例才证明分段表示。例如 branch assignment 检查 `{2} ∪ {11}`，branch refinement 的一项 false-branch 用例检查 `[1,11] ∪ [21,256]`；loop assignment 的一项 while 用例只检查 `[6,11]` 包络及 `mayOverflow=false`，constant-condition 的 false-for 初始化用例检查退出后没有范围事实。不能把这些例子扩展为所有控制流、诊断分类或 reciprocal dependency 的覆盖。四文件的 assertion-abort 清理仍需失败探针核验；这里只同步可见断言，没有新增测试运行证据。

## 候选类型身份与待核实契约

候选 `callableTypeId` 原样复制 overload member 的 `symbol.typeId`；CallAt 另取当前调用事实的闭合 callableTypeId。普通函数夹具验证 selected member 的声明类型，方法注册却可将 ownerTypeId 写入 symbol，而调用 producer 复用该 member SymbolId。`TODO:` 沿 compiler_type_member_register_symbol、member call producer 与 semantic_calls_append_candidate 补方法候选 fixture，核对未来生产消费者如何解释两种 ID。当前 CallCandidatesAt 仓内只有测试调用，不能据缺少夹具断言生产调用已出错。

`TODO:` MaterializeDiagnostics 忽略 AppendDiagnostic 的 false 后仍标 materialized 并返回 true；false 混合不适用与构建失败，沿 compiler/LSP 生命周期核实部分诊断的预期。物化替换会销毁旧嵌套资源，NODE 缓存按 root 指针身份区分，借用 view 消费期间不得并发改写。

`TODO:` PropertyAt 全零且无 source 的位置可能命中二进制 import 的 unavailableRange，需沿 runtime-bootstrap 的合法消费场景核实输入是否应拒绝。PropertyBySymbolId/publisher 对 accessor callable TypeId 关系的验证边界需结合 LSP CollectSymbols 中间态与后续关系发布固定，当前不宣称复制等于全面验证。

## 已证实的静态分配失败链

`BUG:` 对合法非空输出，多个数组查询的首次 Init 原始分配失败仍保留 isValid/容量并继续 Push，可能在 Debug 断言或写空 head：DefinitionsOf、ReferencesOf、三个 edge 查询、Candidates、Declared/External/Visible 及四个关系查询。Visible 的临时 candidate 数组也走此路径。每 API 台账需给出具体合法 producer/fixture、首次 Init 与命中 Push 的证据；这里不泛化为所有扩容故障已闭合，也未执行 OOM 注入。

`BUG:` 合法非空数组的 EXACT expression 发布成功后，只有 TypeAt 这次 InferredType_Copy 的 elementTypes 首次 Init 失败仍进入 Push。完整链从 ArrayLiteralType_Infer/ExpressionType_Infer，经事实复制发布到 TypeAt；直接 TypeAt 仓内调用只有测试，合法数组 producer 用例不能冒充直接 query caller。

`BUG:` PublicContract 的合法非空 export 临时数组首次 Init 失败后仍 Push。scratch 容量等于 statement 数，每 statement 至多一项，因此本条证据只对应首次 Init，不能扩大为未证明的增长缺陷。

## 审查与验证

本批边界是公开 H 的 31 API、17 类型与 10 独立契约，共 58 单元；实现与调用方阅读支持声明契约，各 C 实现本身仍由对应模块台账覆盖。当前枚举包含 619 个直接调用、61 个声明/定义和 288 个完整 caller 函数；API 文本没有额外函数指针或其它首方语言命中。根代理已接受逐单元判断及完整调用方分片，独立复核修正了 FormatCall 的签名文本优先级与 ReferencesOf 的宽度责任。正式台账逐项登记 35 个 commented、7 个 TODO、15 个 BUG 和 1 个 no-comment；332 个证据锚点另行核验。最终 symbols reviewer 独立完整读了 29 个生产和 24 个关键测试函数，其余测试全文语义依据来自先前完整调用方分片及根代理阅读，不能把文件 SHA 核对描述为独立全文审读。

GCC/Clang 修改前各跑七个既有 query/property 可执行文件，163 用例各 161 PASS/2 FAIL。首次 after 均在 WSL CreateVm 超时，未执行编译或用例；原失败收据保留。根据较新的实际 Linux 验证证据及一次有时限的命令检查，于 21:01Z 启动独立 after-recovery；GCC/Clang 均完成 81 个构建动作，退出0，各163例仍为161 PASS/2 FAIL，逐例目标、名称和状态与before完全相同，两个诊断registry/message-table失败保持原身份。包装器实际退出1，不作为全绿结果。MSVC 原before包装器 smoke 失败/OSError、独立CLI烟测及首次中断after均保留；恢复于15:55:23–15:58:08Z完成配置、CLI构建与direct hello_world smoke，三步退出0（session34056）。限定目标对照、源码token等价与台账门禁均已完成；期间外部会话更新core/SSA源码，未声明整个旧工作区受控、分配失败注入或全仓绿色。完整日志和限制见当日验收记录。
