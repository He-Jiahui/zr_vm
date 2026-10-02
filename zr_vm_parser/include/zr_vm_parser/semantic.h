/**
 * @file
 * @brief 声明语义快照、canonical 与事实身份及 AST/HIR 关联接口。
 * @note compiler 与 LSP 在同一快照内发布和消费；Reset 结束旧身份并清宿主配置。原生存储所有权、外部借用生命周期和 VM GC rooting 分别由对应契约约束。
 */

#ifndef ZR_VM_PARSER_SEMANTIC_H
#define ZR_VM_PARSER_SEMANTIC_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"

#ifndef ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
#define ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
/**
 * @brief 标识当前语义快照中的canonical 类型。
 * @note 底层为 UInt32，0 表示无效；Reset 会重用序号，不能跨快照或跨身份域传递。C typedef 不提供静态域检查。
 */
typedef TZrUInt32 TZrTypeId;
/**
 * @brief 标识当前语义快照中的声明符号。
 * @note 底层为 UInt32，0 表示无效；Reset 会重用序号，不能跨快照或跨身份域传递。C typedef 不提供静态域检查。
 */
typedef TZrUInt32 TZrSymbolId;
/**
 * @brief 标识当前语义快照中的重载集合。
 * @note 底层为 UInt32，0 表示无效；Reset 会重用序号，不能跨快照或跨身份域传递。C typedef 不提供静态域检查。
 */
typedef TZrUInt32 TZrOverloadSetId;
/**
 * @brief 标识当前语义快照中的静态清理区域。
 * @note 底层为 UInt32，0 表示无效；Reset 会重用序号，不能跨快照或跨身份域传递。C typedef 不提供静态域检查。
 */
typedef TZrUInt32 TZrLifetimeRegionId;
#endif

#ifndef ZR_VM_PARSER_SEMANTIC_SCOPE_ID_TYPE_DECLARED
#define ZR_VM_PARSER_SEMANTIC_SCOPE_ID_TYPE_DECLARED
/**
 * @brief 标识当前语义快照中的词法作用域。
 * @note 底层为 UInt32，0 表示无效；Reset 会重用序号，不能跨快照或跨身份域传递。C typedef 不提供静态域检查。
 */
typedef TZrUInt32 TZrSemanticScopeId;
#endif

// Semantic IDs reserve 0 as "invalid / not assigned"; allocation starts at 1.
/**
 * @brief 规定语义 ID 的无效值。
 * @note 当前值为0，跨身份域共享哨兵。
 */
#ifndef ZR_SEMANTIC_ID_INVALID
#define ZR_SEMANTIC_ID_INVALID ((TZrUInt32)0U)
#endif
/**
 * @brief 规定空快照 ID 序列的起点。
 * @note 当前值为1，由 Reset 赋给五个计数器。
 */
#ifndef ZR_SEMANTIC_ID_FIRST
#define ZR_SEMANTIC_ID_FIRST ((TZrUInt32)1U)
#endif

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_relations.h"
#include "zr_vm_parser/canonical_type.h"

/**
 * @brief 区分语义类型投影的值、引用、泛型和 union 分类。
 * @note UNKNOWN 注册推断类型时可按 baseType 回退；其他 kind 原样保存。
 */
enum EZrSemanticTypeKind {
    ZR_SEMANTIC_TYPE_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_TYPE_KIND_VALUE,
    ZR_SEMANTIC_TYPE_KIND_REFERENCE,
    ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
    ZR_SEMANTIC_TYPE_KIND_GENERIC_PARAMETER,
    ZR_SEMANTIC_TYPE_KIND_UNION,
};

typedef enum EZrSemanticTypeKind EZrSemanticTypeKind;

/**
 * @brief 区分声明符号种类以供查询和发布筛选。
 * @note 符号发布保存 kind；按名查询同时筛选 kind。
 */
enum EZrSemanticSymbolKind {
    ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
    ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
    ZR_SEMANTIC_SYMBOL_KIND_FIELD,
    ZR_SEMANTIC_SYMBOL_KIND_PROPERTY,
};

typedef enum EZrSemanticSymbolKind EZrSemanticSymbolKind;

/**
 * @brief 区分静态清理候选来自块、实例字段或 struct 值字段。
 * @note 这是计划来源分类，不能直接推断运行时动作已执行。
 */
enum EZrDeterministicCleanupKind {
    ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE = 0,
    ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD,
    ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD,
};

