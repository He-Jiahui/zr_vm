#include "zr_vm_core/function.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

/**
 * @brief 判断签名记录是否指向函数自有的有效 blob 区间。
 *
 * @note 跨模块匹配器只比较当前函数持有的元数据；此处先确认区间完整，避免
 * 将损坏或缺失的签名数据当作可比较的身份依据。返回 false 表示调用者应拒绝匹配。
 */
static TZrBool metadata_record_blob_is_valid(const SZrFunction *function,
                                             const SZrMetadataTokenRecord *record) {
    return function != ZR_NULL &&
           record != ZR_NULL &&
           function->signatureBlobHeap != ZR_NULL &&
           record->signatureBlobLength > 0u &&
           record->signatureBlobOffset < function->signatureBlobHeapLength &&
           record->signatureBlobLength <= function->signatureBlobHeapLength - record->signatureBlobOffset;
}

/**
 * @brief 比较两个元数据记录的规范签名字节。
 *
 * @note TypeSpec 与 TypeDef 绑定先用 hash 快速筛选，再用完整 blob 字节确认身份，
 * 因而不能仅凭 hash 相等建立跨模块绑定。
 */
static TZrBool metadata_record_signature_blobs_equal(const SZrFunction *leftFunction,
                                                     const SZrMetadataTokenRecord *leftRecord,
                                                     const SZrFunction *rightFunction,
                                                     const SZrMetadataTokenRecord *rightRecord) {
    if (!metadata_record_blob_is_valid(leftFunction, leftRecord) ||
        !metadata_record_blob_is_valid(rightFunction, rightRecord) ||
        leftRecord->signatureBlobLength != rightRecord->signatureBlobLength) {
        return ZR_FALSE;
    }

    return ZrCore_Memory_RawCompare(leftFunction->signatureBlobHeap + leftRecord->signatureBlobOffset,
                                    rightFunction->signatureBlobHeap + rightRecord->signatureBlobOffset,
                                    leftRecord->signatureBlobLength) == 0
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/** @brief 在单个函数的主 token 记录流中按 token 查找实体；零 token 不表示实体。 */
static const SZrMetadataTokenRecord *metadata_find_token_record(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordLength,
        TZrMetadataToken token) {
    if (records == ZR_NULL || token == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < recordLength; index++) {
        if (records[index].token == token) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

/**
 * @brief 查找实体对应的配对 SIGNATURE 记录。
 *
 * @note 仅接受实体 relatedToken 指向 SIGNATURE，且签名记录的 relatedToken 与 ownerToken
 * 均回指实体的双向关系；绑定结果因此不会依赖仅同 hash 的旁路记录。
 */
static const SZrMetadataTokenRecord *metadata_find_signature_record(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordLength,
        TZrMetadataToken entityToken) {
    const SZrMetadataTokenRecord *entityRecord;

    if (records == ZR_NULL || entityToken == 0u) {
        return ZR_NULL;
    }

    entityRecord = metadata_find_token_record(records, recordLength, entityToken);
    if (entityRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(entityRecord->relatedToken) != ZR_METADATA_TABLE_SIGNATURE) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < recordLength; index++) {
        const SZrMetadataTokenRecord *record = &records[index];

        if (record->token == entityRecord->relatedToken &&
            ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_SIGNATURE &&
            record->relatedToken == entityToken &&
            record->ownerToken == entityToken) {
            return record;
        }
    }

    return ZR_NULL;
}

/** @brief 为一个 TypeSpec 实体取得本函数记录流中的配对签名记录。 */
static const SZrMetadataTokenRecord *function_find_type_spec_signature_record(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeSpecRecord) {
    return typeSpecRecord == ZR_NULL
                   ? ZR_NULL
                   : metadata_find_signature_record(function == ZR_NULL ? ZR_NULL : function->metadataTokenRecords,
                                                    function == ZR_NULL ? 0u : function->metadataTokenRecordLength,
                                                    typeSpecRecord->token);
}

/**
 * @brief 在 provider 函数中寻找与 caller TypeSpec 规范签名相同的记录。
 *
 * @note 匹配受限于 TypeSpec table、签名 hash 和 blob 字节完全相同；此阶段不写 sidecar，
 * 由上层状态机完成 union 定义与布局校验后再记录绑定。
 */
static const SZrMetadataTokenRecord *function_find_matching_type_spec_record(
        const SZrFunction *providerFunction,
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeSpecRecord) {
    if (providerFunction == ZR_NULL ||
        callerFunction == ZR_NULL ||
        callerTypeSpecRecord == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < providerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *providerRecord = &providerFunction->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(providerRecord->token) != ZR_METADATA_TABLE_TYPE_SPEC ||
            providerRecord->signatureHash != callerTypeSpecRecord->signatureHash) {
            continue;
        }
        if (metadata_record_signature_blobs_equal(callerFunction,
                                                  callerTypeSpecRecord,
                                                  providerFunction,
                                                  providerRecord)) {
            return providerRecord;
        }
    }

    return ZR_NULL;
}

/** @brief 按 string heap 索引取得函数持有的字符串对象；零索引及缺失项均失败。 */
static SZrString *function_metadata_string_heap_lookup(const SZrFunction *function,
                                                       TZrUInt32 stringIndex) {
    if (function == ZR_NULL ||
        function->metadataStringHeap == ZR_NULL ||
        stringIndex == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataStringHeapLength; index++) {
        if (function->metadataStringHeap[index].stringIndex == stringIndex) {
            return function->metadataStringHeap[index].value;
        }
    }

    return ZR_NULL;
}

/**
 * @brief 从签名 blob 读取 little-endian string heap 索引并解析为函数内字符串。
 *
 * @note 返回字符串是 metadata heap 中的借用对象，不转移所有权；调用方只在当前函数
 * 元数据仍存活时使用它进行名字比较。
 */
static TZrBool function_metadata_read_string_ref(const SZrFunction *function,
                                                 const TZrByte *blob,
                                                 TZrSize blobLength,
                                                 TZrSize *cursor,
                                                 SZrString **outString) {
    TZrUInt32 stringIndex;

    if (outString != ZR_NULL) {
        *outString = ZR_NULL;
    }
    if (blob == ZR_NULL || cursor == ZR_NULL || outString == ZR_NULL ||
        *cursor + sizeof(TZrUInt32) > blobLength) {
        return ZR_FALSE;
    }

    stringIndex = ((TZrUInt32)blob[*cursor]) |
                  ((TZrUInt32)blob[*cursor + 1u] << 8) |
                  ((TZrUInt32)blob[*cursor + 2u] << 16) |
                  ((TZrUInt32)blob[*cursor + 3u] << 24);
    *cursor += sizeof(TZrUInt32);
    *outString = function_metadata_string_heap_lookup(function, stringIndex);
    return *outString != ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

/**
 * @brief 从 union TypeSpec 的签名节点提取其底层命名类型。
 *
 * @note 该信息只用于把 union 的 TypeSpec 身份关联到同一 metadata stream 中的 TypeDef，
 * 并不单独构成绑定依据；后续仍会比较定义签名和布局身份。
 */
static TZrBool function_type_spec_union_base_name(const SZrFunction *function,
                                                  const SZrMetadataTokenRecord *typeSpecRecord,
                                                  SZrString **outBaseName) {
    const TZrByte *blob;
    TZrSize cursor = 0;

    if (outBaseName != ZR_NULL) {
        *outBaseName = ZR_NULL;
    }
    if (!metadata_record_blob_is_valid(function, typeSpecRecord) || outBaseName == ZR_NULL) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + typeSpecRecord->signatureBlobOffset;
    if (blob[cursor] != ZR_METADATA_SIGNATURE_NODE_UNION) {
        return ZR_FALSE;
    }
    cursor += 1u;
    if (cursor + sizeof(TZrUInt32) > typeSpecRecord->signatureBlobLength) {
        return ZR_FALSE;
    }
    cursor += sizeof(TZrUInt32);
    return function_metadata_read_string_ref(function,
                                             blob,
                                             typeSpecRecord->signatureBlobLength,
                                             &cursor,
                                             outBaseName);
}

/** @brief 检查 TypeDef 签名节点中的名字是否等于给定 heap 字符串。 */
static TZrBool function_type_def_base_name_matches(const SZrFunction *function,
                                                   const SZrMetadataTokenRecord *typeDefRecord,
                                                   const SZrString *baseName) {
    const TZrByte *blob;
    TZrSize cursor = 0;
    SZrString *typeDefName = ZR_NULL;

    if (!metadata_record_blob_is_valid(function, typeDefRecord) || baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + typeDefRecord->signatureBlobOffset;
    if (blob[cursor] != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
        return ZR_FALSE;
    }
    cursor += 1u;
    if (!function_metadata_read_string_ref(function,
                                           blob,
                                           typeDefRecord->signatureBlobLength,
                                           &cursor,
                                           &typeDefName)) {
        return ZR_FALSE;
    }

    return typeDefName == baseName || ZrCore_String_Equal(typeDefName, (SZrString *)baseName)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/**
 * @brief 通过 union 节点的底层类型名在函数记录流中定位 TypeDef。
 *
 * @note 若同名 TypeDef 有多个候选，此 helper 取首个；之后的签名和 layout 校验负责
 * 判断候选能否作为跨模块对应定义。
 */
static const SZrMetadataTokenRecord *function_find_type_def_for_union_type_spec(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeSpecRecord) {
    SZrString *baseName = ZR_NULL;

    if (function == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL ||
        !function_type_spec_union_base_name(function, typeSpecRecord, &baseName)) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            function_type_def_base_name_matches(function, record, baseName)) {
            return record;
        }
    }

    return ZR_NULL;
}

/** @brief 为一个 TypeDef 实体取得配对 SIGNATURE 记录。 */
static const SZrMetadataTokenRecord *function_find_type_def_signature_record(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeDefRecord) {
    return typeDefRecord == ZR_NULL
                   ? ZR_NULL
                   : metadata_find_signature_record(function == ZR_NULL ? ZR_NULL : function->metadataTokenRecords,
                                                    function == ZR_NULL ? 0u : function->metadataTokenRecordLength,
                                                    typeDefRecord->token);
}

/** @brief 为一个 TypeRef 实体取得配对 SIGNATURE 记录。 */
static const SZrMetadataTokenRecord *function_find_type_ref_signature_record(
        const SZrFunction *function,
        const SZrMetadataTokenRecord *typeRefRecord) {
    return typeRefRecord == ZR_NULL
                   ? ZR_NULL
                   : metadata_find_signature_record(function == ZR_NULL ? ZR_NULL : function->metadataTokenRecords,
                                                    function == ZR_NULL ? 0u : function->metadataTokenRecordLength,
                                                    typeRefRecord->token);
}

/**
 * @brief 比较 caller TypeRef 的基本名与 provider TypeDef 的基本名。
 *
 * @note `TYPE_REF` 的稳定 target token/hash 等字段由外层先检查；本比较再防止同一目标
 * 身份被不同声明名引用。TODO: TypeRef 名称字符串索引解析失败时当前路径按兼容策略放行，
 * 需确认所有可加载 artifact 都保证该索引可解析，或改为 fail-closed。
 */
static TZrBool function_type_ref_base_name_matches_type_def(
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeRefRecord,
        const SZrFunction *providerFunction,
        const SZrMetadataTokenRecord *providerTypeDefRecord) {
    const TZrByte *blob;
    TZrSize cursor = 0;
    SZrString *typeRefName = ZR_NULL;

    if (!metadata_record_blob_is_valid(callerFunction, callerTypeRefRecord)) {
        return ZR_FALSE;
    }

    blob = callerFunction->signatureBlobHeap + callerTypeRefRecord->signatureBlobOffset;
    if (blob[cursor] != ZR_METADATA_SIGNATURE_NODE_TYPE_REF) {
        return ZR_FALSE;
    }
    cursor += 1u;
    if (cursor + sizeof(TZrUInt32) > callerTypeRefRecord->signatureBlobLength) {
        return ZR_FALSE;
    }
    cursor += sizeof(TZrUInt32);
    if (!function_metadata_read_string_ref(callerFunction,
                                           blob,
                                           callerTypeRefRecord->signatureBlobLength,
                                           &cursor,
                                           &typeRefName)) {
        return ZR_TRUE;
    }

    return function_type_def_base_name_matches(providerFunction, providerTypeDefRecord, typeRefName);
}

/**
 * @brief TypeRef 与 provider TypeDef 对照过程的结果分类。
 *
 * @note NONE 表示尚未分类或输入不适用；UNMATCHED 与定义/布局漂移分开计数，供
 * WithStatus API 形成调用方可诊断的结果。
 */
typedef enum EZrMetadataTypeRefMatchResult {
    ZR_METADATA_TYPE_REF_MATCH_NONE = 0,
    ZR_METADATA_TYPE_REF_MATCH_OK,
    ZR_METADATA_TYPE_REF_MATCH_UNMATCHED,
    ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH,
    ZR_METADATA_TYPE_REF_MATCH_LAYOUT_MISMATCH
} EZrMetadataTypeRefMatchResult;

/**
 * @brief 按 TypeRef 携带的稳定目标身份，在 provider 中验证对应 TypeDef。
 *
 * @note caller 的 target token 是检索键，签名 token/hash、module hash、layout 和基本名
 * 是逐级约束；找到 token 但身份不同会返回具体 drift 类别，而不是退化成 unmatched。
 */
static const SZrMetadataTokenRecord *function_find_targeted_type_ref_provider_type_def(
        const SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeRefRecord,
        const SZrFunction *providerFunction,
        EZrMetadataTypeRefMatchResult *outResult) {
    const SZrMetadataTokenRecord *targetRecord = ZR_NULL;

    if (outResult != ZR_NULL) {
        *outResult = ZR_METADATA_TYPE_REF_MATCH_NONE;
    }
    if (callerFunction == ZR_NULL ||
        callerTypeRefRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(callerTypeRefRecord->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < providerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *providerRecord = &providerFunction->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(providerRecord->token) != ZR_METADATA_TABLE_TYPE_DEF ||
            providerRecord->token != callerTypeRefRecord->targetMetadataToken) {
            continue;
        }
        targetRecord = providerRecord;
        if (callerTypeRefRecord->targetSignatureHash == 0u ||
            providerRecord->signatureHash != callerTypeRefRecord->targetSignatureHash) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }
        if (callerTypeRefRecord->targetSignatureToken != 0u &&
            providerRecord->relatedToken != callerTypeRefRecord->targetSignatureToken) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }
        if (callerTypeRefRecord->targetModuleSignatureHash != 0u &&
            providerFunction->moduleSignatureHash != callerTypeRefRecord->targetModuleSignatureHash) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }
        if ((callerTypeRefRecord->layoutVersion != 0u || callerTypeRefRecord->layoutHash != 0u ||
             providerRecord->layoutVersion != 0u || providerRecord->layoutHash != 0u) &&
            (callerTypeRefRecord->layoutVersion != providerRecord->layoutVersion ||
             callerTypeRefRecord->layoutHash != providerRecord->layoutHash)) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_LAYOUT_MISMATCH;
            }
            return providerRecord;
        }
        if (!function_type_ref_base_name_matches_type_def(callerFunction,
                                                          callerTypeRefRecord,
                                                          providerFunction,
                                                          providerRecord)) {
            if (outResult != ZR_NULL) {
                *outResult = ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
            }
            return providerRecord;
        }

        if (outResult != ZR_NULL) {
            *outResult = ZR_METADATA_TYPE_REF_MATCH_OK;
        }
        return providerRecord;
    }

    if (outResult != ZR_NULL) {
        *outResult = targetRecord == ZR_NULL
                             ? ZR_METADATA_TYPE_REF_MATCH_UNMATCHED
                             : ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH;
    }
    return ZR_NULL;
}

