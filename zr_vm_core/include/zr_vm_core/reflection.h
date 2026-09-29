//
// Runtime reflection helpers for canonical typeid/typeof operations.
//

#ifndef ZR_VM_CORE_REFLECTION_H
#define ZR_VM_CORE_REFLECTION_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"

#define ZR_REFLECTION_MODULE_NAME "zr.reflection"

struct SZrState;
struct SZrCallInfo;
struct SZrClosureNative;
struct SZrObject;
struct SZrObjectPrototype;
struct SZrObjectModule;
struct SZrFunction;
struct SZrMetadataRuntime;
struct SZrTypeLayout;
struct SZrString;
struct SZrTypeValue;
struct SZrZrpMetadataFieldDefRow;
struct SZrZrpMetadataGenericParamRow;
struct SZrZrpMetadataGenericParamConstraintRow;
struct SZrZrpMetadataTypeDefRow;
struct SZrZrpMetadataTypeSpecRow;

typedef enum EZrReflectionResolvedTokenKind {
    ZR_REFLECTION_RESOLVED_TOKEN_NONE = 0,
    ZR_REFLECTION_RESOLVED_TOKEN_TYPE = 1,
    ZR_REFLECTION_RESOLVED_TOKEN_METHOD = 2,
    ZR_REFLECTION_RESOLVED_TOKEN_FIELD = 3
} EZrReflectionResolvedTokenKind;

typedef enum EZrReflectionTypeCategory {
    ZR_REFLECTION_TYPE_CATEGORY_ERASED = 0,
    ZR_REFLECTION_TYPE_CATEGORY_CLASS = 1,
    ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS = 2,
    ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS = 3,
    ZR_REFLECTION_TYPE_CATEGORY_STRUCT = 4,
    ZR_REFLECTION_TYPE_CATEGORY_INTERFACE = 5,
    ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS = 6,
    ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT = 7,
    ZR_REFLECTION_TYPE_CATEGORY_ENUM = 8
} EZrReflectionTypeCategory;

typedef struct SZrReflectionTypeIdentity {
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken typeToken;
    TZrUInt64 signatureHash;
    TZrUInt32 metadataGeneration;
    EZrReflectionTypeCategory category;
} SZrReflectionTypeIdentity;

typedef enum EZrReflectionMemberScope {
    ZR_REFLECTION_MEMBER_SCOPE_DECLARED = 0,
    ZR_REFLECTION_MEMBER_SCOPE_INHERITED,
    ZR_REFLECTION_MEMBER_SCOPE_ALL,
} EZrReflectionMemberScope;

typedef enum EZrReflectionMemberAccess {
    ZR_REFLECTION_MEMBER_ACCESS_PUBLIC = 0,
    ZR_REFLECTION_MEMBER_ACCESS_PROTECTED,
    ZR_REFLECTION_MEMBER_ACCESS_PRIVATE,
    ZR_REFLECTION_MEMBER_ACCESS_ALL,
} EZrReflectionMemberAccess;

typedef enum EZrReflectionMemberStorage {
    ZR_REFLECTION_MEMBER_STORAGE_INSTANCE = 0,
    ZR_REFLECTION_MEMBER_STORAGE_STATIC,
    ZR_REFLECTION_MEMBER_STORAGE_ALL,
} EZrReflectionMemberStorage;

typedef enum EZrReflectionMemberKind {
    ZR_REFLECTION_MEMBER_KIND_FIELD = 0,
    ZR_REFLECTION_MEMBER_KIND_PROPERTY,
    ZR_REFLECTION_MEMBER_KIND_METHOD,
    ZR_REFLECTION_MEMBER_KIND_META_METHOD,
    ZR_REFLECTION_MEMBER_KIND_ANY,
} EZrReflectionMemberKind;

typedef enum EZrReflectionQueryStatus {
    ZR_REFLECTION_QUERY_STATUS_OK = 0,
    ZR_REFLECTION_QUERY_STATUS_NOT_FOUND,
    ZR_REFLECTION_QUERY_STATUS_AMBIGUOUS,
    ZR_REFLECTION_QUERY_STATUS_ACCESS_DENIED,
    ZR_REFLECTION_QUERY_STATUS_INVALID_ARGUMENT,
    ZR_REFLECTION_QUERY_STATUS_METADATA_NOT_PRESERVED,
} EZrReflectionQueryStatus;

typedef struct SZrReflectionMemberQuery {
    EZrReflectionMemberScope scope;
    EZrReflectionMemberAccess access;
    EZrReflectionMemberStorage storage;
    TZrBool includeCompilerGenerated;
    TZrBool includeMetaMethods;
    TZrBool hasNonPublicAccessCapability;
} SZrReflectionMemberQuery;

typedef struct SZrReflectionMemberCacheStats {
    TZrUInt64 hitCount;
    TZrUInt64 missCount;
} SZrReflectionMemberCacheStats;

typedef enum EZrReflectionConstructionStatus {
    ZR_REFLECTION_CONSTRUCTION_STATUS_OK = 0,
    ZR_REFLECTION_CONSTRUCTION_STATUS_TYPE_NOT_CONSTRUCTIBLE,
    ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_NOT_FOUND,
    ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_AMBIGUOUS,
    ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_THREW,
    ZR_REFLECTION_CONSTRUCTION_STATUS_INVALID_ARGUMENT,
} EZrReflectionConstructionStatus;