typedef enum EZrDeterministicCleanupKind EZrDeterministicCleanupKind;

/**
 * @brief canonical 类型身份的语义分类与结构信息投影。
 * @note inferredType 嵌套资源由 context 清理；名称和 AST 为借用，推断类型注册的 AST 为空。
 */
typedef struct SZrSemanticTypeRecord {
    TZrTypeId id;
    EZrSemanticTypeKind kind;
    EZrValueType baseType;
    EZrOwnershipQualifier ownershipQualifier;
    SZrString *name;
    SZrAstNode *astNode;
    SZrInferredType inferredType;
} SZrSemanticTypeRecord;

/**
 * @brief 快照内声明身份及类型、重载集合和源码定位的记录。
 * @note 名称、AST、location.source 为借用；记录地址依赖 symbols 缓冲。
 */
typedef struct SZrSemanticSymbolRecord {
    TZrSymbolId id;
    EZrSemanticSymbolKind kind;
    SZrString *name;
    TZrTypeId typeId;
    TZrOverloadSetId overloadSetId;
    SZrAstNode *astNode;
    SZrFileRange location;
} SZrSemanticSymbolRecord;

/**
 * @brief 标记模块、类型、函数、块与泛型词法边界。
 * @note 作用域父链用 ScopeId 建树，发布入口本身不检查 kind 范围。
 */
typedef enum EZrSemanticScopeKind {
    ZR_SEMANTIC_SCOPE_KIND_MODULE = 0,
    ZR_SEMANTIC_SCOPE_KIND_TYPE,
    ZR_SEMANTIC_SCOPE_KIND_FUNCTION,
    ZR_SEMANTIC_SCOPE_KIND_BLOCK,
    ZR_SEMANTIC_SCOPE_KIND_GENERIC
} EZrSemanticScopeKind;

/**
 * @brief 用父 ID、owner 与范围表示词法作用域树节点。
 * @note 发布入口重写 id，根父 ID 为 INVALID，source 借用；静态标记由生产者决定。
 */
typedef struct SZrSemanticScopeFact {
    TZrSemanticScopeId id;
    TZrSemanticScopeId parentScopeId;
    EZrSemanticScopeKind kind;
    SZrFileRange range;
    TZrSymbolId ownerSymbolId;
    TZrBool isStaticContext;
} SZrSemanticScopeFact;

/**
 * @brief 区分调用边缺失端点与目标声明不可定位的原因。
 * @note Publish 从 CALL 引用及词法 owner 投影；caller 缺失可覆盖 target 的缺失状态。与端点 ID 和 hasTargetDeclarationRange 一起解释，不能凭枚举值或名称补造端点。
 */
typedef enum EZrSemanticCallEdgeResolution {
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_UNKNOWN = 0,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_DECLARATION_UNAVAILABLE
} EZrSemanticCallEdgeResolution;

/*
 * A call edge is produced from existing CALL references and lexical scope
 * ownership. It never resolves an endpoint by spelling.
 */
/**
 * @brief 保存 CALL 引用及词法归属的调用图投影，包括缺失端点。
 * @note caller/target 使用同快照 SymbolId，callableTypeId 使用 canonical TypeId；resolution 与 hasTargetDeclarationRange 联合描述可用信息。未解析 target 仍可形成边，不按名字解析端点；range.source 由来源 owner 保持有效。
 */
typedef struct SZrSemanticCallEdgeFact {
    TZrSymbolId callerSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId callableTypeId;
    SZrFileRange callSiteRange;
    SZrFileRange targetDeclarationRange;
    EZrSemanticCallEdgeResolution resolution;
    TZrBool hasTargetDeclarationRange;
} SZrSemanticCallEdgeFact;

/*
 * A producer publishes one candidate for every symbol it has already bound to
 * a lexical scope. The query only projects this fact; it never searches names
 * through the global symbol registry to infer visibility.
 */
/**
 * @brief 保存生产者明确绑定的词法可见候选、声明顺序与来源身份。
 * @note 发布要求已有 scope/symbol 和非零 declarationOrder；查询消费这些显式候选。签名和 range.source 为借用。
 * URI 可产生独立长串副本；原输入的 root 不覆盖它，内部克隆缺根问题见 semantic_context_clone_string。
 */
