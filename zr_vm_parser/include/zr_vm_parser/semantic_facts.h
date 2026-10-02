/** @file
 * @brief 声明语义事实的发布和查询契约，供分析器与 compiler、LSP、debug/REPL 在同一快照内协作。
 * @note ID 域、复制与借用、查询视图有效期以及内部 VM 文本缺根限制见共享事实生命周期说明。
 */
#ifndef ZR_VM_PARSER_SEMANTIC_FACTS_H
#define ZR_VM_PARSER_SEMANTIC_FACTS_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

/* semantic、canonical_type 与 type_system 共享这组 ID typedef，避免同一 translation unit 重复声明。
 * 四类 ID 仍是相同底层整数类型，C 不会静态阻止跨身份域混用。
 */
#ifndef ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
#define ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
/** @brief 经当前 semantic/canonical context 登记或预留的类型身份；跨 context、Reset 或语义快照不能仅凭同一整数恢复类型。 */
typedef TZrUInt32 TZrTypeId;
/** @brief 当前语义 context 的符号身份域；C 底层整数与其它 ID 相同，实际身份须结合所属 context、快照和 reference origin。 */
typedef TZrUInt32 TZrSymbolId;
/** @brief 当前context的重载集合身份；不是单一callee SymbolId。 */
typedef TZrUInt32 TZrOverloadSetId;
/** @brief context 所有权分析的生命周期区域身份；0 为未分配/未知，与 SemanticIr RegionId、LoanId 和 PlaceId 分域。 */
typedef TZrUInt32 TZrLifetimeRegionId;
#endif

/** @brief 本组 Type/Symbol/OverloadSet/LifetimeRegion ID 保留的 0 无效值；不能据此解释其它 ID 或零起点索引。 */
#ifndef ZR_SEMANTIC_ID_INVALID
#define ZR_SEMANTIC_ID_INVALID ((TZrUInt32)0U)
#endif

/* context 的完整数组/状态定义由 semantic.h 提供，facts API 在此只借用其地址。 */
typedef struct SZrSemanticContext SZrSemanticContext;

/** @brief 标记事实证据的精确程度，供后续查询决定是否采用投影。
 * @note 该标签不证明分析成功或 TypeId 有效；数值查询在信息竞争时优先更精确的证据。
 */
typedef enum EZrSemanticFactExactness {
    ZR_SEMANTIC_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_FACT_APPROXIMATE,
    ZR_SEMANTIC_FACT_EXACT
} EZrSemanticFactExactness;

/** @brief 实参与形参的转换标签：有效 canonical TypeId 相等为 EXACT，不同为 IMPLICIT，UNKNOWN 不提供有效转换证据。
 * @note 标签不独立证明兼容性已检查；producer 可根据 validateCompatibility 跳过该检查，consumer 仍须遵守自己的调用契约门槛。
 */
typedef enum EZrSemanticCallConversion {
    ZR_SEMANTIC_CALL_CONVERSION_UNKNOWN = 0,
    ZR_SEMANTIC_CALL_CONVERSION_EXACT,
    ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT
} EZrSemanticCallConversion;

typedef enum EZrSemanticExpressionFactKind {
    ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_EXPRESSION_FACT_LITERAL,
    ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER,
    ZR_SEMANTIC_EXPRESSION_FACT_BINARY,
    ZR_SEMANTIC_EXPRESSION_FACT_UNARY,
    ZR_SEMANTIC_EXPRESSION_FACT_CALL,
    ZR_SEMANTIC_EXPRESSION_FACT_MEMBER,
    ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT,
    ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL,
    ZR_SEMANTIC_EXPRESSION_FACT_ARRAY,
    ZR_SEMANTIC_EXPRESSION_FACT_OBJECT,
    ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA,
    ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN,
    ZR_SEMANTIC_EXPRESSION_FACT_CONVERSION,
    ZR_SEMANTIC_EXPRESSION_FACT_ERROR
} EZrSemanticExpressionFactKind;