typedef struct SZrReflectionConstructionCacheStats {
    TZrUInt64 hitCount;
    TZrUInt64 missCount;
} SZrReflectionConstructionCacheStats;

typedef struct SZrReflectionResolvedGenericArgument {
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken genericSignatureToken;
    TZrUInt64 genericSignatureHash;
    TZrMetadataToken genericBaseToken;
    const SZrMetadataTokenRecord *genericBaseRecord;
    TZrUInt32 argumentIndex;
    TZrUInt32 argumentNodeKind;
    TZrUInt32 argumentPayload0;
    TZrUInt32 argumentPayload1;
    TZrMetadataToken argumentToken;
    const SZrMetadataTokenRecord *argumentRecord;
} SZrReflectionResolvedGenericArgument;

typedef enum EZrReflectionGenericInstanceRoute {
    ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE = 0,
    ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT = 1,
    ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT = 2
} EZrReflectionGenericInstanceRoute;

typedef enum EZrReflectionGenericTypeArgumentKind {
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE = 0,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE = 1,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN = 2,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY = 3,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE = 4,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP = 5,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE = 6,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION = 7
} EZrReflectionGenericTypeArgumentKind;

typedef enum EZrReflectionOwnershipQualifier {
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_NONE = 0,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_UNIQUE = 1,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_SHARED = 2,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_WEAK = 3,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_BORROWED = 4,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_LOANED = 5
} EZrReflectionOwnershipQualifier;

typedef struct SZrReflectionGenericTypeArgument {
    EZrReflectionGenericTypeArgumentKind kind;
    TZrUInt32 primitiveValueType;
    TZrMetadataToken typeToken;
    TZrUInt32 arrayRank;
    const struct SZrReflectionGenericTypeArgument *elementType;
    EZrReflectionOwnershipQualifier ownershipQualifier;
    TZrUInt32 childCount;
    const struct SZrReflectionGenericTypeArgument *childTypes;
    TZrUInt32 unionValueType;
    TZrUInt32 unionNameStringOffset;
} SZrReflectionGenericTypeArgument;

typedef struct SZrReflectionDynamicGenericTypeInstance {
    EZrReflectionGenericInstanceRoute route;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken genericSignatureToken;
    TZrUInt64 genericSignatureHash;
    TZrMetadataToken genericBaseToken;
    const SZrMetadataTokenRecord *genericBaseRecord;
    TZrUInt32 genericArgumentCount;
    TZrUInt32 genericArgumentListBlobOffset;
    /* Borrowed from ResolveConstructedGenericType() input; null for token-only resolution. */
    const SZrReflectionGenericTypeArgument *requestedArguments;
    TZrUInt32 typeLayoutId;
    const struct SZrTypeLayout *typeLayout;
} SZrReflectionDynamicGenericTypeInstance;

typedef struct SZrReflectionResolvedGenericMethodSpec {
    TZrMetadataToken methodSpecToken;
    const SZrMetadataTokenRecord *methodSpecRecord;
    TZrMetadataToken genericMethodToken;
    const SZrMetadataTokenRecord *genericMethodRecord;
    TZrUInt64 genericSignatureHash;
    TZrUInt32 genericArgumentCount;
    TZrUInt32 genericArgumentListBlobOffset;
    /* Borrowed from ResolveConstructedGenericMethod() input. */
    const SZrReflectionGenericTypeArgument *requestedArguments;
} SZrReflectionResolvedGenericMethodSpec;

typedef struct SZrReflectionResolvedGenericParameter {
    TZrMetadataToken ownerToken;
    const SZrMetadataTokenRecord *ownerRecord;
    const struct SZrZrpMetadataGenericParamRow *genericParamRow;
    TZrUInt32 genericParamIndex;
    TZrUInt32 parameterIndex;
    TZrUInt32 nameStringOffset;
    TZrUInt32 firstConstraintIndex;
    TZrUInt32 constraintCount;
    TZrUInt32 flags;
} SZrReflectionResolvedGenericParameter;