typedef struct SZrSemanticVisibleSymbolFact {
    TZrSemanticScopeId scopeId;
    TZrSymbolId symbolId;
    TZrSymbolId ownerSymbolId;
    EZrAccessModifier access;
    TZrUInt32 declarationOrder;
    SZrFileRange declarationRange;
    SZrFileRange definitionRange;
    SZrString *signatureDisplay;
    SZrString *externalOriginUri;
    SZrFileRange externalOriginRange;
    TZrBool hasDefinitionRange;
    TZrBool hasExternalOriginRange;
    TZrBool isHoisted;
    TZrBool isAccessible;
    TZrBool isReceiverMember;
    TZrBool isStatic;
    TZrBool isImport;
    TZrBool isAlias;
    TZrBool isGenericParameter;
} SZrSemanticVisibleSymbolFact;

/**
 * @brief 关联重载集合身份、名称与符号成员数组。
 * @note 名称借用；members 的原生缓冲由 context Reset/Free 逐项释放。
 */
typedef struct SZrSemanticOverloadSetRecord {
    TZrOverloadSetId id;
    SZrString *name;
    SZrArray members; // TZrSymbolId
} SZrSemanticOverloadSetRecord;

/**
 * @brief 按判别位区分静态文本段和 AST 插值段。
 * @note staticText/expression 都不转移所有权；生产者负责正确组合，context 仅保存浅复制。
 */
typedef struct SZrTemplateSegment {
    TZrBool isInterpolation;
    SZrString *staticText;
    SZrAstNode *expression;
} SZrTemplateSegment;

/**
 * @brief 记录静态所有权清理计划的区域、声明顺序与动作属性。
 * @note 身份限本快照；callsClose/callsDestructor 是计划属性，保存本记录不会执行运行时清理。
 */
typedef struct SZrDeterministicCleanupStep {
    EZrDeterministicCleanupKind kind;
    TZrLifetimeRegionId regionId;
    TZrLifetimeRegionId ownerRegionId;
    TZrSymbolId symbolId;
    TZrInt32 declarationOrder;
    EZrOwnershipQualifier ownershipQualifier;
    EZrOwnershipBuiltinKind ownershipBuiltinKind;
    TZrBool callsClose;
    TZrBool callsDestructor;
} SZrDeterministicCleanupStep;

/**
 * @brief 关联属性声明、accessor callable、value 参数及访问和接收者效果。
 * @note property/accessor/value 使用 SymbolId，属性及 callable 使用 canonical TypeId；基础发布要求至少一个 accessor SymbolId 非 INVALID，setter/initializer 的 value 参数须与对应 accessor ID presence 配套。基础发布不验证 accessor 符号和完整 callable 关系；两个范围的 source 均借用。
 */
typedef struct SZrSemanticPropertyContract {
    TZrSymbolId propertySymbolId;
    TZrTypeId propertyTypeId;
    TZrSymbolId getterSymbolId;
    TZrSymbolId setterSymbolId;
    TZrSymbolId initializerSymbolId;
    TZrSymbolId setterValueSymbolId;
    TZrSymbolId initializerValueSymbolId;
    TZrTypeId getterCallableTypeId;
    TZrTypeId setterCallableTypeId;
    TZrTypeId initializerCallableTypeId;
    EZrAccessModifier access;
    EZrAccessModifier getterAccess;
    EZrAccessModifier setterAccess;
    EZrAccessModifier initializerAccess;
    TZrUInt32 modifierFlags;
    EZrCanonicalReceiverEffect receiverEffect;
    EZrReferenceAccess referenceAccess;
    TZrBool exportsWritableRef;
    TZrBool isStatic;
    SZrFileRange declarationRange;
    SZrFileRange selectionRange;
} SZrSemanticPropertyContract;

/**
 * @brief 承载一次语义分析快照的身份、canonical 图与查询事实。
 * @note state/AST/普通字符串及 source 由外部 owner 保持有效。context 拥有原生顶层缓冲及推断类型、重载成员、canonical 定义、引用映射和诊断等嵌套存储；原生所有权不登记 GC root。Reset 结束身份并清宿主配置和缓存。BUG: 可见候选的独立长 URI 克隆未 root。
 */