/**
 * @brief 将已完全匹配的 TypeSpec 与配对签名身份写入 caller sidecar。
 *
 * @note 由 TypeSpec 扫描器在签名和相关 union 定义/布局校验后调用；Upsert 失败时不写字段，
 * 成功时记录 expected caller identity 与 resolved provider identity。
 */
static TZrBool function_record_type_spec_binding(SZrState *state,
                                                 SZrFunction *callerFunction,
                                                 const SZrMetadataTokenRecord *callerTypeSpecRecord,
                                                 const SZrMetadataTokenRecord *callerSignatureRecord,
                                                 const SZrFunction *providerFunction,
                                                 const SZrMetadataTokenRecord *providerTypeSpecRecord,
                                                 const SZrMetadataTokenRecord *providerSignatureRecord) {
    SZrMetadataTokenBinding *binding;

    if (callerTypeSpecRecord == ZR_NULL ||
        callerSignatureRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerTypeSpecRecord == ZR_NULL ||
        providerSignatureRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state,
                                                          callerFunction,
                                                          callerTypeSpecRecord->token);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding->refToken = callerTypeSpecRecord->token;
    binding->refSignatureToken = callerSignatureRecord->token;
    binding->refSignatureHash = callerTypeSpecRecord->signatureHash;
    binding->expectedMetadataToken = callerTypeSpecRecord->token;
    binding->expectedSignatureToken = callerSignatureRecord->token;
    binding->expectedSignatureHash = callerTypeSpecRecord->signatureHash;
    binding->expectedModuleSignatureHash = 0u;
    binding->expectedLayoutVersion = 0u;
    binding->expectedLayoutHash = 0u;
    binding->resolvedMetadataToken = providerTypeSpecRecord->token;
    binding->resolvedSignatureToken = providerSignatureRecord->token;
    binding->resolvedSignatureHash = providerTypeSpecRecord->signatureHash;
    binding->resolvedModuleSignatureHash = providerFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = 0u;
    binding->resolvedLayoutHash = 0u;
    return ZR_TRUE;
}

