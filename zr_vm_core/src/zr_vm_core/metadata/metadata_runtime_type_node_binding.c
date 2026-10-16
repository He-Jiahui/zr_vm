#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

/* 比较已解析节点的完整编码片段，长度和范围由调用方传入的 view 界定。 */
static TZrBool metadata_runtime_type_node_spans_match(
        const SZrZrpMetadataPoolSliceView *leftBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *leftNode,
        const SZrZrpMetadataPoolSliceView *rightBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *rightNode) {
    TZrSize leftLength;
    TZrSize rightLength;

    if (leftBlob == ZR_NULL || leftNode == ZR_NULL || rightBlob == ZR_NULL || rightNode == ZR_NULL ||
        leftBlob->data == ZR_NULL || rightBlob->data == ZR_NULL ||
        leftNode->nextBlobOffset < leftNode->blobOffset ||
        leftNode->nextBlobOffset > leftBlob->byteLength ||
        rightNode->nextBlobOffset < rightNode->blobOffset ||
        rightNode->nextBlobOffset > rightBlob->byteLength) {
        return ZR_FALSE;
    }

    leftLength = (TZrSize)(leftNode->nextBlobOffset - leftNode->blobOffset);
    rightLength = (TZrSize)(rightNode->nextBlobOffset - rightNode->blobOffset);
    return (TZrBool)(leftLength == rightLength &&
                     ZrCore_Memory_RawCompare(
                             (TZrPtr)(leftBlob->data + leftNode->blobOffset),
                             (TZrPtr)(rightBlob->data + rightNode->blobOffset),
                             leftLength) == 0);
}

/* 在指定 token 表逐条取经 ZRP 校验的签名，再匹配一个完整根节点与输入子节点。 */
static const SZrMetadataTokenRecord *metadata_runtime_find_type_node_record(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *blob,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView,
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        TZrUInt32 table) {
    SZrZrpMetadataPoolSliceView recordBlob;
    SZrMetadataRuntimeSignatureTypeNodeView recordNode;
    TZrUInt32 index;

    if (records == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < recordCount; ++index) {
        const SZrMetadataTokenRecord *record = &records[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) != table ||
            !ZrCore_MetadataRuntime_GetSignatureBlob(runtime, record->token, &recordBlob) ||
            !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&recordBlob, 0u, &recordNode) ||
            recordNode.nextBlobOffset != (TZrUInt32)recordBlob.byteLength) {
            continue;
        }
        /* TODO: 此处只取首个字节匹配项；TypeRef 签名编码不含模块名，而记录可按模块区分。
         * 需用跨模块同名 TypeRef 的泛型签名核对上层是否已限定身份及首项选择结果。 */
        if (metadata_runtime_type_node_spans_match(blob, nodeView, &recordBlob, &recordNode)) {
            return record;
        }
    }
    return ZR_NULL;
}

/* 将签名节点映射到当前函数元数据记录：TypeDef/TypeSpec 查主数组，TypeRef 查导入副本。
 * 返回指针借自 metadataFunction，记录与底层 ZRP blob 均须在使用期间保持有效。
 */
const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveSignatureTypeNodeRecord(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *blob,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView) {
    SZrFunction *metadataFunction;

    if (runtime == ZR_NULL || blob == ZR_NULL || nodeView == ZR_NULL ||
        (metadataFunction = runtime->metadataFunction) == ZR_NULL) {
        return ZR_NULL;
    }

    switch (nodeView->node) {
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            return metadata_runtime_find_type_node_record(runtime,
                                                          blob,
                                                          nodeView,
                                                          metadataFunction->metadataTokenRecords,
                                                          metadataFunction->metadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_TYPE_DEF);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
            return metadata_runtime_find_type_node_record(runtime,
                                                          blob,
                                                          nodeView,
                                                          metadataFunction->moduleMetadataTokenRecords,
                                                          metadataFunction->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_TYPE_REF);
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            return metadata_runtime_find_type_node_record(runtime,
                                                          blob,
                                                          nodeView,
                                                          metadataFunction->metadataTokenRecords,
                                                          metadataFunction->metadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_TYPE_SPEC);
        default:
            return ZR_NULL;
    }
}
