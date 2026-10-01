#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/semantic.h"

/**
 * @file semantic_query.h
 * @brief 为 compiler、LSP 和 debug 消费已发布语义事实提供共同查询边界。
 * @note AST、fact、字符串及 FileRange.source 均可能是借用指针；调用方须保持同代 context、AST 和实际字符串 owner/root 有效。
 * 原生数组或结构体的值复制不会延长这些对象的生命周期；Reset、事实数组改写/扩容及 Free 后须重新查询。
 * TypeId/SymbolId 只在当前快照内解释，Reset 会重置编号；跨代保存时须另带调用层身份/版本并重新核验，不能只凭相同数字认定同一对象。
 * TODO: 沿 compiler/LSP 的快照获取、释放与 GC root 注册核实返回字符串的实际保活链；context 存活本身不是所有 pointee 的 GC root 证明。
 * TODO: debug 求值释放临时 context 后，协议仍发布其 TypeId；沿 canonicalTypeId 接收方核实单次元数据与跨请求身份边界，暂停 stateId 不能代替 context 归属。
 */
/** @brief 对需要精确表达式语义的投影统一要求 EXACT；不验证快照、范围或 ID。
 * @note CallAt 也用此结果选择同宽候选，并不因此拒绝全部近似调用；消费方仍须检查所需 fact。 */
static inline TZrBool ZrParser_SemanticQuery_ExactnessAllowsProjection(
        EZrSemanticFactExactness exactness) {
    return exactness == ZR_SEMANTIC_FACT_EXACT;
}

/** @brief MODULE 查询整个 context；NODE 仅以 root 的同源 location 范围裁剪，不遍历 AST 或触发局部分析。 */
typedef enum EZrParserSemanticQueryScopeKind {
    ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE = 0,
    ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE
} EZrParserSemanticQueryScopeKind;

/** @brief 可选查询边界；NULL scope 与 MODULE 等效。
 * @note 有效 NODE 范围过滤依赖非空 root；root 借用同代仍存活 AST，其范围须与 position/facts 的可选 source 身份一致。 */
typedef struct SZrParserSemanticQueryScope {
    EZrParserSemanticQueryScopeKind kind;
    const SZrAstNode *root;
} SZrParserSemanticQueryScope;

/** @brief 一处位置可用的六类独立事实视图，供导航、诊断和类型消费者各取所需。
 * @note 每项可为空，各项可能来自不同范围/节点；有任一项不代表其他项齐全、已解析或 EXACT。 */
typedef struct SZrParserSemanticQueryFacts {
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticNumericFact *numeric;
    const SZrSemanticReachabilityFact *reachability;
    const SZrSemanticLogicalFact *logical;
    const SZrSemanticOwnershipFact *ownership;
} SZrParserSemanticQueryFacts;

/** @brief 对一个已物化 scope 的只读诊断视图；items 及其嵌套资源归 context 持有。
 * @note count=0 时 items 为 NULL；重新物化、Reset 或 Free 后旧视图失效，使用前须重新查询。 */
typedef struct SZrParserSemanticQueryDiagnostics {
    const SZrStructuredDiagnostic *items;
    TZrSize count;
} SZrParserSemanticQueryDiagnostics;

/** @brief canonical 类型 ID 与其位置事实视图，供 hover、token 和 debug 各自验证所需契约。
 * @note typeId 只在同代 context 解释；reference/expression 均借用且可为空，false 也可能保留它们，不能据 bool 推断 resolved 或全零。 */
typedef struct SZrParserSemanticTypeQuery {
    TZrTypeId typeId;
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
} SZrParserSemanticTypeQuery;

/** @brief 当前调用的类型、目标与实参映射视图，供 signature/hover 消费而无需按文本重新推导。
 * @note fact、argumentMappings 及 range.source 借用同代 owner；true 不证明 EXACT 或有 resolved target，身份字段须结合 hasResolvedTarget 消费。 */