typedef struct SZrSemanticContext {
    SZrState *state;
    /* Set by the host before analysis; zero means no provider epoch supplied. */
    /**
     * @brief 保存宿主 metadata epoch 与可选声明 URI resolver 配置。
     * @note 0 generation 表示未提供 epoch；resolver/userData 为借用，在实际同步回调期间有效。context 不验证 epoch 与 resolver 的对应关系。Reset 清零，compile_script 需要跨快照保留时显式保存恢复。
     */
    TZrUInt64 externalProviderGeneration;
    /* Optional host resolver used only to attach metadata-owned declaration URIs. */
    FZrSemanticVirtualDeclarationUriResolver virtualDeclarationUriResolver;
    TZrPtr virtualDeclarationUriResolverUserData;
    /**
     * @brief 五个身份域分别分配当前快照的序号，Reset 从 FIRST 重用。
     * @note Reserve 只取号，不注册记录或撤销失败前缀；旧快照的 ID 不得用于新快照。
     */
    /* TODO: 明确 UInt32 序号耗尽后的拒绝或换代策略；五个 Reserve 当前直接后增，回绕可重用 INVALID 或已有身份。
     * 核查 ReserveTypeId/ReserveSymbolId/ReserveOverloadSetId/ReserveLifetimeRegionId/ReserveScopeId 的发布者和边界场景。
     */
    TZrTypeId nextTypeId;
    TZrSymbolId nextSymbolId;
    TZrOverloadSetId nextOverloadSetId;
    TZrLifetimeRegionId nextLifetimeRegionId;
    TZrSemanticScopeId nextScopeId;
    /**
     * @brief 保存 canonical 图、声明和查询事实的原生存储。
     * @note context 拥有顶层数组，以及 canonical 节点/定义、推断类型、重载成员、引用范围/参数映射、数值分段和诊断等嵌套资源；Reset/Free 按所属子系统释放。AST/source/普通字符串为借用，原生缓冲所有权不提供 GC root；可见 URI 独立长克隆缺少 root 见对应 BUG。
     */
    SZrArray canonicalTypes;    // SZrCanonicalTypeNode
    SZrArray canonicalTypeHashBuckets; // internal TZrUInt32 bucket heads
    SZrArray canonicalTypeHashNext; // internal TZrUInt32 collision links
    SZrArray canonicalTypeDefinitions; // internal canonical TypeDef records
    SZrArray types;             // SZrSemanticTypeRecord
    SZrArray symbols;           // SZrSemanticSymbolRecord
    SZrArray scopeFacts;        // SZrSemanticScopeFact
    SZrArray visibleSymbolFacts; // SZrSemanticVisibleSymbolFact
    SZrArray overloadSets;      // SZrSemanticOverloadSetRecord
    SZrArray cleanupPlan;       // SZrDeterministicCleanupStep
    SZrArray templateSegments;  // SZrTemplateSegment
    /**
     * @brief 以物化标记和 scope root 标识诊断查询缓存。
     * @note NULL root 表示 MODULE scope；NODE 缓存按实际 AST root 身份区分。Reset 先释放各诊断的 relatedInformation/fixes，再清除缓存身份；重新物化先释放旧条目并清长度，在物化结束时更新标记和 root。旧诊断借用视图不可跨重物化或 Reset 使用。
     */
    SZrArray queryDiagnostics;  // SZrStructuredDiagnostic
    TZrBool queryDiagnosticsMaterialized;
    const SZrAstNode *queryDiagnosticsScopeRoot; // NULL for module scope
    SZrArray expressionFacts;   // SZrSemanticExpressionFact
    SZrArray referenceFacts;    // SZrSemanticReferenceFact
    SZrArray numericFacts;      // SZrSemanticNumericFact
    SZrArray reachabilityFacts; // SZrSemanticReachabilityFact
    SZrArray logicalFacts;      // SZrSemanticLogicalFact
    SZrArray ownershipFacts;    // SZrSemanticOwnershipFact
    SZrArray ownershipIntrinsicFacts; // SZrOwnershipIntrinsicFact
    SZrArray receiverGuardFacts; // SZrReceiverGuardFact
    SZrArray diagnosticFacts;   // SZrSemanticDiagnosticFact
    SZrArray propertyContracts; // SZrSemanticPropertyContract
    SZrArray relationFacts;     // SZrSemanticRelationFact
    SZrArray callEdgeFacts;     // SZrSemanticCallEdgeFact
    SZrArray typeDisplayAliasFacts; // SZrSemanticTypeDisplayAliasFact
    SZrArray documentationFacts; // SZrSemanticDocumentationFact
} SZrSemanticContext;

/**
 * @brief 关联根 AST 与语义快照的轻量 HIR 包装。
 * @note 只拥有包装的原生存储，rootAst/semantic 均为借用；使用者保持 AST owner 和 context 存活，构造不延长其寿命。允许空 rootAst。
 */