typedef enum EZrSemanticValueKind {
    ZR_SEMANTIC_VALUE_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_VALUE_KIND_NULL,
    ZR_SEMANTIC_VALUE_KIND_BOOL,
    ZR_SEMANTIC_VALUE_KIND_INT64,
    ZR_SEMANTIC_VALUE_KIND_UINT64,
    ZR_SEMANTIC_VALUE_KIND_DOUBLE,
    ZR_SEMANTIC_VALUE_KIND_STRING
} EZrSemanticValueKind;

/** @brief 表达式诊断 severity；当前 query 将 DEFAULT、ERROR 及未识别值映射为 ERROR。 */
typedef enum EZrSemanticDiagnosticSeverity {
    ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_DEFAULT = 0,
    ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_ERROR,
    ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_WARNING,
    ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_INFO,
    ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_HINT
} EZrSemanticDiagnosticSeverity;

/** @brief 表示引用在程序中的用途，供导航、调用与数据流查询解释同一节点的不同证据。
 * @note DECLARATION/WRITE 可贡献定义，READ 可读取定义和赋值状态；该分类本身不证明身份已解析。
 */
typedef enum EZrSemanticReferenceKind {
    ZR_SEMANTIC_REFERENCE_UNKNOWN = 0,
    ZR_SEMANTIC_REFERENCE_DECLARATION,
    ZR_SEMANTIC_REFERENCE_READ,
    ZR_SEMANTIC_REFERENCE_WRITE,
    ZR_SEMANTIC_REFERENCE_CALL,
    ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS,
    ZR_SEMANTIC_REFERENCE_MEMBER_WRITE,
    ZR_SEMANTIC_REFERENCE_TYPE
} EZrSemanticReferenceKind;

/** @brief 外部 provider 目标类别；使用时结合 hasExternalTarget、owner/token/hash 与 provider 快照，generation 为 0 不单独使事实无效。 */
typedef enum EZrSemanticExternalTargetKind {
    ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN = 0,
    ZR_SEMANTIC_EXTERNAL_TARGET_MODULE,
    ZR_SEMANTIC_EXTERNAL_TARGET_CALLABLE,
    ZR_SEMANTIC_EXTERNAL_TARGET_TYPE,
    ZR_SEMANTIC_EXTERNAL_TARGET_VALUE,
    ZR_SEMANTIC_EXTERNAL_TARGET_FIELD,
    ZR_SEMANTIC_EXTERNAL_TARGET_PROPERTY
} EZrSemanticExternalTargetKind;

/** @brief 定义赋值状态；hasDefiniteAssignmentState 控制存在性，UNKNOWN 不等于 UNINIT。 */
typedef enum EZrSemanticDefiniteAssignmentState {
    ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN = 0,
    ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT,
    ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
    ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT
} EZrSemanticDefiniteAssignmentState;

typedef enum EZrSemanticNumericFactKind {
    ZR_SEMANTIC_NUMERIC_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_NUMERIC_FACT_LITERAL,
    ZR_SEMANTIC_NUMERIC_FACT_PROMOTION,
    ZR_SEMANTIC_NUMERIC_FACT_CONVERSION,
    ZR_SEMANTIC_NUMERIC_FACT_RANGE
} EZrSemanticNumericFactKind;

typedef enum EZrSemanticReachabilityState {
    ZR_SEMANTIC_REACHABILITY_UNKNOWN = 0,
    ZR_SEMANTIC_REACHABILITY_REACHABLE,
    ZR_SEMANTIC_REACHABILITY_UNREACHABLE
} EZrSemanticReachabilityState;

/** @brief 记录节点为何不可达，供诊断和位置查询说明控制流证据。
 * @note AFTER_RETURN/THROW/BREAK/CONTINUE 指转移之后的不可达原因，不表示当前事实节点就是转移语句。
 */
typedef enum EZrSemanticReachabilityCause {
    ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN = 0,
    ZR_SEMANTIC_REACHABILITY_AFTER_RETURN,
    ZR_SEMANTIC_REACHABILITY_AFTER_THROW,
    ZR_SEMANTIC_REACHABILITY_AFTER_BREAK,
    ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE,
    ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE,
    ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
    ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT,
    ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH,
    ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP
} EZrSemanticReachabilityCause;