/** @brief 记录 union TypeSpec 所依赖的 TypeDef 签名及布局身份。 */
static TZrBool function_record_type_def_layout_binding(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeDefRecord,
        const SZrMetadataTokenRecord *callerSignatureRecord,
        const SZrFunction *providerFunction,
        const SZrMetadataTokenRecord *providerTypeDefRecord,
        const SZrMetadataTokenRecord *providerSignatureRecord) {
    SZrMetadataTokenBinding *binding;

    if (callerTypeDefRecord == ZR_NULL ||
        callerSignatureRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerTypeDefRecord == ZR_NULL ||
        providerSignatureRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state,
                                                          callerFunction,
                                                          callerTypeDefRecord->token);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding->refToken = callerTypeDefRecord->token;
    binding->refSignatureToken = callerSignatureRecord->token;
    binding->refSignatureHash = callerTypeDefRecord->signatureHash;
    binding->expectedMetadataToken = callerTypeDefRecord->token;
    binding->expectedSignatureToken = callerSignatureRecord->token;
    binding->expectedSignatureHash = callerTypeDefRecord->signatureHash;
    binding->expectedModuleSignatureHash = 0u;
    binding->expectedLayoutVersion = callerTypeDefRecord->layoutVersion;
    binding->expectedLayoutHash = callerTypeDefRecord->layoutHash;
    binding->resolvedMetadataToken = providerTypeDefRecord->token;
    binding->resolvedSignatureToken = providerSignatureRecord->token;
    binding->resolvedSignatureHash = providerTypeDefRecord->signatureHash;
    binding->resolvedModuleSignatureHash = providerFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = providerTypeDefRecord->layoutVersion;
    binding->resolvedLayoutHash = providerTypeDefRecord->layoutHash;
    return ZR_TRUE;
}