typedef struct SZrHirModule {
    SZrAstNode *rootAst;
    SZrSemanticContext *semantic;
} SZrHirModule;

/**
 * @brief 建立与 VM state 关联的空语义快照。
 * @note 调用方提供有效 state/global，并使分配器活到 Free；空 state 或外层原生申请失败返回 NULL。BUG: types/scopeFacts 初始缓冲失败未传播，非 NULL 不证明内部容器可用，后续合法登记可对空 head 写入。
 */
ZR_PARSER_API SZrSemanticContext *ZrParser_SemanticContext_New(SZrState *state);
/**
 * @brief 逐项清理嵌套类型、成员与诊断资源，再释放上下文及其原生容器。
 * @note NULL 为无操作；非空 context 的 state/global 和分配器须存活，不得重复释放；普通字符串、AST、源码字段不由本入口销毁。
 */
ZR_PARSER_API void ZrParser_SemanticContext_Free(SZrSemanticContext *context);
/**
 * @brief 结束当前语义快照，清理嵌套资源并复用顶层缓冲区。
 * @note NULL 为无操作；所有记录、旧 ID 与借用元素地址失去旧身份；五个计数器从 FIRST 重启，provider/resolver 配置清零，由有意跨快照的宿主重新设置。
 */
ZR_PARSER_API void ZrParser_SemanticContext_Reset(SZrSemanticContext *context);

/**
 * @brief 为 canonical 类型构造保留快照内 TypeId。
 * @note 不创建类型记录；NULL 返回 INVALID；32 位序号无耗尽保护，Reset 后编号复用。
 */
ZR_PARSER_API TZrTypeId ZrParser_Semantic_ReserveTypeId(SZrSemanticContext *context);
/**
 * @brief 为后续符号发布保留快照内 SymbolId。
 * @note 不发布符号；NULL 返回 INVALID；与给定 ID 发布混用时调用者避免编号冲突，32 位序号没有耗尽保护；Reset 后编号复用。
 */
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_ReserveSymbolId(SZrSemanticContext *context);
/**
 * @brief 为新重载集合保留快照内集合编号。
 * @note 不创建集合或成员数组；NULL 返回 INVALID；32 位序号没有耗尽保护；Reset 后编号复用。
 */
ZR_PARSER_API TZrOverloadSetId ZrParser_Semantic_ReserveOverloadSetId(SZrSemanticContext *context);
/**
 * @brief 为静态所有权清理计划保留区域身份。
 * @note 不是运行时槽或资源句柄；NULL 返回 INVALID；32 位序号没有耗尽保护；Reset 后编号复用。
 */
ZR_PARSER_API TZrLifetimeRegionId ZrParser_Semantic_ReserveLifetimeRegionId(SZrSemanticContext *context);
/**
 * @brief 为词法作用域事实保留快照编号。
 * @note 不发布作用域；NULL 返回 INVALID；32 位序号没有耗尽保护；Reset 后编号复用。
 */
ZR_PARSER_API TZrSemanticScopeId ZrParser_Semantic_ReserveScopeId(SZrSemanticContext *context);

/**
 * @brief 按 canonical TypeId 复用结构类型投影。
 * @note 无法取得 canonical ID 返回 INVALID；重复 ID 不更新 kind/元数据。显式 name/astNode 不保存，name 取 typeName，AST 为空。复制拥有嵌套原生类型存储但仍借用 VM 名称，并剥离值范围、布尔和长度事实。canonical 图可先扩充；Copy/Push 没有完整 OOM 状态或事务回滚。
 */
ZR_PARSER_API TZrTypeId ZrParser_Semantic_RegisterInferredType(SZrSemanticContext *context,
                                                       const SZrInferredType *type,
                                                       EZrSemanticTypeKind kind,
                                                       SZrString *name,
                                                       SZrAstNode *astNode);
/**
 * @brief 从名称的 canonical 投影身份登记 object 语义类型记录。
 * @note 名称适配可建立 nominal 身份，再解析定义投影；最终 ID 不保证为 nominal kind。context/name 为空返回 INVALID，重复 ID 复用旧记录；名称/AST 为借用，嵌套 inferred 存储由 context 清理。canonical 前缀可先发布，底层 Push 不提供 OOM 回滚。
 */