typedef struct SZrParserSemanticCallQuery {
    TZrTypeId callableTypeId;
    TZrTypeId receiverTypeId;
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
    SZrFileRange callSiteRange;
    SZrFileRange callTargetRange;
    TZrSize argumentCount;
    TZrBool hasNamedArguments;
    TZrBool isMemberCall;
    TZrBool hasResolvedTarget;
    TZrSymbolId targetSymbolId;
    SZrFileRange targetDeclarationRange;
    const SZrArray *argumentMappings;
} SZrParserSemanticCallQuery;

/** @brief 已发布调用图边的值投影，供层级消费者区分 resolved、目标未解析和 caller 不可用。
 * @note 不含 AST 指针；range.source 仍借用，只有 hasTargetDeclarationRange 时消费目标范围，ID 不能跨代单独解释。 */
typedef struct SZrParserSemanticCallEdgeQuery {
    TZrSymbolId callerSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId callableTypeId;
    SZrFileRange callSiteRange;
    SZrFileRange targetDeclarationRange;
    EZrSemanticCallEdgeResolution resolution;
    TZrBool hasTargetDeclarationRange;
} SZrParserSemanticCallEdgeQuery;

/** @brief 已登记 overload 成员的身份和值投影；isSelected 表示当前调用选中者，不是每项的可调用性验证。
 * @note callableTypeId 原样复制 symbol.typeId，declarationRange.source 借用；方法成员可记录 owner TypeId。
 * TODO: 沿 member symbol 注册、调用事实复用与未来候选消费者补方法 overload fixture，固定此字段的类型契约。 */
typedef struct SZrParserSemanticCallCandidateQuery {
    TZrSymbolId symbolId;
    TZrTypeId callableTypeId;
    SZrFileRange declarationRange;
    TZrBool isSelected;
} SZrParserSemanticCallCandidateQuery;

/** @brief 导航/补全使用的符号值；ID、role、范围与当前事实配对，指针字段仍借用来源 owner。
 * @note SymbolAt 成功可有无效 typeId 或缺 kind/node；ownerSymbolId 由可见性投影填充，不能视为每种查询都提供的字段。 */
typedef struct SZrParserSemanticSymbolQuery {
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrSymbolId ownerSymbolId;
    EZrSemanticSymbolKind kind;
    EZrSemanticReferenceKind role;
    SZrFileRange referenceRange;
    SZrFileRange declarationRange;
    SZrFileRange definitionRange;
    /* Borrowed from the semantic snapshot; never retain across generations. */
    const SZrAstNode *declarationNode;
    SZrString *displayName;
    SZrString *signatureDisplay;
    /* Snapshot-borrowed external identity; generation 0 means unavailable. */
    SZrString *externalOwnerIdentity;
    TZrUInt64 externalProviderGeneration;
    TZrUInt32 externalMetadataToken;
    TZrUInt32 externalSignatureToken;
    TZrUInt64 externalSignatureHash;
    EZrSemanticExternalTargetKind externalTargetKind;
    TZrBool hasExternalTarget;
    /* Borrowed import classification from the snapshot's visible-symbol fact. */
    SZrString *externalOriginUri;
    TZrBool isImport;
} SZrParserSemanticSymbolQuery;

/** @brief 供跨文档匹配消费的 opaque 外部身份值；owner/token/hash 的完整性由查询门槛确定。
 * @note owner/range.source 借用；provider generation 可为零、typeId 可无效，值复制不会延长 provider 或字符串寿命。 */
typedef struct SZrParserSemanticExternalReferenceQuery {
    SZrFileRange referenceRange;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    EZrSemanticReferenceKind role;
    /* Snapshot-borrowed owner identity; all token fields are exact. */
    SZrString *externalOwnerIdentity;
    TZrUInt64 externalProviderGeneration;
    TZrUInt32 externalMetadataToken;
    TZrUInt32 externalSignatureToken;
    TZrUInt64 externalSignatureHash;
    EZrSemanticExternalTargetKind externalTargetKind;
} SZrParserSemanticExternalReferenceQuery;