/** @brief 逻辑事实用途；hasKnownValue 控制值存在性，条件证明 consumer 另筛 ALWAYS_TRUE/FALSE。 */
typedef enum EZrSemanticLogicalFactKind {
    ZR_SEMANTIC_LOGICAL_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_LOGICAL_FACT_TRUTHY,
    ZR_SEMANTIC_LOGICAL_FACT_FALSY,
    ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE,
    ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
    ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT
} EZrSemanticLogicalFactKind;

typedef enum EZrSemanticOwnershipFactKind {
    ZR_SEMANTIC_OWNERSHIP_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION,
    ZR_SEMANTIC_OWNERSHIP_FACT_BORROW,
    ZR_SEMANTIC_OWNERSHIP_FACT_MOVE,
    ZR_SEMANTIC_OWNERSHIP_FACT_COPY,
    ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE,
    ZR_SEMANTIC_OWNERSHIP_FACT_ERROR
} EZrSemanticOwnershipFactKind;

/** @brief 承载表达式事实中可供常量展示和语义查询读取的值。
 * @note 仅在 hasConstant 为真时按 valueKind 读取相应成员；stringValue 借用 AST 的 GC 字符串，Append 不重建它。
 */
typedef union SZrSemanticConstantValue {
    TZrBool boolValue;
    TZrInt64 int64Value;
    TZrUInt64 uint64Value;
    TZrDouble doubleValue;
    SZrString *stringValue;
} SZrSemanticConstantValue;

/** @brief 保存节点最近发布的表达式证据，供后续语义查询复用类型、常量、调用、成员与诊断信息。
 * @note Append 独立复制类型原生容器并重建可选展示文本；类型名、AST、source 与常量字符串仍借用当前快照。hasCallInfo/hasMemberInfo/hasConstant 分别控制相应载荷。
 */
typedef struct SZrSemanticExpressionFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticExpressionFactKind kind;
    EZrSemanticFactExactness exactness;
    SZrInferredType inferredType;
    TZrTypeId typeId;
    EZrSemanticValueKind valueKind;
    TZrBool hasConstant;
    SZrSemanticConstantValue constantValue;
    TZrBool hasCallInfo;
    SZrString *callTargetName;
    SZrFileRange callTargetRange;
    TZrSize argumentCount;
    TZrBool hasNamedArguments;
    TZrBool isMemberCall;
    TZrBool hasMemberInfo;
    SZrString *memberName;
    SZrFileRange memberRange;
    TZrBool memberIsComputed;
    SZrString *diagnosticMessage;
    SZrString *diagnosticCode;
    EZrSemanticDiagnosticSeverity diagnosticSeverity;
} SZrSemanticExpressionFact;

/** @brief 实参到形参的映射；argumentIndex/parameterIndex 为零起点序号，TypeId 属于当前 canonical context，range.source 借用；映射可缺省。 */
typedef struct SZrSemanticCallArgumentFact {
    TZrSize argumentIndex;
    TZrSize parameterIndex;
    SZrFileRange argumentRange;
    TZrTypeId argumentTypeId;
    TZrTypeId parameterTypeId;
    EZrParameterPassingMode passingMode;
    EZrSemanticCallConversion conversion;
    TZrBool isNamed;
} SZrSemanticCallArgumentFact;

/** @brief 保存引用的身份、定义、调用映射、赋值状态与外部目标证据，供导航和后续语义分析共用。
 * @note isResolved 不代替各 consumer 的 ID 和完整性门槛；context ID 不能跨快照凭数值恢复，provider token 与 origin token/index 又是不同身份域。嵌套原生数组由 context 管理，name、AST 和 source 借用当前快照。
 */