ZR_PARSER_API TZrTypeId ZrParser_Semantic_RegisterNamedType(SZrSemanticContext *context,
                                                    SZrString *name,
                                                    EZrSemanticTypeKind kind,
                                                    SZrAstNode *astNode);
/**
 * @brief 为现有 canonical 身份建立语义类型投影。
 * @note 缺失 canonical 节点或无效 ID 返回 false；重复 ID 仅比较 kind，不更新名称/AST。首次记录借用名称/AST，并拥有 inferred 原生存储。底层初始化和 Push 没有完整 OOM 状态；true 不表示事务性发布。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_RegisterCanonicalType(SZrSemanticContext *context,
                                                       TZrTypeId typeId,
                                                       EZrSemanticTypeKind kind,
                                                       SZrString *name,
                                                       SZrAstNode *astNode);
/**
 * @brief 为声明分配身份并保存类型、重载集合与源码关联。
 * @note context/name 为空返回 INVALID；名称/AST/source 借用。同名声明允许，typeId/overloadSetId 可为 0，非零关联由 caller 保持同快照正确性。后续失败不撤回预留编号，底层 Push 不提供完整 OOM 状态。
 */
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_RegisterSymbol(SZrSemanticContext *context,
                                                   SZrString *name,
                                                   EZrSemanticSymbolKind kind,
                                                   TZrTypeId typeId,
                                                   TZrOverloadSetId overloadSetId,
                                                   SZrAstNode *astNode,
                                                   SZrFileRange location);
/**
 * @brief 以调用者提供的 SymbolId 发布一条声明身份记录。
 * @note 拒绝空 context/name、INVALID 和重复符号 ID；不查验 typeId/overloadSetId，不推进 nextSymbolId；调用者保持关联 ID 的快照归属并避免与预留编号冲突；输入指针为借用。
 */
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_RegisterSymbolWithId(SZrSemanticContext *context,
                                                         TZrSymbolId symbolId,
                                                         SZrString *name,
                                                         EZrSemanticSymbolKind kind,
                                                         TZrTypeId typeId,
                                                         TZrOverloadSetId overloadSetId,
                                                          SZrAstNode *astNode,
                                                          SZrFileRange location);
/**
 * @brief 按当前快照的身份查询已发布符号。
 * @note 无效输入或缺失返回 NULL；返回 symbols 元素的借用地址，数组扩容、Reset 或 Free 后不可继续使用。
 */
ZR_PARSER_API const SZrSemanticSymbolRecord *ZrParser_Semantic_FindSymbolById(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId);
/**
 * @brief 浅复制词法作用域描述并分配新的 ScopeId 后发布。
 * @note 非根 parentScopeId 必须已在本 context；输入 id 被覆盖，ownerSymbolId 和 kind 不在此处校验；range.source 借用；显式校验失败返回 INVALID，追加底层没有失败状态。
 */
ZR_PARSER_API TZrSemanticScopeId ZrParser_Semantic_PublishScopeFact(
        SZrSemanticContext *context,
        const SZrSemanticScopeFact *fact);
/**
 * @brief 按当前快照的 ScopeId 查询词法边界。
 * @note 无效输入或缺失返回 NULL；返回 scopeFacts 元素的借用地址，数组扩容、Reset 或 Free 后不可继续使用。
 */
ZR_PARSER_API const SZrSemanticScopeFact *ZrParser_Semantic_FindScopeFactById(
        const SZrSemanticContext *context,
        TZrSemanticScopeId scopeId);
/**
 * @brief 发布已绑定作用域和符号的词法可见候选，供补全和导入身份查询使用。
 * @pre scope/symbol 已存在，declarationOrder 非零；签名与 source 的 owner 覆盖后续查询。
 * @note 仅 URI 经工厂处理，创建显式返回 NULL 时拒绝发布；底层 Push 无失败状态，true 不提供完整 OOM 保证。
 * 独立长 URI 的 GC 根尚未交接，原输入有根也不能保住副本；限制见 semantic_context_clone_string 的 BUG。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_PublishVisibleSymbolFact(
        SZrSemanticContext *context,
        const SZrSemanticVisibleSymbolFact *fact);
/**
 * @brief 按字符串内容与符号分类返回首个匹配记录。
 * @note 不进行词法作用域筛选；无效输入或缺失返回 NULL；结果借用 symbols 缓冲，扩容、Reset、Free 后失效。
 */