/** @brief 让 completion 显式放宽 receiver、import/alias 和不可访问项的过滤；NULL 等于三项关闭。
 * @note 开关不补分析事实，也不解除 static scope 对实例 receiver 成员的限制。 */
typedef struct SZrParserSemanticVisibleSymbolOptions {
    TZrBool includeReceiverMembers;
    TZrBool includeImports;
    TZrBool includeInaccessible;
} SZrParserSemanticVisibleSymbolOptions;

/** @brief 直接关系边的值投影，供导航和 type hierarchy 消费同代端点身份。
 * @note range 由 hasSourceRange/hasTargetRange 判定可用；module identity、URI 与 range.source 借用，generation 字段原样投影，外部身份须由消费者依 provider 契约核验。 */
typedef struct SZrParserSemanticRelationQuery {
    EZrSemanticRelationKind kind;
    TZrSymbolId sourceSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId sourceTypeId;
    TZrTypeId targetTypeId;
    SZrString *sourceModuleIdentity;
    SZrString *targetModuleIdentity;
    /* Zero means unavailable; nonzero values participate in edge identity. */
    TZrUInt64 sourceProviderGeneration;
    TZrUInt64 targetProviderGeneration;
    SZrFileRange sourceRange;
    SZrFileRange targetRange;
    SZrString *externalOriginUri;
    SZrString *virtualDeclarationUri;
    TZrBool hasSourceRange;
    TZrBool hasTargetRange;
    TZrBool isExternal;
} SZrParserSemanticRelationQuery;

/** @brief 区分没有适用 import、命中但身份不一致、唯一一致来源三种结果；不是 bool 成功位。 */
typedef enum EZrParserSemanticImportOriginResolution {
    ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE = 0,
    ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_RESOLVED,
    ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_INVALID
} EZrParserSemanticImportOriginResolution;

/** @brief import literal 的一致来源视图，供 LSP 关系导航定位 URI 而不按 alias 名称猜目标。
 * @note referenceRange.source 和两个 URI 借用同代事实/关系；virtualDeclarationUri 可为 NULL。 */
typedef struct SZrParserSemanticImportOriginQuery {
    SZrFileRange referenceRange;
    /* Borrowed from one consistent set of snapshot facts and relations. */
    SZrString *externalOriginUri;
    SZrString *virtualDeclarationUri;
} SZrParserSemanticImportOriginQuery;

/** @brief 当前摘要 schema 的 hash/count 对，供项目索引决定导入者是否需要重新分析。
 * @note 不含模块身份，也不覆盖完整 public surface；调用方须与模块键、可用性及版本配对保存。 */
typedef struct SZrParserSemanticPublicContractQuery {
    TZrUInt64 hash;
    TZrSize exportCount;
} SZrParserSemanticPublicContractQuery;

/** @brief 复用 property/accessor/value 参数的统一规范契约，避免消费者重新按成员名推导身份。
 * @note 输出按值复制，范围中的 source 仍借用实际 owner；复制不证明 accessor 关系已全部验证。 */
typedef SZrSemanticPropertyContract SZrParserSemanticPropertyQuery;

/* union 声明查询借用 compiler 的当前 AST；前向声明使本接口无需暴露 compiler 布局。 */
struct SZrCompilerState;
typedef struct SZrCompilerState SZrCompilerState;

/** @brief 初始化不按节点裁剪的查询边界；NULL scope 输出为 no-op，不持有 context 或 AST。 */
ZR_PARSER_API void ZrParser_SemanticQueryScope_Module(SZrParserSemanticQueryScope *scope);
/** @brief 将借用 AST root 保存为位置范围过滤器，用于 debug 等已准备局部快照的查询。
 * @note NULL scope 为 no-op；可保存 NULL root，但它不提供有效 NODE 过滤边界，后续须检查各查询结果；不会自动分析节点。 */
ZR_PARSER_API void ZrParser_SemanticQueryScope_Node(SZrParserSemanticQueryScope *scope,
                                                    const SZrAstNode *root);