typedef struct SZrSemanticReferenceFact {
    SZrAstNode *node;
    SZrFileRange range;
    /** @brief hasDefinitionRange 控制单一定义投影；definitionRanges 保存 CFG 的多个候选范围并由 context 管理原生容器，range.source 仍借用。 */
    SZrFileRange declarationRange;
    SZrFileRange definitionRange;
    SZrArray definitionRanges;
    SZrArray argumentMappings;
    TZrBool hasDefinitionRange;
    EZrSemanticReferenceKind kind;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrTypeId receiverTypeId;
    TZrUInt32 placeId;
    TZrUInt32 contractRole;
    EZrOwnershipQualifier ownershipQualifier;
    SZrString *name;
    SZrString *signatureDisplay;
    /** @brief provider 发布的 owner、generation、metadata/signature token 与 hash；这些域不同于 semantic ID，generation 0 不单独阻止外部目标投影。 */
    SZrString *externalOwnerIdentity;
    TZrUInt64 externalProviderGeneration;
    TZrUInt32 externalMetadataToken;
    TZrUInt32 externalSignatureToken;
    TZrUInt64 externalSignatureHash;
    EZrSemanticExternalTargetKind externalTargetKind;
    TZrBool hasExternalTarget;
    /** @brief hasDefiniteAssignmentState 控制状态存在性；UNKNOWN 不是 UNINIT，查询仅对有状态的 READ 投影相应诊断。 */
    EZrSemanticDefiniteAssignmentState definiteAssignmentState;
    TZrBool hasDefiniteAssignmentState;
    TZrBool isResolved;
    EZrSemanticReferenceOriginKind originKind;
    EZrSemanticRuntimeRootKind runtimeRootKind;
    TZrUInt64 originToken;
    TZrUInt32 originIndex;
} SZrSemanticReferenceFact;

/** @brief 保存数值表达式的范围和风险证据，供语义查询、范围展示及溢出诊断使用。
 * @note hasRange 配合数值类型决定 signed/double 边界，hasUnsignedRange 控制 unsigned 投影。分段原生载荷由 context 管理，AST/source 借用；mayOverflow 是风险证据，不能当作分析流程失败。
 */
typedef struct SZrSemanticNumericFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticNumericFactKind kind;
    EZrSemanticFactExactness exactness;
    EZrValueType sourceType;
    EZrValueType targetType;
    TZrBool hasRange;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrSize rangeSegmentCount;
    SZrNumericRangeSegment rangeSegments[ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY];
    SZrArray rangeExtraSegments;
    TZrBool hasUnsignedRange;
    TZrUInt64 minUnsignedValue;
    TZrUInt64 maxUnsignedValue;
    TZrDouble minDoubleValue;
    TZrDouble maxDoubleValue;
    TZrBool mayOverflow;
} SZrSemanticNumericFact;

/** @brief 节点可达性及cause节点投影；均不拥有AST/source。 */
typedef struct SZrSemanticReachabilityFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticReachabilityState state;
    EZrSemanticReachabilityCause cause;
    SZrAstNode *causeNode;
} SZrSemanticReachabilityFact;

/** @brief 保存条件和短路表达式的已知逻辑证据，供后续推断和语义查询解释节点。
 * @note hasKnownValue 控制值存在性；relatedNode 是借用的相关 AST 见证，不固定表示左条件。能否用作条件证明仍由 consumer 按 kind 判断。
 */
typedef struct SZrSemanticLogicalFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticLogicalFactKind kind;
    EZrSemanticFactExactness exactness;
    TZrBool knownValue;
    TZrBool hasKnownValue;
    SZrAstNode *relatedNode;
} SZrSemanticLogicalFact;

/** @brief 所有权 transition/violation 与 region/cause 投影；diagnosticMessage 可供 debug/REPL 展示。
 * @note Append 重建可选消息，但当前原生事实未为内部 VM 长串副本建立根；其生命周期限制见共享事实说明。
 */
typedef struct SZrSemanticOwnershipFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticOwnershipFactKind kind;
    EZrOwnershipQualifier qualifier;
    TZrSymbolId symbolId;
    TZrLifetimeRegionId lifetimeRegionId;
    TZrLifetimeRegionId ownerLifetimeRegionId;
    SZrAstNode *relatedNode;
    TZrBool isViolation;
    SZrString *diagnosticMessage;
} SZrSemanticOwnershipFact;