ZR_PARSER_API const SZrSemanticSymbolRecord *ZrParser_Semantic_FindSymbolByNameAndKind(
        const SZrSemanticContext *context,
        SZrString *name,
        EZrSemanticSymbolKind kind);
/**
 * @brief 保持 SymbolId，将其类型投影改绑到已存在的 canonical TypeId。
 * @note 目标 canonical 类型和符号必须存在；失败返回 false，成功只改 symbol.typeId，不改 canonical 节点。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_RebindSymbolType(
        SZrSemanticContext *context,
        TZrSymbolId symbolId,
        TZrTypeId typeId);
/**
 * @brief 依次发布 canonical 类型投影和 TYPE 声明符号。
 * @note canonical 类型须存在，SymbolId 由 caller 预留；拒绝重复类型 ID、符号 ID 或同名 TYPE。名称/AST/source 借用，不推进 nextSymbolId。两次 Push 顺序执行，无追加失败返回或事务回滚，不能假定两条记录原子出现。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_PublishCanonicalTypeSymbol(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrSemanticTypeKind typeKind,
        SZrString *name,
        SZrAstNode *astNode,
        TZrSymbolId symbolId,
        SZrFileRange location);
/**
 * @brief 按名称内容取得当前快照的重载集合。
 * @note 不按 scope/kind 分组；context/name 为空返回 INVALID。名称借用，members 缓冲由 context Reset/Free 释放。BUG: members 初始分配失败仍发布非零 ID，LSP 登记函数后 AddOverloadMember 可向空 head 写入。
 */
ZR_PARSER_API TZrOverloadSetId ZrParser_Semantic_GetOrCreateOverloadSet(SZrSemanticContext *context,
                                                                SZrString *name);
/**
 * @brief 向重载集合加入同快照的符号身份，重复成员为幂等成功。
 * @note 空 context、无效 ID 或缺失集合返回 false；不查询 symbols，caller 应提供已发布 SymbolId。集合和成员属于不同身份域。底层 Push 不提供 OOM 状态。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_AddOverloadMember(SZrSemanticContext *context,
                                                TZrOverloadSetId overloadSetId,
                                                TZrSymbolId symbolId);

/**
 * @brief 保存静态确定性清理计划的一条记录。
 * @note context/step 为空返回 false；值复制，不执行 close/destructor，不验证区域和符号存在。记录随 Reset 丢弃。底层 Push 无失败状态，因此返回 true 不保证 OOM 时成功，也不回滚已保存前缀。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_AppendCleanupStep(SZrSemanticContext *context,
                                                const SZrDeterministicCleanupStep *step);
/**
 * @brief 按生产者顺序保存静态文本段或 AST 插值段。
 * @note 空 context/segment 返回 false；仅浅复制，不验证判别位与指针组合。staticText 的实际 VM root 和 expression 的 AST owner 须覆盖消费期间，context 不接管二者；底层 Push 没有 OOM 状态或前缀回滚。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_AppendTemplateSegment(SZrSemanticContext *context,
                                                    const SZrTemplateSegment *segment);
/**
 * @brief 在基础声明链接校验后发布属性契约。
 * @note PROPERTY 符号及类型须一致，至少一个 accessor SymbolId 非 INVALID；setter/initializer 的 value PARAMETER 须与对应 accessor ID presence 配套且类型匹配，重复 property 拒绝。后续关系发布校验已配置 accessor 为 FUNCTION、callable ID 非零且等于该符号的 typeId，不构成 canonical 节点及完整关系校验。各 ID 应属于同快照，range.source 借用；底层 Push 无 OOM 状态。
 */
ZR_PARSER_API TZrBool ZrParser_Semantic_PublishPropertyContract(
        SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract);

/**
 * @brief 分配只关联 root AST 与 semantic context 的轻量 HIR 包装。
 * @note state/context 为空或原生分配失败返回 NULL；rootAst 可为空；不接管两个指针，调用方保持其生命周期。
 */
ZR_PARSER_API SZrHirModule *ZrParser_HirModule_New(SZrState *state,
                                           SZrSemanticContext *context,
                                           SZrAstNode *rootAst);
/**
 * @brief 释放 HIR 包装的原生存储。
 * @note state/module 为空为无操作；使用创建时兼容的同一 global 分配器，不释放 root AST 或 context。
 */
ZR_PARSER_API void ZrParser_HirModule_Free(SZrState *state, SZrHirModule *module);

#endif // ZR_VM_PARSER_SEMANTIC_H