/**
 * @brief 把位置上所选 EXACT 表达式的旧推断类型复制给需要 InferredType 模型的消费者。
 * @pre outType 与输入事实不重叠且不持有未释放的旧缓冲；成功后嵌套类型/区间数组由调用方用兼容 state 释放，typeName 仍借用。
 * @return 普通输入、scope 或非 EXACT 拒绝时 false 且 outType 未改写；命中后通过 void Copy 返回 true。
 * BUG: 合法非空数组 EXACT fact 的元素缓冲首次分配失败，Copy 仍 Push 并断言/空写；见 ArrayLiteralType_Infer→事实发布→TypeAt→InferredType_Copy 静态链，未注入 OOM。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_TypeAt(const SZrSemanticContext *context,
                                                    SZrFileRange position,
                                                    const SZrParserSemanticQueryScope *scope,
                                                    SZrInferredType *outType);
/** @brief 为 hover/token/debug 取得位置的 canonical TypeId，优先非无效 reference 类型，再用 EXACT expression。
 * @return 输出入口清零；reference 优先路径不要求 isResolved，false 可留所选 fact 指针，不能代替消费者自身验证。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CanonicalTypeAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticTypeQuery *outQuery);
/** @brief 将已发布调用事实组合为 signature/hover 所需的调用契约，不执行解析或重载选择。
 * @return 有效输出入口清零；成功可能是 APPROXIMATE 或无 resolved target，消费者按用途检查 exactness/hasResolvedTarget。
 * @note query 内的 facts、实参映射与 source 均借用；FormatCall/Candidates 的前提比本查询更严格。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CallAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticCallQuery *outQuery);
/** @brief 为同代且可投影的调用契约生成展示签名，优先引用事实提供的签名文本，避免消费者按文本重建参数。
 * @return 有效缓冲入口先置空；只有 true 时展示。false 的后期 snprintf 路径可留下截断文本，不承诺始终空串。
 * @pre query 的 fact 指针仍有效且 reference/type 一致，expression 为 EXACT；bufferSize 包含终止符空间。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_FormatCall(
        const SZrSemanticContext *context,
        const SZrParserSemanticCallQuery *query,
        TZrChar *buffer,
        TZrSize bufferSize);
/** @brief 按 callsite 位置读取已发布调用边，供消费者区分 resolution 而不从同名符号猜目标。
 * @pre outEdges 为 Construct/全零或同宽 SZrParserSemanticCallEdgeQuery 数组，缓冲由调用方用兼容 context global 的 state 释放。
 * @return 正确准备后清长；合法空集合为 true，NODE root 为空显式 false；早期输入/宽度拒绝不保证清旧结果，值内 source 仍借用。
 * BUG: 合法非空边查询的首次 Init 分配失败仍进入 Push，可能断言/空写；见 semantic_calls.c 的 prepare/append 链，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CallEdgesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges);
/** @brief 按同代 caller ID 提供已发布出边，LSP call hierarchy 再核验目标身份和可导航范围。
 * @pre outEdges 为 Construct/全零或同宽 SZrParserSemanticCallEdgeQuery 数组，allocator 与 context global 兼容。
 * @return 准备后清长；valid caller 的合法空集合及无 root NODE 为 true/空，invalid ID 为 false；更早拒绝不保证清旧输出。
 * BUG: 合法非空出边查询的首次 Init 分配失败仍进入 Push，可能断言/空写；见 semantic_calls.c 的 prepare/append 链，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_OutgoingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId callerSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges);
/** @brief 按同代 target ID 提供已发布入边，保留 caller unavailable 等 resolution 给层级消费者判断。
 * @pre outEdges 为 Construct/全零或同宽 SZrParserSemanticCallEdgeQuery 数组，缓冲由调用方用兼容 state 释放。
 * @return 准备后清长；valid target 的合法空集合及无 root NODE 为 true/空，invalid ID 为 false；source 指针仍借用快照。
 * BUG: 合法非空入边查询的首次 Init 分配失败仍进入 Push，可能断言/空写；见 semantic_calls.c 的 prepare/append 链，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_IncomingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId targetSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges);
/** @brief 投影当前 selected target 的已登记 overload-set 成员，不重新选择重载或筛选 viable candidates。
 * @pre outCandidates 为 Construct/全零或同宽 SZrParserSemanticCallCandidateQuery 数组，allocator 与 context global 兼容。
 * @note candidate.callableTypeId 原样复制 symbol.typeId；CallAt 另保存本次闭合调用类型，不能普遍称候选为声明签名。
 * @return EXACT/resolved 调用且 selected/member 身份全有效才返回非空 true；缺成员或部分无效清空并 false。
 * TODO: 方法注册可将 owner TypeId 写入 symbol.typeId；沿 member producer、候选 fixture 与未来生产消费者固定方法字段契约。
 * BUG: 合法非空候选的首次 Init 分配失败仍继续 append/Push，可能断言/空写；见 semantic_calls.c，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CallCandidatesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outCandidates);
/** @brief 为导航取单一 READ reaching definition，缺少唯一结果时回退到声明。
 * @note 不展开 definitionRanges；结果为当前事实数组的借用指针，引用和定义均受 scope 限制。 */
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DefinitionOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope);
/** @brief 给声明导航与身份去重取同代 SymbolId 的最窄 resolved declaration，不按名称猜测。
 * @return 无匹配或无效输入为 NULL；结果借用当前 fact 数组，scope 按源码范围过滤且不触发分析。 */
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DeclarationOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope);
/** @brief 为多位置导航投影已解析的 reaching definitions；无可用写入时回退声明，按同源位置排序并去重。
 * @pre 输出为 Construct/全零数组，或同元素宽度 const SZrSemanticReferenceFact* 的可复用数组，allocator 与 context global 兼容。
 * @note backing 归调用方，fact 指针仍借用；通过输入/facts/scope gate 后清空长度，早期 false 不保证清旧结果。
 * @return true 表示至少一个定义；false 包括普通无命中，消费者不得读取先前结果来替代。
 * BUG: 合法非空定义查询首次 Init 失败仍进入 append_definition_fact 的 Push，断言/空写而非 false；见 semantic_query.c:235/259，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DefinitionsOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outDefinitions);
/** @brief 按当前快照 SymbolId 收集范围内引用事实；code lens/导航另筛选声明、已解析状态和重复项。
 * @pre 输出为 Construct/全零或同元素宽度 const SZrSemanticReferenceFact* 的可复用数组；本接口不校验复用宽度，释放须使用兼容 context global 的 state。
 * @note 有效 context/referenceFacts 后清长度，即使随后拒绝 invalid ID；更早拒绝不保证清空，元素指针仍借用。
 * @return 至少一个匹配 fact 才为 true；false 也可表示合法空结果，本接口不自行排除 DECLARATION 或未解析 role。
 * TODO: 沿调用方数组复用与其它查询的 prepare helper 核实此宽度校验差异的设计意图。
 * BUG: 合法匹配 fact 下首次 Init 失败仍继续 Push，断言/空写而非 false；见 semantic_query.c:834/852，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_ReferencesOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences);
/** @brief 汇集位置上的独立语义视图，让调用方检查需要的事实及其精确性，避免从文本重建语义。
 * @return 有效输出先清零；至少一项非空为 true，缺少任一其他事实不构成失败。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_FactsAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryFacts *outFacts);
/** @brief 把位置上的 resolved reference 投影为导航/补全符号值，并拒绝冲突 import URI。
 * @return 输出入口清零；成功只要求有效 SymbolId，不保证 typeId 可用或 symbol record 存在，kind/node 可仍为零。
 * @note name/signature/identity/URI、AST 和所有 range.source 浅借用实际 owner/root，不因值复制而独立存活。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_SymbolAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticSymbolQuery *outSymbol);
/** @brief 按 import literal 来源范围连接 visible symbol 与 canonical origin relation，用于关系导航。
 * @return 输出入口清零；未适用为 NOT_APPLICABLE，命中但缺项/冲突为 INVALID，唯一一致来源为 RESOLVED。
 * @note 不按 alias 拼写回退；范围 source/URI 借用同代事实，virtualDeclarationUri 可为 NULL。 */