/** @brief intrinsic 决策与 input/result 类型投影；Append 复制类型原生容器，类型名、AST 和 source 借用。placeId 与 loanId 分域，当前 producer 的 loanId 为 0。 */
typedef struct SZrOwnershipIntrinsicFact {
    SZrAstNode *node;
    SZrAstNode *argument;
    SZrFileRange range;
    SZrFileRange nameRange;
    SZrFileRange argumentRange;
    EZrOwnershipIntrinsicOperation operation;
    SZrInferredType inputType;
    SZrInferredType resultType;
    TZrUInt32 placeId;
    TZrUInt32 loanId;
    TZrBool consuming;
} SZrOwnershipIntrinsicFact;

typedef enum EZrReceiverGuardKind {
    ZR_RECEIVER_GUARD_NULL = 0,
    ZR_RECEIVER_GUARD_WEAK_WAKE,
} EZrReceiverGuardKind;

typedef enum EZrReceiverGuardMode {
    ZR_RECEIVER_GUARD_DIRECT = 0,
    ZR_RECEIVER_GUARD_OPTIONAL,
} EZrReceiverGuardMode;

/** @brief 表示 receiver guard 跳过访问链时提供何种结果，供 compiler 生成 optional 访问的指令。
 * @note DIRECT 使用 UNCHANGED；optional 结果可为 NULLABLE，最终为 NULL/void 时使用 VOID_NOOP。
 */
typedef enum EZrReceiverGuardResultLift {
    ZR_RECEIVER_GUARD_RESULT_UNCHANGED = 0,
    ZR_RECEIVER_GUARD_RESULT_NULLABLE,
    ZR_RECEIVER_GUARD_RESULT_VOID_NOOP,
} EZrReceiverGuardResultLift;

/** @brief 保存 receiver 访问链的 guard 决策，供 compiler 在同一 AST 快照中解释检查和跳过访问的结果。
 * @note chainSegmentStart/End 是零起点半开区间 [start,end)，node 为 firstSegment；两种类型的原生容器独立，类型名、receiver 和 source 仍借用。
 */
typedef struct SZrReceiverGuardFact {
    SZrAstNode *node;
    SZrAstNode *receiver;
    SZrAstNode *firstSegment;
    SZrFileRange range;
    EZrReceiverGuardKind kind;
    EZrReceiverGuardMode mode;
    EZrReceiverGuardResultLift resultLift;
    TZrSize chainSegmentStart;
    TZrSize chainSegmentEnd;
    SZrInferredType receiverType;
    SZrInferredType guardedType;
} SZrReceiverGuardFact;

/** @brief 保存生产阶段的结构化诊断，供后续语义查询继续使用完整诊断证据。
 * @note Append 为 context 建立独立原生诊断载荷，Reset 负责释放；node 和诊断范围的 source 仍借用，VM 文本由 GC 管理。
 */
typedef struct SZrSemanticDiagnosticFact {
    SZrAstNode *node;
    SZrStructuredDiagnostic diagnostic;
} SZrSemanticDiagnosticFact;

/* 事实与相关 AST/source 属于同一语义快照；ID 域由字段及所属注册表解释，ordinal 不作为 ID。
 * Append 复制指定原生容器；类型名、constant string、reference name 仍借用，部分展示文本在 state 中重建。
 * Find 返回数组内借用指针；增长、Reset/Free 或对应载荷替换后失效。
 * 位置查询仅判断 position.start：双方各有非零端 offset 时用 offset 闭区间，否则用行列，width 评分始终用 offset 差。
 * Init/Copy/Push 并非都报告分配失败；各 bool API 仅承诺实现显式检查的失败，不构成全局 OOM 或回滚保证。
 * 内部独立长串副本当前没有 context GC 根；只保持原输入有根不能保住副本，限制见 semantic_facts_clone_string 的 BUG。
 */
/** @brief 初始化新 context 的事实与关系子系统，供生产阶段发布可由后续查询复用的证据。
 * @note 要求有效 state 且 context 尚未持有待释放的旧事实载荷；本 API 不负责 context 本体，void 返回不报告初始化或分配结果。
 */
ZR_PARSER_API void ZrParser_SemanticFacts_Init(SZrSemanticContext *context);
/** @brief 结束当前事实批次的使用，使 context 的事实容器可以接收下一批生产结果。
 * @note 释放事实持有的原生载荷并使旧事实与关系借用失效；保留可复用缓冲。此 Reset 不重置全 context 的注册实体、ID 计数或 provider generation。
 */