/**
 * @brief 记录已验证 TypeRef 到 provider TypeDef 的目标、签名、模块及布局身份。
 *
 * @note 该 helper 不负责发现候选或比较约束；只有完整匹配后才由 TypeRef 扫描器调用。
 */
static TZrBool function_record_type_ref_binding(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *callerTypeRefRecord,
        const SZrMetadataTokenRecord *callerSignatureRecord,
        const SZrFunction *providerFunction,
        const SZrMetadataTokenRecord *providerTypeDefRecord,
        const SZrMetadataTokenRecord *providerSignatureRecord) {
    SZrMetadataTokenBinding *binding;

    if (callerTypeRefRecord == ZR_NULL ||
        callerSignatureRecord == ZR_NULL ||
        providerFunction == ZR_NULL ||
        providerTypeDefRecord == ZR_NULL ||
        providerSignatureRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state,
                                                          callerFunction,
                                                          callerTypeRefRecord->token);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding->refToken = callerTypeRefRecord->token;
    binding->refSignatureToken = callerSignatureRecord->token;
    binding->refSignatureHash = callerTypeRefRecord->signatureHash;
    binding->expectedMetadataToken = callerTypeRefRecord->targetMetadataToken;
    binding->expectedSignatureToken = callerTypeRefRecord->targetSignatureToken;
    binding->expectedSignatureHash = callerTypeRefRecord->targetSignatureHash;
    binding->expectedModuleSignatureHash = callerTypeRefRecord->targetModuleSignatureHash;
    binding->expectedLayoutVersion = callerTypeRefRecord->layoutVersion;
    binding->expectedLayoutHash = callerTypeRefRecord->layoutHash;
    binding->resolvedMetadataToken = providerTypeDefRecord->token;
    binding->resolvedSignatureToken = providerSignatureRecord->token;
    binding->resolvedSignatureHash = providerTypeDefRecord->signatureHash;
    binding->resolvedModuleSignatureHash = providerFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = providerTypeDefRecord->layoutVersion;
    binding->resolvedLayoutHash = providerTypeDefRecord->layoutHash;
    return ZR_TRUE;
}