ZR_PARSER_API EZrParserSemanticImportOriginResolution
ZrParser_SemanticQuery_ImportOriginAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticImportOriginQuery *outImport);
/** @brief 为 inlay/token/code lens 投影 resolved declaration 与精确匹配 record，按源码位置排序去重。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticSymbolQuery 数组，缓冲归调用方，allocator 与 context global 兼容。
 * @return 准备后清长，再检查 referenceFacts；有效事实下 true 可以空，typeId 可以无效；早期宽度拒绝不清长。
 * @note AST/name/signature/URI 与 range.source 仍借用实际 owner/root，source-order 不代表跨代身份稳定。
 * BUG: 合法非空声明的首次输出 Init 分配失败仍进入 Push，可能断言/空写；见 semantic_query_symbols.c，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DeclaredSymbols(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outSymbols);
/** @brief 为跨文档匹配投影 owner/token/hash 身份完整的 resolved 外部引用，缺项时不按名称补 metadata。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticExternalReferenceQuery 数组，调用方用兼容 allocator 释放。
 * @return context/referenceFacts 与宽度 gate 后清长，至少一项才 true；typeId 与 provider generation 非零不参与完整性门槛。
 * @note owner identity 和 range.source 浅借用；generation=0 的外部身份须由消费者按 provider 契约判断。
 * BUG: 合法非空完整外部引用的首次 Init 分配失败仍进入 Push，可能断言/空写；见 semantic_query_symbols.c，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_ExternalReferences(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences);
/** @brief 为 completion 从已发布 scope/visible facts 投影可见符号，处理可用性、遮蔽与 overload 成员保留。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticSymbolQuery 数组，缓冲归调用方，allocator 与 context global 兼容。
 * @return 先准备/清长再查位置与 active scope；至少一项才 true，不从全局 symbol table 猜缺失可见性。
 * @note 默认过滤 receiver/import/不可访问项，static scope 仍排除实例成员；元素指针仍借用 owner/root。
 * BUG: 合法非空可见候选的临时或输出数组首次 Init 分配失败仍继续 Push，可能断言/空写；见 semantic_query_symbols.c，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_VisibleSymbols(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        const SZrParserSemanticVisibleSymbolOptions *options,
        SZrArray *outSymbols);
/** @brief 提供同代 SymbolId 位于任一端点的直接关系，供 import/导航消费者检查唯一性和身份。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticRelationQuery 数组，使用兼容 context global 的 allocator。
 * @return context/facts 与宽度 gate 后清长，随后 invalid ID 或无匹配为 false；至少一项才 true。
 * @note 不展开传递闭包；复制的 range.source、URI 与 module identity 仍借用同代来源。
 * BUG: 合法非空匹配的首次 Init 分配失败仍 append/Push，可能断言/空写；见 semantic_relations.c，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_RelationsOfSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations);
/** @brief 提供指向目标符号的直接 IMPLEMENTATION/OVERRIDE 边，供实现导航；不展开传递闭包。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticRelationQuery 数组，由调用方用兼容 allocator 释放。
 * @return context/facts 与宽度 gate 后清长，随后 invalid ID 或无匹配为 false；至少一项才 true。
 * BUG: 合法非空匹配的首次 Init 分配失败仍 append/Push，可能断言/空写；见 semantic_relations.c 的静态链，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_ImplementationsOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations);
/** @brief 提供指定 derived TypeId 的直接 BASE_TYPE 边，供 type hierarchy 向上导航。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticRelationQuery 数组，缓冲归调用方且使用兼容 allocator。
 * @return context/facts gate 后准备/清长，invalid ID 或无匹配为 false；无 scope 参数，不展开传递闭包。
 * BUG: 合法非空匹配的首次 Init 分配失败仍 append/Push，可能断言/空写；见 semantic_relations.c 的静态链，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_BaseTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations);
/** @brief 提供指定 base TypeId 的直接派生边，供 type hierarchy 向下导航。
 * @pre 输出为 Construct/全零或同宽 SZrParserSemanticRelationQuery 数组；元素 URI/module identity/source 仍借用。
 * @return context/facts gate 后准备/清长，invalid ID 或无匹配为 false；无 scope 参数，不展开传递闭包。
 * BUG: 合法非空匹配的首次 Init 分配失败仍 append/Push，可能断言/空写；见 semantic_relations.c 的静态链，未注入 OOM。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DerivedTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations);
/**
 * @brief 在语义分析生命周期中替换一个 scope 的诊断缓存，随后同 scope 的 Diagnostics 只读消费。
 * @pre 先完成本次所需事实解析；调用期间不保留旧诊断视图，也不并发改写 context。
 * @note 这会销毁旧诊断的嵌套资源；NODE 缓存按 root 指针区分，其他 scope 不会触发自动重析。
 * TODO: AppendDiagnostic 的 false 混有不适用和构建失败，当前物化忽略它仍返回 true；沿 compiler/LSP 发布链核实部分诊断契约。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_MaterializeDiagnostics(
        SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope);
/** @brief 只读返回已物化的诊断缓存，不触发解析、分析或物化。
 * @return 输出先为 NULL/0；未物化时 true 空视图，已物化且 scope root 与缓存不同则 false。
 * @note true/空结果不证明事实已分析或没有错误；事实变化后调用方须先按所需 scope 重新物化。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_Diagnostics(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryDiagnostics *outDiagnostics);
/**
 * @brief 为项目依赖刷新计算 schema v1 的函数和显式 public mutable 变量摘要。
 * @pre typeEnvironment 绑定同一 context，moduleRoot 为同代完整 script，published/query 诊断为空且 canonical 类型可读。
 * @note schema 包括所有普通顶层函数，未编码模块身份或完整语言 public ABI；hash/count 与模块键配对比较。
 * @return outQuery 入口清零，普通无效或不支持输入为 false；只有 true 时记录新摘要。
 * BUG: 合法非空 export 的临时数组首次 Init 分配失败，后续 Push 会断言/空写而非正常 false；见 semantic_query_public_contract.c 的静态链，未注入 OOM。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_PublicContract(
        const SZrSemanticContext *context,
        const SZrTypeEnvironment *typeEnvironment,
        const SZrAstNode *moduleRoot,
        SZrParserSemanticPublicContractQuery *outQuery);
/** @brief 优先按位置引用身份取 property 契约，再按 selection/declaration 范围支持声明导航与 code action。
 * @pre position、scope 与 context 同代；可选 source 按两侧一致性检查，结果中的范围 source 仍借用。
 * @return 有效输出入口清零；命中复制契约，无匹配为 false。
 * TODO: 全零无 source 位置可能命中 binary import 的 unavailableRange；沿 runtime-bootstrap 用例核实无定位输入应否可查询。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_PropertyAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticPropertyQuery *outQuery);
/** @brief 由 property、accessor 或 setter/init value 参数的同代 SymbolId 回到统一属性契约。
 * @return 有效输出入口清零；不依赖源范围，未命中/invalid ID 为 false。
 * TODO: 基础 publisher 未验证全部 accessor callable TypeId 关系；沿 LSP CollectSymbols 中间态及后续关系发布固定可查询边界。 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_PropertyBySymbolId(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrParserSemanticPropertyQuery *outQuery);
/** @brief 供 LSP union-switch 穷尽性分析从 compiler 当前脚本/extern AST 查找 union 声明。
 * @note 名称先去除首个泛型实参文本，可能创建 GC 字符串；返回借用 AST，缺少声明或非 union 为 NULL。 */
ZR_PARSER_API SZrAstNode *ZrParser_SemanticQuery_FindUnionDeclarationByTypeName(
        SZrCompilerState *compilerState,
        SZrString *typeName);

#endif // ZR_VM_PARSER_SEMANTIC_QUERY_H
