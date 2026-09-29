#include "compiler_metadata_module_record.h"
#include "compiler_metadata_signature.h"
#include "compiler_metadata_type_def.h"

/* Module 签名固定由节点标记及两个 string-heap 索引组成；plan 与 emit 共用该尺寸，
 * 使调用方能先累计容量，再在统一缓冲区中顺序写入。 */
static TZrSize metadata_module_record_signature_size(void) {
    return 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
}

/* 按 Module 签名格式写入模块名和版本的 string-heap 索引；缺失字符串由共享索引
 * 规则编码为 0，因此收集阶段必须先把可用名称加入最终排序后的字符串表。 */
static void metadata_module_record_write_signature(TZrByte *buffer,
                                                   TZrSize *offset,
                                                   const SZrFunction *function,
                                                   const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                   TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_MODULE);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    function != ZR_NULL ? function->functionName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    function != ZR_NULL ? function->moduleVersion : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
}

/* 参与 metadata token 构建前的统一字符串收集，使 emit 使用稳定的 string heap 索引；
 * collector 失败即中止整个构建，function 为空则表示本次没有 Module 数据。 */
TZrBool compiler_metadata_module_record_collect_strings(SZrCompilerState *cs,
                                                        const SZrFunction *function,
                                                        TZrMetadataTypeDefStringCollector collector,
                                                        void *userData) {
    if (collector == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!collector(cs, function->functionName, userData)) {
        return ZR_FALSE;
    }
    return collector(cs, function->moduleVersion, userData);
}

/* 供 token 构建器预留一条 Module 记录及其签名字节；plan 仅是该次同步构建的
 * 临时容量描述，不持有资源，必须与同一 function 的 emit 配对使用。 */
TZrBool compiler_metadata_module_record_plan(SZrCompilerState *cs,
                                             const SZrFunction *function,
                                             SZrMetadataModuleRecordPlan *outPlan) {
    ZR_UNUSED_PARAMETER(cs);

    if (outPlan == ZR_NULL) {
        return ZR_FALSE;
    }
    outPlan->moduleRecordCount = 0u;
    outPlan->signatureHeapLength = 0u;
    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    outPlan->moduleRecordCount = 1u;
    outPlan->signatureHeapLength = metadata_module_record_signature_size();
    return ZR_TRUE;
}

/* 将唯一的 Module 行与其 Signature 行写成互相引用的一对记录。调用方须先依据
 * plan 预留两条记录和签名堆空间，并提供最终 string heap；失败时 heap offset 可能已推进，
 * 因此输出数组、heap 与游标只应视为本次构建的暂存状态并整体丢弃。 */
TZrBool compiler_metadata_module_record_emit(SZrCompilerState *cs,
                                             const SZrFunction *function,
                                             SZrMetadataTokenRecord *records,
                                             TZrUInt32 recordCount,
                                             TZrUInt32 *ioRecordIndex,
                                             TZrByte *heap,
                                             TZrSize heapLength,
                                             TZrSize *ioHeapOffset,
                                             TZrUInt32 *ioSignatureRidCursor,
                                             const SZrMetadataStringHeapEntry *stringHeapEntries,
                                             TZrUInt32 stringHeapEntryCount) {
    TZrSize signatureStart;
    TZrSize expectedLength;
    TZrUInt64 signatureHash;
    TZrUInt32 recordIndex;
    TZrMetadataToken moduleToken;
    TZrMetadataToken signatureToken;

    ZR_UNUSED_PARAMETER(cs);

    if (function == ZR_NULL || records == ZR_NULL || ioRecordIndex == ZR_NULL || heap == ZR_NULL ||
        ioHeapOffset == ZR_NULL || ioSignatureRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedLength = metadata_module_record_signature_size();
    /* 记录以成对形式写入，且 signature blob offset 是 32 位字段；上层 plan
     * 聚合器负责确保总长度可表示，局部检查防止越过实际分配容量。 */
    if (*ioHeapOffset > heapLength || expectedLength > heapLength - *ioHeapOffset ||
        *ioRecordIndex + 1u >= recordCount) {
        return ZR_FALSE;
    }

    signatureStart = *ioHeapOffset;
    metadata_module_record_write_signature(heap,
                                           ioHeapOffset,
                                           function,
                                           stringHeapEntries,
                                           stringHeapEntryCount);
    if (*ioHeapOffset - signatureStart != expectedLength) {
        return ZR_FALSE;
    }

    signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
    if (signatureHash == 0u) {
        return ZR_FALSE;
    }

    moduleToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u);
    /* 此格式每个函数仅有一个 Module（RID 固定为 1）；Signature RID 与其它
     * 元数据签名共用游标，双向关联使通用 token 查询能从任一侧定位签名。 */
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*ioSignatureRidCursor)++);
    recordIndex = *ioRecordIndex;
    records[recordIndex].token = moduleToken;
    records[recordIndex].relatedToken = signatureToken;
    records[recordIndex].ownerToken = 0u;
    records[recordIndex].ownerIndex = 0u;
    records[recordIndex].signatureBlobOffset = (TZrUInt32)signatureStart;
    records[recordIndex].signatureBlobLength = (TZrUInt32)expectedLength;
    records[recordIndex].signatureHash = signatureHash;
    recordIndex++;

    records[recordIndex].token = signatureToken;
    records[recordIndex].relatedToken = moduleToken;
    records[recordIndex].ownerToken = moduleToken;
    records[recordIndex].ownerIndex = 0u;
    records[recordIndex].signatureBlobOffset = (TZrUInt32)signatureStart;
    records[recordIndex].signatureBlobLength = (TZrUInt32)expectedLength;
    records[recordIndex].signatureHash = signatureHash;
    recordIndex++;

    *ioRecordIndex = recordIndex;
    return ZR_TRUE;
}