/**
 * @brief 只读查询函数级 ref-to-def binding sidecar。
 *
 * @pre function 在查询期间仍有效；返回指针借用函数拥有的 sidecar 存储。
 * @return 找到该 ref token 时返回 binding，否则返回 ZR_NULL。
 * @note 运行期反射泛型解析与模块调用兼容性检查消费成功验证留下的结果；查询不触发解析。
 */
const SZrMetadataTokenBinding *ZrCore_Function_FindModuleMetadataBinding(const SZrFunction *function,
                                                                         TZrMetadataToken refToken) {
    if (function == ZR_NULL || refToken == 0u || function->moduleMetadataBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->moduleMetadataBindingLength; index++) {
        const SZrMetadataTokenBinding *binding = &function->moduleMetadataBindings[index];

        if (binding->refToken == refToken) {
            return binding;
        }
    }

    return ZR_NULL;
}

/**
 * @brief 按 caller ref token 复用或扩展函数拥有的 binding sidecar。
 *
 * @pre state、global、function 和非零 refToken 必须有效；返回指针只在后续扩容前稳定。
 * @return 新建或已存在的可写记录；参数无效或分配失败时返回 ZR_NULL。
 * @note import signature 验证先取得槽位，再填入 expected/resolved identity；重复 ref 更新
 * 原槽位以维持每个 ref token 一个结果。
 */
SZrMetadataTokenBinding *ZrCore_Function_UpsertModuleMetadataBinding(SZrState *state,
                                                                     SZrFunction *function,
                                                                     TZrMetadataToken refToken) {
    SZrMetadataTokenBinding *newBindings;
    TZrUInt32 newCapacity;
    TZrSize newBytes;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || refToken == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->moduleMetadataBindingLength; index++) {
        if (function->moduleMetadataBindings[index].refToken == refToken) {
            return &function->moduleMetadataBindings[index];
        }
    }

    if (function->moduleMetadataBindingLength >= function->moduleMetadataBindingCapacity) {
        newCapacity = function->moduleMetadataBindingCapacity == 0u
                              ? 4u
                              : function->moduleMetadataBindingCapacity * 2u;
        if (newCapacity <= function->moduleMetadataBindingCapacity) {
            return ZR_NULL;
        }

        newBytes = sizeof(SZrMetadataTokenBinding) * (TZrSize)newCapacity;
        newBindings = (SZrMetadataTokenBinding *)ZrCore_Memory_RawMallocWithType(
                state->global,
                newBytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newBindings == ZR_NULL) {
            return ZR_NULL;
        }

        ZrCore_Memory_RawSet(newBindings, 0, newBytes);
        if (function->moduleMetadataBindings != ZR_NULL && function->moduleMetadataBindingCapacity > 0u) {
            ZrCore_Memory_RawCopy(newBindings,
                                  function->moduleMetadataBindings,
                                  sizeof(SZrMetadataTokenBinding) *
                                          (TZrSize)function->moduleMetadataBindingLength);
            ZrCore_Memory_RawFreeWithType(state->global,
                                          function->moduleMetadataBindings,
                                          sizeof(SZrMetadataTokenBinding) *
                                                  (TZrSize)function->moduleMetadataBindingCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }

        function->moduleMetadataBindings = newBindings;
        function->moduleMetadataBindingCapacity = newCapacity;
    }

    return &function->moduleMetadataBindings[function->moduleMetadataBindingLength++];
}

