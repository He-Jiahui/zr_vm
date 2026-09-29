#include "compiler_metadata_ref.h"

/**
 * @brief 在函数级 token 记录中按 token 找到关联项；导入聚合用它取回实体对应的 SIGNATURE。
 */
static const SZrMetadataTokenRecord *metadata_ref_find_record(const SZrMetadataTokenRecord *records,
                                                              TZrUInt32 recordCount,
                                                              TZrMetadataToken token) {
    if (records == ZR_NULL || token == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < recordCount; index++) {
        if (records[index].token == token) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

/**
 * @brief 限定模块级导入表的实体种类，避免把本地定义重复复制到跨模块引用快照。
 */
static TZrBool metadata_ref_record_is_import_entity(const SZrMetadataTokenRecord *record) {
    TZrUInt32 table;

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    table = ZR_METADATA_TOKEN_TABLE(record->token);
    return table == ZR_METADATA_TABLE_ASSEMBLY_REF ||
           table == ZR_METADATA_TABLE_TYPE_REF ||
           table == ZR_METADATA_TABLE_MEMBER_REF;
}

/**
 * @brief 以签名字节补足哈希比较，避免碰撞或堆偏移差异影响导入实体去重。
 */
static TZrBool metadata_ref_records_have_same_signature_blob(const SZrFunction *function,
                                                              const SZrMetadataTokenRecord *left,
                                                              const SZrMetadataTokenRecord *right) {
    /* TODO: 零长签名若以 heap 末尾为合法 offset，当前严格边界会判为不同；需核对上游是否能生成。 */
    if (function == ZR_NULL || left == ZR_NULL || right == ZR_NULL ||
        left->signatureBlobLength != right->signatureBlobLength ||
        left->signatureBlobOffset >= function->signatureBlobHeapLength ||
        right->signatureBlobOffset >= function->signatureBlobHeapLength ||
        left->signatureBlobLength > function->signatureBlobHeapLength - left->signatureBlobOffset ||
        right->signatureBlobLength > function->signatureBlobHeapLength - right->signatureBlobOffset) {
        return ZR_FALSE;
    }

    return left->signatureBlobLength == 0 ||
           ZrCore_Memory_RawCompare(function->signatureBlobHeap + left->signatureBlobOffset,
                                    function->signatureBlobHeap + right->signatureBlobOffset,
                                    left->signatureBlobLength) == 0
                   ? ZR_TRUE
                   : ZR_FALSE;
}

/**
 * @brief 按目标身份和签名字节判断实体是否已选，使模块侧快照只保留一份等价导入。
 */
static TZrBool metadata_ref_entity_seen(const SZrFunction *function,
                                        const TZrUInt32 *selectedIndexes,
                                        TZrUInt32 selectedCount,
                                        const SZrMetadataTokenRecord *record) {
    TZrUInt32 table;

    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        selectedIndexes == ZR_NULL || record == ZR_NULL) {
        return ZR_FALSE;
    }

    table = ZR_METADATA_TOKEN_TABLE(record->token);
    for (TZrUInt32 index = 0; index < selectedCount; index++) {
        const SZrMetadataTokenRecord *selected = &function->metadataTokenRecords[selectedIndexes[index]];
        if (ZR_METADATA_TOKEN_TABLE(selected->token) == table &&
            selected->ownerToken == record->ownerToken &&
            selected->signatureHash == record->signatureHash &&
            selected->targetMetadataToken == record->targetMetadataToken &&
            selected->targetSignatureToken == record->targetSignatureToken &&
            selected->targetSignatureHash == record->targetSignatureHash &&
            selected->targetModuleSignatureHash == record->targetModuleSignatureHash &&
            /* TODO: 同文异对象的长版本字符串会绕过去重；需核实生产构造链是否能送来不同指针。 */
            selected->requestedModuleVersion == record->requestedModuleVersion &&
            selected->minModuleVersionInclusive == record->minModuleVersionInclusive &&
            selected->maxModuleVersionExclusive == record->maxModuleVersionExclusive &&
            metadata_ref_records_have_same_signature_blob(function, selected, record)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/**
 * @brief 重建前释放旧模块侧导入快照，维持序列化表与当前函数级 metadata 的一致性。
 */
static void metadata_ref_clear_module_table(SZrCompilerState *cs, SZrFunction *function) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || function == ZR_NULL) {
        return;
    }

    if (function->moduleMetadataTokenRecords != ZR_NULL && function->moduleMetadataTokenRecordLength > 0) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      function->moduleMetadataTokenRecords,
                                      sizeof(SZrMetadataTokenRecord) * function->moduleMetadataTokenRecordLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    function->moduleMetadataTokenRecords = ZR_NULL;
    function->moduleMetadataTokenRecordLength = 0;
}

/**
 * @brief 从主 metadata 表聚合导入实体及签名，供 artifact 写出和运行时快速查询；关联缺失或分配失败即失败。
 * @note 通过参数校验并开始重建后先释放旧快照；此后的失败留下空表，唯一调用方随即清除本轮函数 metadata。
 */
TZrBool compiler_build_module_metadata_ref_table(SZrCompilerState *cs, SZrFunction *function) {
    TZrUInt32 *selectedIndexes;
    TZrUInt32 selectedCount = 0;
    SZrMetadataTokenRecord *records;
    TZrUInt32 outputIndex = 0;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    metadata_ref_clear_module_table(cs, function);
    if (function->metadataTokenRecords == ZR_NULL || function->metadataTokenRecordLength == 0) {
        return ZR_TRUE;
    }

    selectedIndexes = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(TZrUInt32) *
                                                                          function->metadataTokenRecordLength,
                                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (selectedIndexes == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 仅挑选能找到 SIGNATURE 配对的模块导入实体；缺失时让上层放弃整轮函数 metadata。 */
    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        const SZrMetadataTokenRecord *signatureRecord;

        if (!metadata_ref_record_is_import_entity(record) ||
            metadata_ref_entity_seen(function, selectedIndexes, selectedCount, record)) {
            continue;
        }

        signatureRecord = metadata_ref_find_record(function->metadataTokenRecords,
                                                   function->metadataTokenRecordLength,
                                                   record->relatedToken);
        if (signatureRecord == ZR_NULL ||
            ZR_METADATA_TOKEN_TABLE(signatureRecord->token) != ZR_METADATA_TABLE_SIGNATURE) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          selectedIndexes,
                                          sizeof(TZrUInt32) * function->metadataTokenRecordLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        selectedIndexes[selectedCount++] = index;
    }

    if (selectedCount == 0) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      selectedIndexes,
                                      sizeof(TZrUInt32) * function->metadataTokenRecordLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_TRUE;
    }

    records = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                       sizeof(SZrMetadataTokenRecord) *
                                                                               selectedCount * 2u,
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      selectedIndexes,
                                      sizeof(TZrUInt32) * function->metadataTokenRecordLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < selectedCount; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[selectedIndexes[index]];
        const SZrMetadataTokenRecord *signatureRecord =
                metadata_ref_find_record(function->metadataTokenRecords,
                                         function->metadataTokenRecordLength,
                                         record->relatedToken);

        records[outputIndex++] = *record;
        records[outputIndex++] = *signatureRecord;
    }

    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  selectedIndexes,
                                  sizeof(TZrUInt32) * function->metadataTokenRecordLength,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    /* 将实体与 SIGNATURE 配对快照交给函数对象；writer 序列化，跨模块查询读取，函数析构负责回收。 */
    function->moduleMetadataTokenRecords = records;
    function->moduleMetadataTokenRecordLength = outputIndex;
    return ZR_TRUE;
}