typedef struct SZrReflectionResolvedGenericParameterConstraint {
    SZrReflectionResolvedGenericParameter genericParameter;
    const struct SZrZrpMetadataGenericParamConstraintRow *constraintRow;
    TZrUInt32 constraintIndex;
    TZrMetadataToken constraintTypeToken;
    const SZrMetadataTokenRecord *constraintTypeRecord;
    const TZrByte *signatureBlobData;
    TZrSize signatureBlobByteLength;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrReflectionResolvedGenericParameterConstraint;

typedef struct SZrReflectionResolvedMethodSpecGenericArgument {
    TZrMetadataToken methodSpecToken;
    TZrMetadataToken methodToken;
    const SZrMetadataTokenRecord *methodRecord;
    TZrUInt64 genericSignatureHash;
    TZrUInt32 argumentIndex;
    TZrUInt32 argumentNodeKind;
    TZrUInt32 argumentPayload0;
    TZrUInt32 argumentPayload1;
    TZrMetadataToken argumentToken;
    const SZrMetadataTokenRecord *argumentRecord;
} SZrReflectionResolvedMethodSpecGenericArgument;

typedef struct SZrReflectionResolvedToken {
    EZrReflectionResolvedTokenKind kind;
    TZrMetadataToken token;
    const SZrMetadataTokenRecord *record;
    TZrMetadataToken methodToken;
    const SZrMetadataTokenRecord *methodRecord;
    TZrMetadataToken methodSignatureToken;
    const SZrMetadataTokenRecord *methodSignatureRecord;
    TZrUInt64 methodSignatureHash;
    TZrUInt32 methodFunctionIndex;
    const SZrAotMethodInfo *methodInfo;
    FZrAotEntryThunk methodFunctionPointer;
    FZrAotReflectionInvoker methodInvoker;
    const struct SZrZrpMetadataTypeDefRow *typeDefRow;
    const struct SZrZrpMetadataTypeSpecRow *typeSpecRow;
    TZrMetadataToken genericSignatureToken;
    TZrUInt64 genericSignatureHash;
    TZrMetadataToken genericBaseToken;
    const SZrMetadataTokenRecord *genericBaseRecord;
    TZrUInt32 genericArgumentCount;
    TZrUInt32 genericArgumentListBlobOffset;
    TZrUInt32 typeLayoutId;
    TZrUInt32 cTypeId;
    const struct SZrTypeLayout *typeLayout;
    const struct SZrZrpMetadataFieldDefRow *fieldDefRow;
    TZrMetadataToken ownerTypeToken;
    const SZrMetadataTokenRecord *ownerTypeRecord;
    const struct SZrZrpMetadataTypeDefRow *ownerTypeDefRow;
    TZrMetadataToken fieldTypeToken;
    const SZrMetadataTokenRecord *fieldTypeRecord;
    TZrUInt32 byteOffset;
    TZrUInt32 fieldTypeLayoutId;
    TZrUInt32 ownerTypeLayoutId;
    const struct SZrTypeLayout *fieldTypeLayout;
    const struct SZrTypeLayout *ownerTypeLayout;
} SZrReflectionResolvedToken;

ZR_CORE_API TZrInt64 ZrCore_Reflection_TypeOfNativeEntry(struct SZrState *state);

ZR_CORE_API TZrBool ZrCore_Reflection_TypeOfValue(struct SZrState *state,
                                                  const struct SZrTypeValue *targetValue,
                                                  struct SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Reflection_IsReflectionObject(struct SZrState *state, struct SZrObject *object);

ZR_CORE_API struct SZrString *ZrCore_Reflection_FormatObject(struct SZrState *state, struct SZrObject *object);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildTypeLiteralObject(struct SZrState *state,
                                                                       struct SZrString *typeName);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildTypeIdObject(
        struct SZrState *state,
        struct SZrString *canonicalTypeName,
        const SZrReflectionTypeIdentity *identity);

ZR_CORE_API TZrBool ZrCore_Reflection_IsTypeIdObject(
        struct SZrState *state,
        struct SZrObject *object);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadTypeIdObject(
        struct SZrState *state,
        struct SZrObject *object,
        SZrReflectionTypeIdentity *outIdentity,
        struct SZrString **outCanonicalTypeName);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveTypeIdObject(
        struct SZrState *state,
        struct SZrObject *typeIdObject);

ZR_CORE_API void ZrCore_Reflection_MemberQueryInitDefault(
        SZrReflectionMemberQuery *query);

ZR_CORE_API TZrBool ZrCore_Reflection_QueryMembers(
        struct SZrState *state,
        struct SZrObject *typeDescriptor,
        EZrReflectionMemberKind kind,
        const SZrReflectionMemberQuery *query,
        struct SZrObject **outMembers,
        EZrReflectionQueryStatus *outStatus);

ZR_CORE_API TZrBool ZrCore_Reflection_GetMember(
        struct SZrState *state,
        struct SZrObject *typeDescriptor,
        const struct SZrString *name,
        EZrReflectionMemberKind kind,
        const struct SZrObject *const *parameterTypeIds,
        TZrUInt32 parameterTypeCount,
        const SZrReflectionMemberQuery *query,
        struct SZrObject **outMember,
        EZrReflectionQueryStatus *outStatus);

ZR_CORE_API void ZrCore_Reflection_DebugResetMemberCacheStats(void);

ZR_CORE_API SZrReflectionMemberCacheStats
ZrCore_Reflection_DebugGetMemberCacheStats(void);

ZR_CORE_API TZrBool ZrCore_Reflection_RequireConstructible(
        struct SZrState *state,
        struct SZrObject *typeDescriptor,
        EZrReflectionConstructionStatus *outStatus);

ZR_CORE_API TZrBool ZrCore_Reflection_CreateInstance(
        struct SZrState *state,
        struct SZrObject *typeDescriptor,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result,
        EZrReflectionConstructionStatus *outStatus);

ZR_CORE_API void ZrCore_Reflection_DebugResetConstructionCacheStats(void);

ZR_CORE_API SZrReflectionConstructionCacheStats
ZrCore_Reflection_DebugGetConstructionCacheStats(void);

ZR_CORE_API TZrBool ZrCore_Reflection_BindTypeIdDescriptor(
        struct SZrState *state,
        struct SZrObject *typeIdObject,
        struct SZrObject *descriptor);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildCallableTypeLiteralObject(
        struct SZrState *state,
        struct SZrString *callableName,
        struct SZrString *returnTypeName,
        struct SZrString *const *parameterNames,
        struct SZrString *const *parameterTypeNames,
        struct SZrString *const *parameterModeNames,
        TZrUInt32 parameterCount,
        struct SZrString *const *genericParameterNames,
        TZrUInt32 genericParameterCount,
        TZrBool isVariadic);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildFieldInfoTokenObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken);

/**
 * @brief 按 FieldDef token 读取顶层字段，统一返回 VM 值或可借用的内联 struct/union 地址。
 * @pre 非 NULL 的 state/runtime/storage 在调用期间有效，非 NULL 的 outValue 可写；若目标为 VALUE_SLOT，该槽已初始化且对齐。
 * @return outValue 为空或字段无效时返回 false；非空输出在失败时置 null；成功时复制 VM 值、解码 POD 或返回借用 NativePointer。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        struct SZrTypeValue *outValue);

/** @brief FieldInfo 对象形式的顶层读取适配器；先取其 runtime/token 身份，再复用 Token 版字段和存储校验。
 * @pre 非 NULL 的 state/FieldInfo/outValue 有效且输出可写，FieldInfo 的 runtime 与 storage 有效；若目标为 VALUE_SLOT，该槽已初始化且对齐。
 * @return outValue 为 NULL 或身份/字段读取失败时返回 false；失败时将非空 outValue 置 null。
 * @note VALUE_SLOT 和 primitive 返回独立 VM 值；内联 struct/union 返回指向 backing storage 的借用 NativePointer，调用方须保持该存储有效。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        struct SZrTypeValue *outValue);

/** @brief 单层嵌套读取的 FieldInfo 适配器；身份提取成功后转发给 token 版，不持有结果视图。
 * @pre 非 NULL 的 state/FieldInfo/outValue 有效且输出可写，FieldInfo 的 runtime 与 storage 有效；若命中 VALUE_SLOT，该槽已初始化且对齐。
 * @return outValue 为 NULL 或身份/字段读取失败时返回 false；失败时将非空 outValue 置 null。
 * TODO: 沿用 Token 入口尚未核对的 union activeTag；核查单层非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        struct SZrTypeValue *outValue);

/** @brief 嵌套路径读取的 FieldInfo 适配器；身份和路径约束最终由 token 版与 nested helper 共同执行。
 * @pre 非 NULL 的 state/FieldInfo/路径数组/outValue 有效且输出可写，runtime 与 storage 有效；若终端为 VALUE_SLOT，该槽已初始化且对齐。
 * @return outValue 为 NULL 或身份/路径读取失败时返回 false；失败时将非空 outValue 置 null；成功结果按 VM copy 规则取得。
 * TODO: 单段终端及多段中间/终端 union 字段未核对 activeTag；核查非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        struct SZrTypeValue *outValue);

/** @brief primitive 嵌套路径读取的 FieldInfo 适配器，恢复身份后转发到 token 版。
 * @pre 非 NULL 的 state/FieldInfo/路径数组/outValue 在调用期间有效且输出可写，runtime 与 backing storage 保持有效。
 * @return outValue 为 NULL 或身份/叶子解码失败时返回 false；失败时将非空 outValue 置 null。
 * TODO: primitive 终端及中间 union 字段未核对 activeTag；核查非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        struct SZrTypeValue *outValue);

/** @brief 嵌套路径写入的 FieldInfo 适配器；从对象恢复身份后委托 token 版执行目标定位与值替换。
 * @pre 非空的 state、FieldInfo 对象、路径数组和输入 value 在调用期间有效，runtime 与可写 backing storage 保持有效；若终端为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 身份、路径或目标字段不兼容时返回 false；成功时只替换一个终端值槽。
 * TODO: 单段终端及多段中间/终端 union 字段未核对 activeTag；核查写入时拒绝或更新 tag 的契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const struct SZrTypeValue *value);

/** @brief primitive 嵌套路径写入的 FieldInfo 适配器，身份读取成功后转发到 token 版。
 * @pre 非空的 state、FieldInfo 对象、路径数组和输入 value 在调用期间有效，runtime 与目标 storage 保持有效。
 * @return 身份、路径或 primitive 编码失败时返回 false。
 * TODO: primitive 终端及中间 union 字段未核对 activeTag；核查写入时拒绝或更新 tag 的契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const struct SZrTypeValue *value);

/** @brief 单层嵌套写入的 FieldInfo 适配器；从对象恢复身份后复用 token 版更新子字段。
 * @pre 非空的 state、FieldInfo 对象和输入 value 在调用期间有效，runtime 与可写 backing storage 保持有效；若目标为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 身份、索引或目标子字段不兼容时返回 false。
 * TODO: 沿用 Token 入口尚未核对的 union activeTag；核查单层写入时拒绝或更新 tag 的契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const struct SZrTypeValue *value);

/** @brief FieldInfo 对象形式的顶层写入适配器；成功取回身份后转发给 token 版。
 * @pre 非空的 state、FieldInfo 对象和输入 value 在调用期间有效，runtime 与可写 storage 保持有效；若目标为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 身份、字段范围或目标表示与输入不兼容时返回 false。
 * @note 聚合源借用 NativePointer，实际长度由调用方保证；布局级 copy 失败可能已修改前缀字段，不承诺整块回滚。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const struct SZrTypeValue *value);

/**
 * @brief 按 FieldDef token 写入顶层字段，区分 VM 值槽、原始 primitive POD 与可复制的内联聚合。
 * @pre state、runtime、可写 storage、value 均有效；若目标为 VALUE_SLOT，该槽须已初始化并对齐；聚合 NativePointer 源须覆盖目标布局。
 * @return token/layout 无法解析或字段超出给定长度时返回 false；VM 值槽遵循 Value_Copy 的替换/ownership 规则。
 * primitive 写入拒绝类型或范围不匹配；布局 copy 失败不承诺整块回滚。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const struct SZrTypeValue *value);

/** @brief 读取内联聚合中由索引选中的一层子字段，路径遍历与 VM 值所有权处理委托给 nested helper。
 * @pre 非 NULL 的 state/runtime/storage/outValue 在调用期间有效且输出可写；若命中 VALUE_SLOT，该槽已初始化且对齐。
 * @return outValue 为 NULL、外层不可借用、索引越界或子字段不支持时返回 false；失败时将非空 outValue 置 null，成功时按 VM 值复制。
 * TODO: 单层读取未核对 union activeTag；下一步核查 tag 存储及非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        struct SZrTypeValue *outValue);

/** @brief 沿索引路径读取内联聚合中的终端字段，保留普通 nested helper 对终端 VALUE_SLOT 的 VM copy 语义。
 * @pre 非 NULL 的 state/runtime/storage/路径数组/outValue 在调用期间有效且输出可写；若命中 VALUE_SLOT，该槽已初始化且对齐。
 * @return outValue 为 NULL、空路径、中间布局无法解析或终端不支持时返回 false；失败时将非空 outValue 置 null，有效终端值通过 VM copy 返回。
 * TODO: 中间及终端 union 字段未核对 activeTag；核查多段与单段路径的非活动成员读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        struct SZrTypeValue *outValue);

/** @brief 沿路径读取原始 primitive POD 叶子，并要求调用方给出的 ValueType 与终端布局一致。
 * @pre 非 NULL 的 state/runtime/路径数组/outValue 在调用期间有效且输出可写，backing storage 保持有效。
 * @return outValue 为 NULL、路径无效、外层不可借用、primitive 类型或终端表示不匹配时返回 false；失败时将非空 outValue 置 null，成功时返回独立 VM 值。
 * TODO: primitive 终端及中间 union 字段未核对 activeTag；核查非活动成员的读取契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        struct SZrTypeValue *outValue);

/** @brief 将 VM 值写到内联聚合路径的终端值槽；字段级替换的 copy/drop 交给 nested helper。
 * @pre 非空的 state、runtime、可写 backing storage、路径数组和输入 value 在调用期间有效；若目标为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 空路径、不可解析字段或不兼容终端返回 false；成功时只按 VM Value_Copy 更新一个终端槽。
 * TODO: 中间及终端 union 字段未核对 activeTag；核查写入时拒绝或更新 tag 的规则。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const struct SZrTypeValue *value);

/** @brief 将 VM 值按显式 primitive 类型编码到嵌套路径的原始 POD 叶子。
 * @pre 非空的 state、runtime、可写 backing storage、路径数组和输入 value 在调用期间有效。
 * @return 路径为空、primitive 类型无效、布局不支持或值类型/尺寸/范围不匹配时返回 false。
 * TODO: primitive 终端及中间 union 字段未核对 activeTag；核查写入时拒绝或更新 tag 的规则。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const struct SZrTypeValue *value);

/** @brief 将 VM 值替换到内联聚合中索引选中的单层子字段，VM copy 负责旧值 ownership/drop。
 * @pre 非空的 state、runtime、可写 backing storage 和输入 value 在调用期间有效；若目标为 VALUE_SLOT，该槽已初始化且满足对齐。
 * @return 外层布局或子索引不匹配、目标不是值槽或尺寸不足时返回 false。
 * TODO: 单层 union 子字段写入未核对 activeTag；核查写入时拒绝或更新 tag 的契约。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const struct SZrTypeValue *value);

/**
 * @brief 把受支持的 metadata token 转成当前运行时的反射视图。
 * @pre 非 NULL 的 runtime 须指向有效元数据运行时，非 NULL 的 outResolved 可写；无效 token 可作为失败查询传入。
 * @return runtime/outResolved 为 NULL、token 无效或不支持时返回 false，并将非空输出清零；解析成功时返回 true。
 * @note 记录、行、布局和 AOT 绑定指针由 runtime 背后的模块数据借用；保持模块元数据与代码注册有效。
 *       方法缺 signature record 时仍可能解析成功，signature 字段保持零值（TODO: 核查元数据完整性）；缺 AOT 绑定时也可解析，但不能经 invoker 执行。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ResolveToken(struct SZrMetadataRuntime *runtime,
                                                   TZrMetadataToken token,
                                                   SZrReflectionResolvedToken *outResolved);
ZR_CORE_API EZrArtifactStatus ZrCore_Reflection_ResolveArtifactType(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic);

/**
 * @brief 通过方法 token 的 AOT 绑定直接调用 value-only invoker。
 * @pre 非空的 state/runtime 在调用期有效；非空 outReturn 可写，self 与 args 按对应 invoker 的 ABI 提供；本接口不接收参数数量。
 * @return state/runtime/outReturn 为 NULL、methodToken 为零或绑定/传递模式无效时返回 false；invoker 已被调用时返回 true。
 * @note 参数与接收者按原指针转交，不在此复制、装箱或 pin GC 值；调用方须提供匹配签名的参数存储。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_InvokeMethodToken(struct SZrState *state,
                                                        struct SZrMetadataRuntime *runtime,
                                                        TZrMetadataToken methodToken,
                                                        struct SZrTypeValue *self,
                                                        struct SZrTypeValue *args,
                                                        struct SZrTypeValue *outReturn);

/**
 * @brief 校验固定参数的数量与值标签后，通过 AOT 绑定调用方法。
 * @pre 非空的 state/runtime 在调用期有效；非空 outReturn 可写；self 遵循所选 receiver ABI，非空 args 覆盖至少 argCount 个可读值。
 * @return state/runtime/outReturn 为 NULL、methodToken 为零或绑定/签名/实参校验失败时返回 false；返回标签不匹配也返回 false，但 invoker 已运行。
 * @note varargs 仅校验固定参数前缀；参数原样传递且不 pin GC 值。void 方法调用后将 outReturn 置 null。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_InvokeMethodTokenWithArgCount(struct SZrState *state,
                                                                    struct SZrMetadataRuntime *runtime,
                                                                    TZrMetadataToken methodToken,
                                                                    struct SZrTypeValue *self,
                                                                    struct SZrTypeValue *args,
                                                                    TZrUInt32 argCount,
                                                                    struct SZrTypeValue *outReturn);

/**
 * @brief 读取 TypeSpec 中指定下标的泛型实参签名节点。
 * @pre 非 NULL 的 runtime 在调用期间有效，非 NULL 的 outArgument 可写。
 * @return runtime/outArgument 为 NULL、token 或索引无效时返回 false，非空输出清零；成功时填充节点种类、载荷和 token 身份。
 * @note argumentRecord 与 genericBaseRecord 均借用 runtime 背后的模块元数据；本函数不创建 GC 对象。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ResolveTypeSpecGenericArgument(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex,
        SZrReflectionResolvedGenericArgument *outArgument);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrReflectionDynamicGenericTypeInstance *outInstance);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
        struct SZrMetadataRuntime *requesterRuntime,
        TZrMetadataToken requesterTypeSpecToken,
        struct SZrMetadataRuntime *providerRuntime,
        SZrReflectionDynamicGenericTypeInstance *outInstance);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveConstructedGenericType(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionDynamicGenericTypeInstance *outInstance);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveConstructedGenericMethod(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionResolvedGenericMethodSpec *outMethodSpec);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildConstructedGenericMethodObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionResolvedGenericMethodSpec *methodSpec);

ZR_CORE_API TZrBool ZrCore_Reflection_RevalidateDynamicGenericTypeInstance(
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        SZrReflectionDynamicGenericTypeInstance *outResolved);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_MakeGenericTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_MakeGenericMethodObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount);

/**
 * @brief 用对象化泛型实参和方法定义 carrier 构造 MethodSpec 反射对象。
 * @pre 非空的 state、runtime 和两个对象在调用期间有效；泛型实参数组及递归节点在两遍解码期间保持稳定。
 * @return 空参数、定义身份或实参 schema 不符、元数据/资源获取失败时返回 NULL；成功对象须由调用方建立 GC 根。
 * @note runtime 裸指针只用于核对定义身份，元数据访问使用显式 runtime。
 * BUG: 同域多个 RUNNING mutator 并发 pin/unpin 时，无锁 ignored-root registry 可能丢根或破坏登记索引。
 * TODO: pin 不冻结对象图；核查 native 调用方是否保证两遍解码期间无并发改写，或需锁/快照。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_MakeGenericMethodFromObjects(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrObject *genericMethodDefinition,
        struct SZrObject *genericArguments);

ZR_CORE_API TZrInt64 ZrCore_Reflection_MakeGenericMethodNativeEntry(struct SZrState *state);

ZR_CORE_API struct SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime);

/**
 * @brief 为给定 MetadataRuntime 新建独立的反射服务 module。
 * @pre state、runtime、runtime->module 非空且有效；state 与 runtime->module 同属一个 GC domain，module 反向持有 runtime。
 * @return 四个导出校验通过时返回 READY module；普通失败返回空，OOM 可非局部抛出。
 * @note 成功后会 pin runtime module；返回的服务 module 仍须由调用方在下次 GC 前建立 root。
 * TODO: 公开入口只核 runtime 与 module 回指；需确认是否应在入口拒绝违反同域前置条件的调用。
 */
ZR_CORE_API struct SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntime(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime);

ZR_CORE_API struct SZrObjectModule *ZrCore_Reflection_GetOrCreateModuleForRuntime(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildGenericMethodDefinitionObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildMethodSpecGenericContextObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken);

/**
 * @brief 将 MethodSpec 构造出的泛型方法上下文绑定到活动的解释器 CallInfo。
 * @pre 非空的 state、runtime 和 callInfo 在调用期间有效；非空 callInfo 属于当前活动帧，runtime 在上下文使用期间保持有效。
 * @return MethodSpec 有效且 callInfo 为非 native 帧、上下文构造成功时返回 true；空参数或其他失败返回 false，旧方法上下文保持清空。
 * @note 上下文对象只保存 metadataRuntime 的 native 指针；使用期间 runtime 必须仍然有效。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrCallInfo *callInfo,
        TZrMetadataToken methodSpecToken);

/**
 * @brief 读取非 native CallInfo 已绑定的 MethodSpec 泛型上下文对象。
 * @pre 非空的 state/callInfo 在读取期间有效；callInfo 属于当前 GC 扫描的活动帧。
 * @return 活动帧保存有效反射对象时返回它；native 帧、空槽或无效对象返回 NULL。
 * @note GC 根随 CallInfo 活跃期存在；结果要跨帧保留时，调用方须另行建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
        struct SZrState *state,
        struct SZrCallInfo *callInfo);

/**
 * @brief 从活动 CallInfo 的 MethodSpec 上下文解析指定方法 owner 的泛型实参。
 * @pre 非空的 state、runtime 和 callInfo 在调用期间有效；非空 callInfo 保持为 GC 扫描的活动帧。
 * @return 空参数返回 NULL；MethodSpec 上下文属于 runtime 与 genericMethodToken 且 parameterIndex 有对应实参时返回对象，否则返回 NULL。
 * @note 返回对象由 CallInfo 的方法上下文图持有；若需跨帧保留，调用方须另行建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrCallInfo *callInfo,
        TZrMetadataToken genericMethodToken,
        TZrUInt32 parameterIndex);

/**
 * @brief 以 MethodSpec 的方法泛型上下文调用调用方已解析出的解释器函数。
 * @pre 非空的 state、runtime、function 和 result 在调用期间有效；function 须由调用方保证对应 methodSpecToken 的底层方法。
 *      非空 arguments 须覆盖 argumentCount 个值，其中 GC 值在上下文构造期间保持有根。
 * @return state/runtime/function/result 为 NULL、非零 argumentCount 却无 arguments 或校验/调用失败时返回 false；成功调用返回 true。
 * @note argumentCount 为零时 arguments 可为 NULL；入口先清非空 result。构造 MethodSpec 上下文时，调用方须保活 native 实参中的 GC 值。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        struct SZrFunction *function,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result);

/**
 * @brief 从 MethodSpec 解析底层解释器函数，并用对应的方法泛型上下文同步调用。
 * @pre 非空的 state、runtime 和 result 在调用期间有效；非空 arguments 覆盖 argumentCount 个可读值，其中 GC 值在上下文构造可能触发的 GC 期间保持有根。
 * @return state/runtime/result 为 NULL、非零 argumentCount 却无 arguments、MethodSpec/函数形状无效或调用失败时返回 false；成功调用返回 true。
 * @note argumentCount 为零时 arguments 可为 NULL；入口先清非空 result。此入口解析 function；直接调用 ResolvedFunction 版须自行保证方法身份。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result);

/**
 * @brief 用经过 runtime 重验且走 interpreter-deopt 路由的泛型 carrier 创建语言对象。
 * @pre 传入的非空指针在调用期间有效；instance 借用的请求树在重验期间保持可读。
 * @return 空参数、prototype 非 class/struct、carrier 重验失败或分配失败时返回 NULL；成功返回带隐藏上下文的实例。
 * @note struct prototype 保留 struct 身份；上下文对象由新实例强引用，但 runtime 指针不延长其生命周期。返回前会撤销临时 pin，调用方须为实例建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_NewInterpreterGenericInstanceObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        struct SZrObjectPrototype *openPrototype);

/**
 * @brief 从支持的对象或 struct 实例中取出其隐藏的反射泛型类型对象。
 * @pre 非空的 state 和 instanceObject 在读取期间有效。
 * @return 隐藏字段保存反射对象时返回该对象；空参数、普通对象、缺失字段或无效值返回 NULL。
 * @note 返回对象由 instanceObject 持有；若调用方要跨越后续 GC 保留它，须另行建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(
        struct SZrState *state,
        struct SZrObject *instanceObject);

/**
 * @brief 按实例的泛型 owner 和参数索引返回已经构造的实参反射对象。
 * @pre 非空的 state、runtime 和 instanceObject 在调用期间有效。
 * @return 空参数返回 NULL；隐藏上下文属于 runtime 和 genericOwnerToken 且数组形状及 parameterIndex 匹配时返回实参对象，否则返回 NULL。
 * @note 返回对象由实例的泛型参数数组持有；若要在实例之外跨 GC 保留，调用方须另行建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex);

/**
 * @brief 将实例的类型泛型上下文绑定到一个活动的解释器 CallInfo。
 * @pre 非空的 state、callInfo 和 instanceObject 在调用期间有效；非空 callInfo 属于当前活动帧。
 * @return 能提取实例上下文且 callInfo 为非 native 帧时返回 true；空参数或上下文无效时返回 false，旧类型上下文保持清空。
 * @note 活动 CallInfo 的上下文槽由 GC 扫描；帧退出或复用后不再替调用方持有对象。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo(
        struct SZrState *state,
        struct SZrCallInfo *callInfo,
        struct SZrObject *instanceObject);

/**
 * @brief 读取非 native CallInfo 已绑定的类型泛型上下文。
 * @pre 非空的 state/callInfo 在读取期间有效；callInfo 属于当前 GC 扫描的活动帧。
 * @return 活动帧保存了反射对象时返回该对象；native 帧、空槽或无效对象返回 NULL。
 * @note 返回引用的 GC 根随 CallInfo 活跃期存在；要跨帧保留需由调用方另行建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
        struct SZrState *state,
        struct SZrCallInfo *callInfo);

/**
 * @brief 从活动 CallInfo 的类型上下文解析指定 owner 的泛型实参。
 * @pre 非空的 state、runtime 和 callInfo 在调用期间有效；非空 callInfo 保持为 GC 扫描的活动帧。
 * @return 空参数返回 NULL；CallInfo 类型上下文属于 runtime 与 genericOwnerToken 且 parameterIndex 有对应实参时返回对象，否则返回 NULL。
 * @note 返回对象由 CallInfo 的泛型上下文图持有；跨帧或后续 GC 保留需另行建立根。
 */
ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrCallInfo *callInfo,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex);