/**
 * @brief 执行无状态输出的 TypeSpec best-effort 绑定入口。
 *
 * @note 单纯存在 unmatched TypeSpec 按历史契约可继续；需诊断差异的调用方使用 WithStatus。
 * BUG: 先遇 unmatched、后续绑定写入失败时，仅凭 unmatched 计数会把失败误报为成功，留下不完整 sidecar。
 */
TZrBool ZrCore_Function_BindMatchingTypeSpecMetadata(SZrState *state,
                                                     SZrFunction *callerFunction,
                                                     const SZrFunction *providerFunction) {
    SZrMetadataTypeSpecBindStatus status;

    if (!ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(state,
                                                                callerFunction,
                                                                providerFunction,
                                                                &status)) {
        return status.unmatchedTypeSpecCount > 0u &&
                       status.definitionMismatchCount == 0u &&
                       status.layoutMismatchCount == 0u
                       ? ZR_TRUE
                       : ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 扫描 caller TypeSpec，并为 provider 中可验证的对应项写入 binding sidecar。
 *
 * @pre 两个函数及 state 有效且 caller/provider token record 数组存在；status 非空时在进入扫描
 * 前清零并汇总 caller、匹配、未匹配及首个 drift 上下文。
 * @return status 非空时，仅全部适用 TypeSpec 匹配且无漂移才返回 true；未匹配项仍可写入
 * 其他匹配项。status 为空时失配按 best-effort 返回 true；输入、签名或写入硬失败仍返回 false。
 * @note Union TypeSpec 还要求对应 TypeDef 签名 hash/blob 与布局身份一致，检查通过后才写入
 * 两类 sidecar；helper 不是原子事务，后续分配失败可能保留此前成功写入的记录。status 为空时
 * 不累计失配，最终按 best-effort 规则返回 true，即使存在 unmatched 或定义/布局漂移。
 */
TZrBool ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunction *providerFunction,
        SZrMetadataTypeSpecBindStatus *status) {
    if (state == ZR_NULL ||
        callerFunction == ZR_NULL ||
        providerFunction == ZR_NULL ||
        callerFunction->metadataTokenRecords == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL) {
        if (status != ZR_NULL) {
            ZrCore_Memory_RawSet(status, 0, sizeof(*status));
        }
        return ZR_FALSE;
    }
    if (status != ZR_NULL) {
        ZrCore_Memory_RawSet(status, 0, sizeof(*status));
    }

    for (TZrUInt32 index = 0; index < callerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *callerRecord = &callerFunction->metadataTokenRecords[index];
        const SZrMetadataTokenRecord *providerRecord;
        const SZrMetadataTokenRecord *callerSignatureRecord;
        const SZrMetadataTokenRecord *providerSignatureRecord;
        const SZrMetadataTokenRecord *callerTypeDefRecord;
        const SZrMetadataTokenRecord *providerTypeDefRecord;
        const SZrMetadataTokenRecord *callerTypeDefSignatureRecord;
        const SZrMetadataTokenRecord *providerTypeDefSignatureRecord;

        if (ZR_METADATA_TOKEN_TABLE(callerRecord->token) != ZR_METADATA_TABLE_TYPE_SPEC) {
            continue;
        }
        if (status != ZR_NULL) {
            status->callerTypeSpecCount++;
        }

        providerRecord = function_find_matching_type_spec_record(providerFunction,
                                                                 callerFunction,
                                                                 callerRecord);
        if (providerRecord == ZR_NULL) {
            /* TypeSpec 没有 provider 对应项时保留 best-effort import 语义，并把缺口交给 status 诊断。 */
            if (status != ZR_NULL) {
                status->unmatchedTypeSpecCount++;
                if (status->firstUnmatchedTypeSpecToken == 0u) {
                    status->firstUnmatchedTypeSpecToken = callerRecord->token;
                    status->firstUnmatchedSignatureHash = callerRecord->signatureHash;
                }
            }
            continue;
        }

        callerSignatureRecord = function_find_type_spec_signature_record(callerFunction, callerRecord);
        providerSignatureRecord = function_find_type_spec_signature_record(providerFunction, providerRecord);
        if (callerSignatureRecord == ZR_NULL || providerSignatureRecord == ZR_NULL) {
            return ZR_FALSE;
        }
        callerTypeDefRecord = function_find_type_def_for_union_type_spec(callerFunction, callerRecord);
        providerTypeDefRecord = function_find_type_def_for_union_type_spec(providerFunction, providerRecord);
        /* Union 的定义身份和布局是 TypeSpec 绑定的组成部分；任一侧缺失或漂移都不能留下该绑定。 */
        if (callerTypeDefRecord != ZR_NULL || providerTypeDefRecord != ZR_NULL) {
            if (callerTypeDefRecord == ZR_NULL || providerTypeDefRecord == ZR_NULL) {
                if (status != ZR_NULL) {
                    status->definitionMismatchCount++;
                    if (status->firstDefinitionMismatchTypeSpecToken == 0u) {
                        status->firstDefinitionMismatchTypeSpecToken = callerRecord->token;
                        status->firstExpectedDefinitionSignatureHash =
                                callerTypeDefRecord != ZR_NULL ? callerTypeDefRecord->signatureHash : 0u;
                        status->firstActualDefinitionSignatureHash =
                                providerTypeDefRecord != ZR_NULL ? providerTypeDefRecord->signatureHash : 0u;
                    }
                }
                continue;
            }
            if (callerTypeDefRecord->signatureHash != providerTypeDefRecord->signatureHash ||
                !metadata_record_signature_blobs_equal(callerFunction,
                                                       callerTypeDefRecord,
                                                       providerFunction,
                                                       providerTypeDefRecord)) {
                if (status != ZR_NULL) {
                    status->definitionMismatchCount++;
                    if (status->firstDefinitionMismatchTypeSpecToken == 0u) {
                        status->firstDefinitionMismatchTypeSpecToken = callerRecord->token;
                        status->firstExpectedDefinitionSignatureHash = callerTypeDefRecord->signatureHash;
                        status->firstActualDefinitionSignatureHash = providerTypeDefRecord->signatureHash;
                    }
                }
                continue;
            }
            if (callerTypeDefRecord->layoutVersion != providerTypeDefRecord->layoutVersion ||
                callerTypeDefRecord->layoutHash != providerTypeDefRecord->layoutHash) {
                if (status != ZR_NULL) {
                    status->layoutMismatchCount++;
                    if (status->firstLayoutMismatchTypeSpecToken == 0u) {
                        status->firstLayoutMismatchTypeSpecToken = callerRecord->token;
                        status->firstExpectedLayoutVersion = callerTypeDefRecord->layoutVersion;
                        status->firstExpectedLayoutHash = callerTypeDefRecord->layoutHash;
                        status->firstActualLayoutVersion = providerTypeDefRecord->layoutVersion;
                        status->firstActualLayoutHash = providerTypeDefRecord->layoutHash;
                    }
                }
                continue;
            }
        }

        if (!function_record_type_spec_binding(state,
                                               callerFunction,
                                               callerRecord,
                                               callerSignatureRecord,
                                               providerFunction,
                                               providerRecord,
                                               providerSignatureRecord)) {
            return ZR_FALSE;
        }
        if (callerTypeDefRecord != ZR_NULL || providerTypeDefRecord != ZR_NULL) {
            callerTypeDefSignatureRecord = function_find_type_def_signature_record(callerFunction,
                                                                                   callerTypeDefRecord);
            providerTypeDefSignatureRecord = function_find_type_def_signature_record(providerFunction,
                                                                                     providerTypeDefRecord);
            if (callerTypeDefSignatureRecord == ZR_NULL ||
                providerTypeDefSignatureRecord == ZR_NULL ||
                !function_record_type_def_layout_binding(state,
                                                         callerFunction,
                                                         callerTypeDefRecord,
                                                         callerTypeDefSignatureRecord,
                                                         providerFunction,
                                                         providerTypeDefRecord,
                                                         providerTypeDefSignatureRecord)) {
                return ZR_FALSE;
            }
        }
        if (status != ZR_NULL) {
            status->matchedTypeSpecCount++;
        }
    }

    return status == ZR_NULL ||
                   (status->unmatchedTypeSpecCount == 0u &&
                    status->definitionMismatchCount == 0u &&
                    status->layoutMismatchCount == 0u)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/**
 * @brief 兼容旧调用方的 TypeRef 绑定包装入口。
 *
 * @note 状态细节由 WithStatus 版本统一产生；此包装保持相同匹配规则并折叠为布尔结果。
 */
TZrBool ZrCore_Function_BindMatchingTypeRefMetadata(SZrState *state,
                                                    SZrFunction *callerFunction,
                                                    const SZrFunction *providerFunction) {
    SZrMetadataTypeRefBindStatus status;

    if (!ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(state,
                                                               callerFunction,
                                                               providerFunction,
                                                               &status)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 绑定携带稳定 provider TypeDef target identity 的 caller TypeRef。
 *
 * @pre state、caller/provider 函数和两侧 token record 数组有效；status 非空时在扫描前清零并
 * 记录 caller、matched、unmatched 及定义/布局 drift 的首个上下文。
 * @return status 非空时，无适用 TypeRef 或全部匹配才返回 true；失配会计入 status 并返回 false。
 * status 为空时失配按 best-effort 返回 true；配对签名缺失或绑定写入失败仍返回 false。
 * @note 仅处理 targetMetadataToken 指向 TypeDef table 的记录；status 为空时失配不影响 true
 * 返回值。module import 在已经验证主导入后调用它补充跨模块类型身份 sidecar 与诊断，故其
 * best-effort 结果不回滚已验证的 import。
 */
TZrBool ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunction *providerFunction,
        SZrMetadataTypeRefBindStatus *status) {
    if (state == ZR_NULL ||
        callerFunction == ZR_NULL ||
        providerFunction == ZR_NULL ||
        callerFunction->metadataTokenRecords == ZR_NULL ||
        providerFunction->metadataTokenRecords == ZR_NULL) {
        if (status != ZR_NULL) {
            ZrCore_Memory_RawSet(status, 0, sizeof(*status));
        }
        return ZR_FALSE;
    }
    if (status != ZR_NULL) {
        ZrCore_Memory_RawSet(status, 0, sizeof(*status));
    }

    for (TZrUInt32 index = 0; index < callerFunction->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *callerRecord = &callerFunction->metadataTokenRecords[index];
        const SZrMetadataTokenRecord *callerSignatureRecord;
        const SZrMetadataTokenRecord *providerRecord;
        const SZrMetadataTokenRecord *providerSignatureRecord;
        EZrMetadataTypeRefMatchResult matchResult;

        if (ZR_METADATA_TOKEN_TABLE(callerRecord->token) != ZR_METADATA_TABLE_TYPE_REF ||
            ZR_METADATA_TOKEN_TABLE(callerRecord->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF) {
            continue;
        }
        if (status != ZR_NULL) {
            status->callerTypeRefCount++;
        }

        callerSignatureRecord = function_find_type_ref_signature_record(callerFunction, callerRecord);
        providerRecord = function_find_targeted_type_ref_provider_type_def(callerFunction,
                                                                           callerRecord,
                                                                           providerFunction,
                                                                           &matchResult);
        /* 目标查找已分类签名、模块、布局和名字身份；这里只汇总差异，不把失配项写入 sidecar。 */
        if (matchResult == ZR_METADATA_TYPE_REF_MATCH_UNMATCHED) {
            if (status != ZR_NULL) {
                status->unmatchedTypeRefCount++;
                if (status->firstUnmatchedTypeRefToken == 0u) {
                    status->firstUnmatchedTypeRefToken = callerRecord->token;
                    status->firstUnmatchedSignatureHash = callerRecord->targetSignatureHash;
                }
            }
            continue;
        }
        if (matchResult == ZR_METADATA_TYPE_REF_MATCH_DEFINITION_MISMATCH) {
            if (status != ZR_NULL) {
                status->definitionMismatchCount++;
                if (status->firstDefinitionMismatchTypeRefToken == 0u) {
                    status->firstDefinitionMismatchTypeRefToken = callerRecord->token;
                    status->firstExpectedDefinitionSignatureHash = callerRecord->targetSignatureHash;
                    status->firstActualDefinitionSignatureHash =
                            providerRecord != ZR_NULL ? providerRecord->signatureHash : 0u;
                }
            }
            continue;
        }
        if (matchResult == ZR_METADATA_TYPE_REF_MATCH_LAYOUT_MISMATCH) {
            if (status != ZR_NULL) {
                status->layoutMismatchCount++;
                if (status->firstLayoutMismatchTypeRefToken == 0u) {
                    status->firstLayoutMismatchTypeRefToken = callerRecord->token;
                    status->firstExpectedLayoutVersion = callerRecord->layoutVersion;
                    status->firstExpectedLayoutHash = callerRecord->layoutHash;
                    status->firstActualLayoutVersion = providerRecord != ZR_NULL ? providerRecord->layoutVersion : 0u;
                    status->firstActualLayoutHash = providerRecord != ZR_NULL ? providerRecord->layoutHash : 0u;
                }
            }
            continue;
        }
        providerSignatureRecord = function_find_type_def_signature_record(providerFunction, providerRecord);
        if (callerSignatureRecord == ZR_NULL ||
            providerRecord == ZR_NULL ||
            providerSignatureRecord == ZR_NULL ||
            !function_record_type_ref_binding(state,
                                              callerFunction,
                                              callerRecord,
                                              callerSignatureRecord,
                                              providerFunction,
                                              providerRecord,
                                              providerSignatureRecord)) {
            return ZR_FALSE;
        }
        if (status != ZR_NULL) {
            status->matchedTypeRefCount++;
        }
    }

    return status == ZR_NULL ||
                   (status->unmatchedTypeRefCount == 0u &&
                    status->definitionMismatchCount == 0u &&
                    status->layoutMismatchCount == 0u)
                   ? ZR_TRUE
                   : ZR_FALSE;
}