ZR_PARSER_API void ZrParser_SemanticFacts_Reset(SZrSemanticContext *context);
/** @brief 结束 context 事实子系统的使用并释放其持有的原生载荷。
 * @note 不释放 context 本体；正常由 SemanticContext_Free 完成整体销毁。调用后不得继续使用事实或 relations 的借用指针。
 */
ZR_PARSER_API void ZrParser_SemanticFacts_Free(SZrSemanticContext *context);

/** @brief 发布节点的表达式证据，使后续语义查询复用生产阶段的类型、常量与展示信息。
 * @note 非空同节点代表最近发布的投影；已有 TypeId 须由生产方保持与 context 同代。context 管理类型原生载荷；可选展示文本以 state 重建但未因此取得 context GC 根。类型名、AST/source 与常量字符串仍借用；同节点替换使旧类型载荷失效。
 * @return FALSE 仅报告输入或事实数组有效性检查失败；类型 Copy 无失败返回，可选文本重建为 NULL 也可成功，不保证报告全部分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendExpression(SZrSemanticContext *context,
                                                              const SZrSemanticExpressionFact *fact);
/** @brief 发布可供后续语义查询使用的完整结构化诊断。
 * @note 需要有效 code/message 及修复或不可修复理由；相同位置/code/message 的既有诊断可复用，不合并其余载荷。context 持有独立原生副本，AST/source 仍借用。
 * @return TRUE 包括已有同一诊断的情形；FALSE 报告校验或显式复制失败，不保证报告底层 Push 分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticDiagnosticFact *fact);
/** @brief 发布引用身份及相关定义、调用和外部目标证据，供导航与后续语义分析共用。
 * @note context 持有独立嵌套原生容器，生产方返回后可释放输入容器；name、AST/source 借用。context ID 的同代性由生产方保证，本 API 不重映射；可选 signature/owner 文本重建失败并不自动拒绝事实。
 * @return FALSE 报告输入或容器复制 helper 的显式失败；这些失败分支清理未发布的原生副本，不保证检测所有分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReference(SZrSemanticContext *context,
                                                             const SZrSemanticReferenceFact *fact);
/** @brief 在不建立控制流模型的事实批次中提供按发布顺序解释的定义导航证据。
 * @note 仅 READ 使用同 symbol 的已有 resolved 声明/写入证据；结果不代表分支或循环上的路径合并，也不保证清除未获得新证据的既有单一定义投影。
 * @return TRUE 表示该线性解析完成；FALSE 表示输入有效性检查失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(
        SZrSemanticContext *context);
/** @brief 为后续定义导航提供控制流路径上的引用定义证据。
 * @note 借用稳定 context/reference facts 与 root AST；当前根覆盖限定于 script/function declaration 和 block 路径，不承诺覆盖所有可调用体。已写入结果不因后续失败回滚。
 * @return TRUE 表示本次解析流程成功；FALSE 表示输入或显式分析失败，不保证捕获全部分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(
        SZrSemanticContext *context,
        SZrAstNode *root);
/** @brief 在按发布顺序解释的事实批次中提供读取的赋值状态证据。
 * @note 不表示控制流路径合并；最近相关定义状态未知时，不能用更早已赋值状态补作证据。
 * @return TRUE 表示线性解析完成，可仍含未初始化读取；FALSE 不表示语义违规本身。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(
        SZrSemanticContext *context);
/** @brief 为后续未初始化读取诊断提供控制流路径上的赋值状态证据。
 * @note 借用同一快照的 context/reference facts 与 root AST；当前根覆盖限定于 script/function declaration 和 block 路径，不承诺覆盖所有可调用体。已写入状态不因后续失败回滚。
 * @return TRUE 表示分析流程成功，可仍含语义违规；FALSE 表示输入或显式分析失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(
        SZrSemanticContext *context,
        SZrAstNode *root);
/** @brief 为 compiler 与语义查询提供控制流中的所有权迁移和违规证据。
 * @note 调用期间 reference facts 的索引、身份及 root AST 须稳定；临时分析载荷由本次调用管理，区域绑定和已发布事实在失败后可能保留。
 * @return TRUE 表示分析及发布流程成功，可仍含语义违规；FALSE 表示输入或分析未完成。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_ResolveControlFlowOwnership(
        SZrSemanticContext *context,
        SZrAstNode *root);
/** @brief 发布数值范围及风险证据，供后续查询、展示和诊断读取。
 * @note 输入分段的 count、inline/extra 形状及边界须一致；context 管理独立原生分段载荷，AST/source 借用。
 * @return FALSE 报告输入或分段复制的显式失败；不保证检测分配失败，也不能据此推定畸形源的部分 extra 载荷已清理。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendNumeric(SZrSemanticContext *context,
                                                           const SZrSemanticNumericFact *fact);
/** @brief 为数值证据的消费者提供指定范围段的只读视图。
 * @note index 为零起点段序号；返回指针借用输入 fact 的存储，与 context 中是否登记该 fact 无关。
 * @return 空 fact、越界或缺少相应 extra 载荷时返回 NULL。
 */