/**
 * @brief 将解释器方法作为实例方法调用，并为方法体安装该闭合泛型实例的类型上下文。
 * @pre 非空的 state、runtime、instanceObject、function 和 result 在调用期间有效；function 须由调用方保证属于 genericOwnerToken 对应的方法；
 *      非空 arguments 须覆盖 argumentCount 个值，其中 GC 值在反射上下文读取期间保持有根。
 * @return 必需指针为 NULL、非零 argumentCount 却无 arguments、owner/函数形状/参数数不匹配或调用失败时返回 false；成功调用返回 true。
 * @note instanceObject 单独作为 receiver，arguments 不含它；零实参可传 NULL。入口先清非空 result；字段键读取可能触发 GC，调用方须保活实参中的 GC 值。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        struct SZrFunction *function,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result);

/**
 * @brief 返回泛型 owner 中指定形参的元数据行及其约束索引范围。
 * @pre 非 NULL 的 runtime 在调用期间有效，非 NULL 的 outParameter 可写。
 * @return runtime/outParameter 为 NULL、owner token 或索引无效时返回 false，非空输出清零；找到参数时返回 true。
 * @note ownerRecord 与 genericParamRow 是 runtime 元数据的借用指针；本函数不分配 GC 对象。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ResolveGenericParameter(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrReflectionResolvedGenericParameter *outParameter);

/**
 * @brief 返回泛型形参的一条约束及其类型 token 和签名 blob 片段。
 * @pre 非 NULL 的 runtime 在调用期间有效，非 NULL 的 outConstraint 可写。
 * @return runtime/outConstraint 为 NULL、owner token 或索引无效时返回 false，非空输出清零；找到约束时返回 true。
 * @note 行、记录和 signatureBlobData 均借用 runtime 元数据；使用期间须保持其 backing buffer 有效。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ResolveGenericParameterConstraint(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        TZrUInt32 constraintIndex,
        SZrReflectionResolvedGenericParameterConstraint *outConstraint);

/**
 * @brief 返回 MethodSpec 中指定下标的泛型实参节点和底层方法身份。
 * @pre 非 NULL 的 runtime 在调用期间有效，非 NULL 的 outArgument 可写。
 * @return runtime/outArgument 为 NULL、MethodSpec token 或索引无效时返回 false，非空输出清零；找到实参时返回 true。
 * @note methodRecord 与 argumentRecord 是 runtime 元数据的借用指针，不会由此接口延长其生命周期。
 */
ZR_CORE_API TZrBool ZrCore_Reflection_ResolveMethodSpecGenericArgument(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        TZrUInt32 argumentIndex,
        SZrReflectionResolvedMethodSpecGenericArgument *outArgument);

ZR_CORE_API void ZrCore_Reflection_AttachModuleRuntimeMetadata(struct SZrState *state,
                                                               struct SZrObjectModule *module,
                                                               struct SZrFunction *entryFunction);

ZR_CORE_API void ZrCore_Reflection_AttachPrototypeRuntimeMetadata(struct SZrState *state,
                                                                  struct SZrObjectPrototype *prototype,
                                                                  struct SZrObjectModule *module,
                                                                  struct SZrFunction *entryFunction);

#endif // ZR_VM_CORE_REFLECTION_H