ZR_PARSER_API const SZrNumericRangeSegment *ZrParser_SemanticNumericFact_RangeSegmentAt(
        const SZrSemanticNumericFact *fact,
        TZrSize index);
/** @brief 发布节点可达性及原因证据，供后续诊断和位置查询使用。
 * @note node、causeNode 与 source 借用当前 AST/语义快照，context 不接管它们。
 * @return FALSE 表示输入或事实数组有效性检查失败，不保证检测底层数组分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReachability(SZrSemanticContext *context,
                                                                const SZrSemanticReachabilityFact *fact);
/** @brief 发布逻辑分类和可选已知值证据，供后续推断与语义查询复用。
 * @note AST、relatedNode 和 source 借用当前快照；knownValue 的使用须遵守 hasKnownValue 及 consumer 的分类门槛。
 * @return FALSE 表示输入或事实数组有效性检查失败，不保证检测底层数组分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendLogical(SZrSemanticContext *context,
                                                           const SZrSemanticLogicalFact *fact);
/** @brief 发布所有权 transition/violation 投影，借用 AST/source/relatedNode，并以当前 state 重建可选消息。
 * @note 非空消息创建失败时拒绝发布；创建成功不意味着内部消息已取得 GC 根。消息由 debug/REPL 实际读取，内部 clone 缺根限制见共享事实生命周期说明。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendOwnership(SZrSemanticContext *context,
                                                             const SZrSemanticOwnershipFact *fact);
/** @brief 发布 ownership intrinsic 的决策和类型证据，供同一快照的后续消费者使用。
 * @note context 管理 input/result 类型的独立原生容器；类型名、node/argument 及各 range.source 仍借用。
 * @return FALSE 表示输入或事实数组有效性检查失败；类型 Copy 无失败返回，不保证检测分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendOwnershipIntrinsic(
        SZrSemanticContext *context,
        const SZrOwnershipIntrinsicFact *fact);
/** @brief 发布 receiver guard 的访问链证据，供 compiler 在当前 AST 快照中解释 guard。
 * @note context 管理类型的独立原生容器；类型名、receiver/firstSegment/node/source 以及链序号仍绑定当前快照。
 * @return FALSE 表示输入或事实数组有效性检查失败；类型 Copy 无失败返回，不保证检测分配失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReceiverGuard(
        SZrSemanticContext *context,
        const SZrReceiverGuardFact *fact);

/** @brief 为节点语义查询提供该节点最近发布的表达式证据。
 * @note 不补造或重新推断缺失事实；同节点替换还会使旧 inferredType 原生载荷失效。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
/** @brief 为位置语义查询提供当前位置的表达式证据。
 * @note 位置仅取 position.start；双方各有非零端 offset 时按 offset 闭区间命中，否则按行列。命中须同源，跨度评分只取 offset 差。 该选择仅定位已有投影，不证明类型可供规范查询使用；等宽候选采用后项。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
/** @brief 为导航和位置语义查询提供当前位置的引用证据。
 * @note 位置仅取 position.start；两个 range 各有至少一个非零端 offset 时按 offset 闭区间命中，否则按行列。命中须同源；先偏好 range 起点与光标相同的候选，再比较较小 offset 跨度和较高引用用途优先级，完全平局保留前项。返回事实仍须满足具体 consumer 的身份门槛。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
/** @brief 为需要特定引用用途的语义查询提供当前位置的引用证据。
 * @note 位置仅取 position.start；双方各有非零端 offset 时按 offset 闭区间命中，否则按行列。命中须同源，跨度评分只取 offset 差。 用途由 kind 限定；等宽候选保留前项，不采用普通引用位置查询的起点偏好。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
        const SZrSemanticContext *context,
        SZrFileRange position,
        EZrSemanticReferenceKind kind);
/** @brief 为依赖节点 identity 和引用用途的分析提供已有引用证据。
 * @note kind 区分同节点的不同用途；该查询不验证 resolved、ID 或 source 的有效性，也不补造缺失事实。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        EZrSemanticReferenceKind kind);
/** @brief 为节点语义查询提供按既有信息排序选出的单条数值事实，不合并范围或汇总其它候选的风险。
 * @note mayOverflow 只在 exactness、分段信息、signed 范围存在性及宽度、unsigned 范围存在性均相同时作为最后偏好；完全平局保留前项，返回事实不能解释为无溢出证明。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticNumericFact *ZrParser_SemanticFacts_FindNumericByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
/** @brief 为位置语义查询提供节点可达性和原因的已有证据。
 * @note 只定位包含 position.start 的同源事实；优先直接转移之后的原因，等优先级保留前项。返回事实未按可达状态筛选，诊断 consumer 仍须检查 state。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticReachabilityFact *ZrParser_SemanticFacts_FindReachabilityAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
/** @brief 为节点语义查询和后续推断提供已有逻辑证据。
 * @note 同节点可有不同用途的逻辑事实；返回首条并不证明 hasKnownValue 或可作条件证明，consumer 须检查相应字段。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
/** @brief 为位置语义查询提供当前位置的逻辑证据。
 * @note 位置仅取 position.start；双方各有非零端 offset 时按 offset 闭区间命中，否则按行列。命中须同源，跨度评分只取 offset 差。 等宽候选采用后项；返回事实不保证存在 knownValue。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
/** @brief 为节点语义查询提供已有所有权证据。
 * @note 同节点可并存迁移与错误事实；返回首条不能解释为最严重违规或完整生命周期证明。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
/** @brief 为位置语义查询提供当前位置的所有权证据。
 * @note 位置仅取 position.start；双方各有非零端 offset 时按 offset 闭区间命中，否则按行列。命中须同源，跨度评分只取 offset 差。 violation 仅在跨度相等时优先，其余平局保留前项；返回事实仍须按 qualifier、ID 和见证解释。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
/** @brief 为节点语义查询提供已有 ownership intrinsic 决策证据。
 * @note 缺失事实不会由查询补造；返回记录不额外认证 operation、loanId 或 consuming。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrOwnershipIntrinsicFact *
ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);
/** @brief 为位置语义查询提供当前位置的 ownership intrinsic 决策证据。
 * @note 位置仅取 position.start；双方各有非零端 offset 时按 offset 闭区间命中，否则按行列。命中须同源，跨度评分只取 offset 差。 必须命中整体 range；nameRange 可帮助定位名称，不能绕过整体范围。等宽候选保留前项。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrOwnershipIntrinsicFact *
ZrParser_SemanticFacts_FindOwnershipIntrinsicAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position);
/** @brief 为 compiler 和所有权分析提供当前 segment 的 receiver guard 证据。
 * @note 按同一 AST 快照的 segment identity 使用；返回记录不证明可直接 lowering，compiler 仍须验证 receiver、链边界和模式。
 * @return 返回当前 context 事实数组内的借用指针或 NULL；对应数组增长、Reset/Free 后不得持有，也不能将记录或其 context ID 跨快照使用。
 */
ZR_PARSER_API const SZrReceiverGuardFact *ZrParser_SemanticFacts_FindReceiverGuardByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node);

#endif // ZR_VM_PARSER_SEMANTIC_FACTS_H
