#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

#define ZR_METADATA_RUNTIME_SIGNATURE_MAX_RECURSION_DEPTH 64u

static TZrBool metadata_runtime_is_method_record_token(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return table == ZR_METADATA_TABLE_MEMBER_DEF || table == ZR_METADATA_TABLE_MEMBER_REF;
}

static TZrBool metadata_runtime_is_type_record_token(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return table == ZR_METADATA_TABLE_TYPE_DEF ||
           table == ZR_METADATA_TABLE_TYPE_REF ||
           table == ZR_METADATA_TABLE_TYPE_SPEC;
}

static const SZrMetadataTokenRecord *metadata_runtime_find_attached_record(SZrMetadataRuntime *runtime,
                                                                           TZrMetadataToken token,
                                                                           TZrUInt32 localTable) {
    if (runtime->metadataFunction == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZR_METADATA_TOKEN_TABLE(token) == localTable) {
        return ZrCore_Function_FindMetadataTokenRecord(runtime->metadataFunction, token);
    }

    return ZrCore_Function_FindModuleMetadataTokenRecord(runtime->metadataFunction, token);
}

static const SZrMetadataTokenRecord *metadata_runtime_find_any_attached_record(SZrMetadataRuntime *runtime,
                                                                               TZrMetadataToken token) {
    const SZrMetadataTokenRecord *record;

    if (runtime == ZR_NULL || runtime->metadataFunction == ZR_NULL || token == 0u) {
        return ZR_NULL;
    }

    record = ZrCore_Function_FindMetadataTokenRecord(runtime->metadataFunction, token);
    if (record != ZR_NULL) {
        return record;
    }

    return ZrCore_Function_FindModuleMetadataTokenRecord(runtime->metadataFunction, token);
}

static const SZrTypeLayout *metadata_runtime_get_type_layout(SZrMetadataRuntime *runtime,
                                                             TZrUInt32 typeLayoutId) {
    if (runtime == ZR_NULL ||
        runtime->codeRegistration == ZR_NULL ||
        runtime->codeRegistration->typeLayouts == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= runtime->typeLayoutCount) {
        return ZR_NULL;
    }

    return runtime->codeRegistration->typeLayouts[typeLayoutId];
}

static const SZrAotGcDescriptor *metadata_runtime_get_gc_descriptor(SZrMetadataRuntime *runtime,
                                                                    TZrUInt32 typeLayoutId) {
    if (runtime == ZR_NULL ||
        runtime->codeRegistration == ZR_NULL ||
        runtime->codeRegistration->gcDescriptors == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= runtime->gcDescriptorCount) {
        return ZR_NULL;
    }

    return runtime->codeRegistration->gcDescriptors[typeLayoutId];
}

static TZrBool metadata_runtime_signature_read_u8(const TZrByte *buffer,
                                                  TZrSize bufferLength,
                                                  TZrSize *offset,
                                                  TZrUInt8 *outValue) {
    if (buffer == ZR_NULL || offset == ZR_NULL || outValue == ZR_NULL || *offset >= bufferLength) {
        return ZR_FALSE;
    }

    *outValue = buffer[*offset];
    *offset += 1u;
    return ZR_TRUE;
}

static TZrBool metadata_runtime_signature_skip_u32(const TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   TZrSize *offset) {
    if (buffer == ZR_NULL || offset == ZR_NULL || *offset > bufferLength || bufferLength - *offset < 4u) {
        return ZR_FALSE;
    }

    *offset += 4u;
    return ZR_TRUE;
}

static TZrBool metadata_runtime_signature_read_u32(const TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   TZrSize *offset,
                                                   TZrUInt32 *outValue) {
    if (!metadata_runtime_signature_skip_u32(buffer, bufferLength, offset)) {
        return ZR_FALSE;
    }

    *outValue = ((TZrUInt32)buffer[*offset - 4u]) |
                ((TZrUInt32)buffer[*offset - 3u] << 8u) |
                ((TZrUInt32)buffer[*offset - 2u] << 16u) |
                ((TZrUInt32)buffer[*offset - 1u] << 24u);
    return ZR_TRUE;
}

static TZrBool metadata_runtime_signature_skip_type_node(const TZrByte *signatureBlob,
                                                         TZrSize signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 depth);

static TZrBool metadata_runtime_signature_skip_type_list(const TZrByte *signatureBlob,
                                                         TZrSize signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 count,
                                                         TZrUInt32 depth) {
    TZrUInt32 index;

    for (index = 0u; index < count; ++index) {
        if (!metadata_runtime_signature_skip_type_node(signatureBlob, signatureBlobLength, offset, depth + 1u)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool metadata_runtime_signature_skip_type_node(const TZrByte *signatureBlob,
                                                         TZrSize signatureBlobLength,
                                                         TZrSize *offset,
                                                         TZrUInt32 depth) {
    TZrUInt8 node;
    TZrUInt32 count;

    if (depth > ZR_METADATA_RUNTIME_SIGNATURE_MAX_RECURSION_DEPTH ||
        !metadata_runtime_signature_read_u8(signatureBlob, signatureBlobLength, offset, &node)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            return metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            return metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            return metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   metadata_runtime_signature_skip_type_node(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            return metadata_runtime_signature_read_u32(signatureBlob, signatureBlobLength, offset, &count) &&
                   metadata_runtime_signature_skip_type_list(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            count,
                                                            depth);
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            if (!metadata_runtime_signature_skip_type_node(signatureBlob, signatureBlobLength, offset, depth + 1u) ||
                !metadata_runtime_signature_read_u32(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return metadata_runtime_signature_skip_type_list(signatureBlob, signatureBlobLength, offset, count, depth);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            return metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   metadata_runtime_signature_skip_type_node(signatureBlob,
                                                            signatureBlobLength,
                                                            offset,
                                                            depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset) ||
                !metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset) ||
                !metadata_runtime_signature_read_u32(signatureBlob, signatureBlobLength, offset, &count)) {
                return ZR_FALSE;
            }
            return metadata_runtime_signature_skip_type_list(signatureBlob, signatureBlobLength, offset, count, depth);
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return metadata_runtime_signature_skip_type_node(signatureBlob, signatureBlobLength, offset, depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
            return metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            return metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset) &&
                   metadata_runtime_signature_skip_u32(signatureBlob, signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrCore_MetadataRuntime_AttachZrpMetadata(SZrMetadataRuntime *runtime,
                                                 const TZrByte *buffer,
                                                 TZrSize bufferLength) {
    SZrZrpMetadataHeader header;

    if (runtime == ZR_NULL || buffer == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_ZrpMetadata_ReadHeader(buffer, bufferLength, &header) ||
        !ZrCore_ZrpMetadata_ValidateDefinitionTables(buffer, bufferLength, &header)) {
        return ZR_FALSE;
    }

    runtime->zrpMetadataBuffer = buffer;
    runtime->zrpMetadataBufferLength = bufferLength;
    runtime->zrpMetadataHeader = header;
    runtime->hasZrpMetadata = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_GetZrpSectionView(SZrMetadataRuntime *runtime,
                                                 EZrZrpMetadataSectionKind sectionKind,
                                                 SZrZrpMetadataSectionView *outView) {
    if (runtime == ZR_NULL || !runtime->hasZrpMetadata || outView == ZR_NULL) {
        if (outView != ZR_NULL) {
            ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
        }
        return ZR_FALSE;
    }

    return ZrCore_ZrpMetadata_GetSectionView(runtime->zrpMetadataBuffer,
                                             runtime->zrpMetadataBufferLength,
                                             &runtime->zrpMetadataHeader,
                                             sectionKind,
                                             outView);
}

static TZrBool metadata_runtime_get_signature_record_blob(SZrMetadataRuntime *runtime,
                                                          const SZrMetadataTokenRecord *signatureRecord,
                                                          SZrZrpMetadataPoolSliceView *outSlice) {
    if (outSlice != ZR_NULL) {
        ZrCore_Memory_RawSet(outSlice, 0, sizeof(*outSlice));
    }
    if (runtime == ZR_NULL ||
        !runtime->hasZrpMetadata ||
        signatureRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(signatureRecord->token) != ZR_METADATA_TABLE_SIGNATURE ||
        signatureRecord->signatureBlobLength == 0u ||
        outSlice == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_ZrpMetadata_GetPoolSlice(runtime->zrpMetadataBuffer,
                                         runtime->zrpMetadataBufferLength,
                                         &runtime->zrpMetadataHeader,
                                         ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                         signatureRecord->signatureBlobOffset,
                                         signatureRecord->signatureBlobLength,
                                         outSlice)) {
        return ZR_FALSE;
    }
    if (!ZrCore_ZrpMetadata_ValidateSignatureBlob(outSlice->data, outSlice->byteLength)) {
        ZrCore_Memory_RawSet(outSlice, 0, sizeof(*outSlice));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_GetSignatureBlob(SZrMetadataRuntime *runtime,
                                                TZrMetadataToken entityToken,
                                                SZrZrpMetadataPoolSliceView *outSlice) {
    const SZrMetadataTokenRecord *signatureRecord;

    if (outSlice != ZR_NULL) {
        ZrCore_Memory_RawSet(outSlice, 0, sizeof(*outSlice));
    }
    if (runtime == ZR_NULL || !runtime->hasZrpMetadata || outSlice == ZR_NULL) {
        return ZR_FALSE;
    }

    signatureRecord = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, entityToken);
    return metadata_runtime_get_signature_record_blob(runtime, signatureRecord, outSlice);
}

TZrBool ZrCore_MetadataRuntime_ReadSignatureTypeNode(
        const SZrZrpMetadataPoolSliceView *blob,
        TZrUInt32 blobOffset,
        SZrMetadataRuntimeSignatureTypeNodeView *outView) {
    TZrSize offset;
    TZrUInt8 node;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (blob == ZR_NULL ||
        blob->data == ZR_NULL ||
        outView == ZR_NULL ||
        (TZrSize)blobOffset >= blob->byteLength) {
        return ZR_FALSE;
    }

    offset = (TZrSize)blobOffset;
    if (!metadata_runtime_signature_read_u8(blob->data, blob->byteLength, &offset, &node)) {
        return ZR_FALSE;
    }

    outView->node = (EZrMetadataSignatureNode)node;
    outView->blobOffset = blobOffset;

    switch (outView->node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0) ||
                !metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload1)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->baseTypeBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_node(blob->data, blob->byteLength, &offset, 0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->childCount)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->childListBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_list(blob->data,
                                                           blob->byteLength,
                                                           &offset,
                                                           outView->childCount,
                                                           0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            outView->baseTypeBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_node(blob->data, blob->byteLength, &offset, 0u) ||
                !metadata_runtime_signature_read_u32(blob->data,
                                                     blob->byteLength,
                                                     &offset,
                                                     &outView->childCount)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->childListBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_list(blob->data,
                                                           blob->byteLength,
                                                           &offset,
                                                           outView->childCount,
                                                           0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->baseTypeBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_node(blob->data, blob->byteLength, &offset, 0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0) ||
                !metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload1) ||
                !metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->childCount)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->childListBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_list(blob->data,
                                                           blob->byteLength,
                                                           &offset,
                                                           outView->childCount,
                                                           0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            outView->baseTypeBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_node(blob->data, blob->byteLength, &offset, 0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            if (!metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload0) ||
                !metadata_runtime_signature_read_u32(blob->data, blob->byteLength, &offset, &outView->payload1)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            break;

        case ZR_METADATA_SIGNATURE_NODE_FUNC:
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
            return ZR_FALSE;
    }

    outView->nextBlobOffset = (TZrUInt32)offset;
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecSignatureView *outView) {
    const SZrMetadataTokenRecord *typeRecord;
    const SZrMetadataTokenRecord *signatureRecord;
    SZrZrpMetadataPoolSliceView blob;
    SZrMetadataRuntimeSignatureTypeNodeView genericInstanceNode;
    SZrMetadataRuntimeSignatureTypeNodeView baseTypeNode;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }

    typeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, typeSpecToken);
    signatureRecord = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, typeSpecToken);
    if (typeRecord == ZR_NULL ||
        signatureRecord == ZR_NULL ||
        typeRecord->relatedToken != signatureRecord->token ||
        signatureRecord->relatedToken != typeSpecToken ||
        !ZrCore_MetadataRuntime_GetSignatureBlob(runtime, typeSpecToken, &blob) ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&blob, 0u, &genericInstanceNode) ||
        genericInstanceNode.node != ZR_METADATA_SIGNATURE_NODE_GENERIC_INST ||
        genericInstanceNode.nextBlobOffset != (TZrUInt32)blob.byteLength ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&blob,
                                                     genericInstanceNode.baseTypeBlobOffset,
                                                     &baseTypeNode) ||
        (baseTypeNode.node != ZR_METADATA_SIGNATURE_NODE_TYPE_REF &&
         baseTypeNode.node != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF)) {
        return ZR_FALSE;
    }

    outView->typeSpecToken = typeSpecToken;
    outView->signatureToken = signatureRecord->token;
    outView->signatureHash = signatureRecord->signatureHash;
    outView->blob = blob;
    outView->genericInstanceNode = genericInstanceNode;
    outView->baseTypeNode = baseTypeNode;
    outView->argumentCount = genericInstanceNode.childCount;
    outView->argumentListBlobOffset = genericInstanceNode.childListBlobOffset;
    return ZR_TRUE;
}

static TZrBool metadata_runtime_type_node_matches_record_signature(
        SZrMetadataRuntime *runtime,
        const SZrMetadataTokenRecord *record,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView) {
    SZrZrpMetadataPoolSliceView blob;
    SZrMetadataRuntimeSignatureTypeNodeView recordNodeView;

    if (record == ZR_NULL ||
        nodeView == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetSignatureBlob(runtime, record->token, &blob) ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&blob, 0u, &recordNodeView)) {
        return ZR_FALSE;
    }

    return recordNodeView.node == nodeView->node &&
           recordNodeView.payload0 == nodeView->payload0 &&
           recordNodeView.payload1 == nodeView->payload1 &&
           recordNodeView.nextBlobOffset == (TZrUInt32)blob.byteLength;
}

static const SZrMetadataTokenRecord *metadata_runtime_find_type_record_by_node(
        SZrMetadataRuntime *runtime,
        const SZrMetadataRuntimeSignatureTypeNodeView *baseTypeNode) {
    const SZrMetadataTokenRecord *records;
    TZrUInt32 recordLength;
    TZrUInt32 table;
    TZrUInt32 index;

    if (runtime == ZR_NULL || runtime->metadataFunction == ZR_NULL || baseTypeNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (baseTypeNode->node == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
        records = runtime->metadataFunction->metadataTokenRecords;
        recordLength = runtime->metadataFunction->metadataTokenRecordLength;
        table = ZR_METADATA_TABLE_TYPE_DEF;
    } else if (baseTypeNode->node == ZR_METADATA_SIGNATURE_NODE_TYPE_REF) {
        records = runtime->metadataFunction->moduleMetadataTokenRecords;
        recordLength = runtime->metadataFunction->moduleMetadataTokenRecordLength;
        table = ZR_METADATA_TABLE_TYPE_REF;
    } else {
        return ZR_NULL;
    }

    if (records == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < recordLength; ++index) {
        const SZrMetadataTokenRecord *record = &records[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) == table &&
            metadata_runtime_type_node_matches_record_signature(runtime, record, baseTypeNode)) {
            return record;
        }
    }

    return ZR_NULL;
}

TZrBool ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecGenericBindingView *outView) {
    SZrMetadataRuntimeTypeSpecSignatureView signatureView;
    const SZrMetadataTokenRecord *baseRecord;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(runtime, typeSpecToken, &signatureView)) {
        return ZR_FALSE;
    }

    baseRecord = metadata_runtime_find_type_record_by_node(runtime, &signatureView.baseTypeNode);
    if (baseRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->signatureView = signatureView;
    outView->baseToken = baseRecord->token;
    outView->baseRecord = baseRecord;
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex,
        SZrMetadataRuntimeTypeSpecGenericArgumentView *outView) {
    SZrMetadataRuntimeTypeSpecGenericBindingView bindingView;
    SZrMetadataRuntimeSignatureTypeNodeView argumentNode;
    const SZrMetadataTokenRecord *argumentRecord;
    TZrUInt32 index;
    TZrUInt32 argumentBlobOffset;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, typeSpecToken, &bindingView) ||
        argumentIndex >= bindingView.signatureView.argumentCount) {
        return ZR_FALSE;
    }

    argumentBlobOffset = bindingView.signatureView.argumentListBlobOffset;
    for (index = 0u; index <= argumentIndex; ++index) {
        if (!ZrCore_MetadataRuntime_ReadSignatureTypeNode(&bindingView.signatureView.blob,
                                                         argumentBlobOffset,
                                                         &argumentNode)) {
            return ZR_FALSE;
        }
        if (index < argumentIndex) {
            argumentBlobOffset = argumentNode.nextBlobOffset;
        }
    }

    argumentRecord = ZR_NULL;
    if (argumentNode.node == ZR_METADATA_SIGNATURE_NODE_TYPE_REF ||
        argumentNode.node == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
        argumentRecord = metadata_runtime_find_type_record_by_node(runtime, &argumentNode);
        if (argumentRecord == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    outView->bindingView = bindingView;
    outView->argumentNode = argumentNode;
    outView->argumentIndex = argumentIndex;
    if (argumentRecord != ZR_NULL) {
        outView->argumentToken = argumentRecord->token;
        outView->argumentRecord = argumentRecord;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        SZrMetadataRuntimeMethodSpecSignatureView *outView) {
    const SZrMetadataTokenRecord *methodSpecRecord;
    const SZrMetadataTokenRecord *methodRecord;
    SZrZrpMetadataPoolSliceView blob;
    SZrMetadataRuntimeSignatureTypeNodeView genericInstanceNode;
    SZrMetadataRuntimeSignatureTypeNodeView methodNode;
    TZrMetadataToken methodToken;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(methodSpecToken) != ZR_METADATA_TABLE_SIGNATURE) {
        return ZR_FALSE;
    }

    methodSpecRecord = metadata_runtime_find_any_attached_record(runtime, methodSpecToken);
    if (methodSpecRecord == ZR_NULL ||
        methodSpecRecord->token != methodSpecToken ||
        !metadata_runtime_is_method_record_token(methodSpecRecord->relatedToken) ||
        methodSpecRecord->ownerToken != methodSpecRecord->relatedToken) {
        return ZR_FALSE;
    }

    methodToken = methodSpecRecord->relatedToken;
    methodRecord = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, methodToken);
    if (methodRecord == ZR_NULL ||
        !metadata_runtime_get_signature_record_blob(runtime, methodSpecRecord, &blob) ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&blob, 0u, &genericInstanceNode) ||
        genericInstanceNode.node != ZR_METADATA_SIGNATURE_NODE_GENERIC_INST ||
        genericInstanceNode.nextBlobOffset != (TZrUInt32)blob.byteLength ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&blob,
                                                     genericInstanceNode.baseTypeBlobOffset,
                                                     &methodNode) ||
        methodNode.node != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
        methodNode.payload0 != methodToken) {
        return ZR_FALSE;
    }

    outView->methodSpecToken = methodSpecToken;
    outView->methodToken = methodToken;
    outView->signatureHash = methodSpecRecord->signatureHash;
    outView->blob = blob;
    outView->genericInstanceNode = genericInstanceNode;
    outView->methodNode = methodNode;
    outView->methodRecord = methodRecord;
    outView->argumentCount = genericInstanceNode.childCount;
    outView->argumentListBlobOffset = genericInstanceNode.childListBlobOffset;
    return ZR_TRUE;
}

const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveTypeLayout(SZrMetadataRuntime *runtime,
                                                              TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *layout = metadata_runtime_get_type_layout(runtime, typeLayoutId);

    if (layout == ZR_NULL || layout->cTypeId != typeLayoutId) {
        return ZR_NULL;
    }

    return layout;
}

const SZrAotGcDescriptor *ZrCore_MetadataRuntime_ResolveGcDescriptor(SZrMetadataRuntime *runtime,
                                                                     TZrUInt32 typeLayoutId) {
    const SZrAotGcDescriptor *descriptor = metadata_runtime_get_gc_descriptor(runtime, typeLayoutId);

    if (descriptor == ZR_NULL ||
        descriptor->typeLayoutId != typeLayoutId ||
        ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId) == ZR_NULL) {
        return ZR_NULL;
    }

    return descriptor;
}

void ZrCore_MetadataRuntime_AttachFunction(SZrMetadataRuntime *runtime, SZrFunction *function) {
    if (runtime == ZR_NULL || function == ZR_NULL) {
        return;
    }

    function->metadataCodeRegistration = runtime->codeRegistration;
    function->metadataTypeLayoutCount = runtime->typeLayoutCount;
    function->metadataGcDescriptorCount = runtime->gcDescriptorCount;
}

static TZrBool metadata_runtime_function_has_prototype_instances(const SZrFunction *function) {
    return function != ZR_NULL &&
           function->prototypeInstances != ZR_NULL &&
           function->prototypeInstancesLength > 0u &&
           function->prototypeCount > 0u;
}

static const SZrFunction *metadata_runtime_select_prototype_context_function(const SZrFunction *function) {
    if (metadata_runtime_function_has_prototype_instances(function)) {
        return function;
    }

    if (function != ZR_NULL &&
        metadata_runtime_function_has_prototype_instances(function->prototypeContextFunction)) {
        return function->prototypeContextFunction;
    }

    return ZR_NULL;
}

static TZrBool metadata_runtime_find_prototype_type_layout_id(const SZrFunction *function,
                                                              const struct SZrObjectPrototype *prototype,
                                                              TZrUInt32 *outTypeLayoutId) {
    const SZrFunction *prototypeContextFunction = metadata_runtime_select_prototype_context_function(function);
    TZrUInt32 prototypeLimit;

    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    if (prototypeContextFunction == ZR_NULL || prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    prototypeLimit = prototypeContextFunction->prototypeInstancesLength;
    if (prototypeLimit > prototypeContextFunction->prototypeCount) {
        prototypeLimit = prototypeContextFunction->prototypeCount;
    }

    for (TZrUInt32 index = 0u; index < prototypeLimit; ++index) {
        if (prototypeContextFunction->prototypeInstances[index] == prototype) {
            if (outTypeLayoutId != ZR_NULL) {
                *outTypeLayoutId = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(const SZrFunction *function,
                                                                      TZrUInt32 typeLayoutId) {
    const SZrFunction *registryFunction = function;
    SZrMetadataRuntime runtimeView;

    if (registryFunction != ZR_NULL &&
        registryFunction->metadataCodeRegistration == ZR_NULL &&
        registryFunction->prototypeContextFunction != ZR_NULL) {
        registryFunction = registryFunction->prototypeContextFunction;
    }
    if (registryFunction == ZR_NULL || registryFunction->metadataCodeRegistration == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(&runtimeView, 0, sizeof(runtimeView));
    runtimeView.metadataFunction = (SZrFunction *)registryFunction;
    runtimeView.codeRegistration = registryFunction->metadataCodeRegistration;
    runtimeView.typeLayoutCount = registryFunction->metadataTypeLayoutCount;
    runtimeView.gcDescriptorCount = registryFunction->metadataGcDescriptorCount;
    return ZrCore_MetadataRuntime_ResolveTypeLayout(&runtimeView, typeLayoutId);
}

const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(const SZrFunction *function,
                                                                               const struct SZrObjectPrototype *prototype,
                                                                               TZrUInt32 *outTypeLayoutId) {
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    const SZrTypeLayout *typeLayout;

    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    if (!metadata_runtime_find_prototype_type_layout_id(function, prototype, &typeLayoutId)) {
        return ZR_NULL;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(function, typeLayoutId);
    if (typeLayout == ZR_NULL) {
        return ZR_NULL;
    }

    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = typeLayoutId;
    }
    return typeLayout;
}

TZrBool ZrCore_MetadataRuntime_ReadSignatureView(SZrMetadataRuntime *runtime,
                                                 TZrMetadataToken entityToken,
                                                 SZrMetadataRuntimeSignatureView *outView) {
    SZrZrpMetadataPoolSliceView blob;
    TZrSize offset;
    TZrUInt8 rootNode;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetSignatureBlob(runtime, entityToken, &blob)) {
        return ZR_FALSE;
    }

    offset = 0u;
    if (!metadata_runtime_signature_read_u8(blob.data, blob.byteLength, &offset, &rootNode)) {
        return ZR_FALSE;
    }

    outView->rootNode = (EZrMetadataSignatureNode)rootNode;
    outView->blob = blob;

    switch (outView->rootNode) {
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
            if (!metadata_runtime_signature_read_u8(blob.data,
                                                    blob.byteLength,
                                                    &offset,
                                                    &outView->callingConvention) ||
                !metadata_runtime_signature_read_u8(blob.data, blob.byteLength, &offset, &outView->flags) ||
                !metadata_runtime_signature_read_u32(blob.data,
                                                     blob.byteLength,
                                                     &offset,
                                                     &outView->genericParameterCount)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->returnTypeBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_node(blob.data, blob.byteLength, &offset, 0u) ||
                !metadata_runtime_signature_read_u32(blob.data,
                                                     blob.byteLength,
                                                     &offset,
                                                     &outView->parameterCount)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->parameterListBlobOffset = (TZrUInt32)offset;
            return ZR_TRUE;

        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
            if (!metadata_runtime_signature_read_u8(blob.data, blob.byteLength, &offset, &outView->flags)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            outView->fieldTypeBlobOffset = (TZrUInt32)offset;
            if (!metadata_runtime_signature_skip_type_node(blob.data, blob.byteLength, &offset, 0u)) {
                ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
                return ZR_FALSE;
            }
            return ZR_TRUE;

        default:
            ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
            return ZR_FALSE;
    }
}

const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveMethodRecord(SZrMetadataRuntime *runtime,
                                                                         TZrMetadataToken token) {
    const SZrMetadataTokenRecord *record;

    if (runtime == ZR_NULL || token == 0u || !metadata_runtime_is_method_record_token(token)) {
        return ZR_NULL;
    }

    if (runtime->methodRecordCacheToken == token) {
        return runtime->methodRecordCache;
    }

    record = metadata_runtime_find_attached_record(runtime, token, ZR_METADATA_TABLE_MEMBER_DEF);

    if (record == ZR_NULL) {
        return ZR_NULL;
    }

    runtime->methodRecordCacheToken = token;
    runtime->methodRecordCache = record;
    return record;
}

const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveFieldRecord(SZrMetadataRuntime *runtime,
                                                                        TZrMetadataToken token) {
    const SZrMetadataTokenRecord *record;

    if (runtime == ZR_NULL || token == 0u || !metadata_runtime_is_method_record_token(token)) {
        return ZR_NULL;
    }

    if (runtime->fieldRecordCacheToken == token) {
        return runtime->fieldRecordCache;
    }

    record = metadata_runtime_find_attached_record(runtime, token, ZR_METADATA_TABLE_MEMBER_DEF);

    if (record == ZR_NULL) {
        return ZR_NULL;
    }

    runtime->fieldRecordCacheToken = token;
    runtime->fieldRecordCache = record;
    return record;
}

const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveTypeRecord(SZrMetadataRuntime *runtime,
                                                                       TZrMetadataToken token) {
    const SZrMetadataTokenRecord *record;

    if (runtime == ZR_NULL || token == 0u || !metadata_runtime_is_type_record_token(token)) {
        return ZR_NULL;
    }

    if (runtime->typeRecordCacheToken == token) {
        return runtime->typeRecordCache;
    }

    if (ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_SPEC) {
        record = ZrCore_Function_FindMetadataTokenRecord(runtime->metadataFunction, token);
    } else {
        record = metadata_runtime_find_attached_record(runtime, token, ZR_METADATA_TABLE_TYPE_DEF);
    }

    if (record == ZR_NULL) {
        return ZR_NULL;
    }

    runtime->typeRecordCacheToken = token;
    runtime->typeRecordCache = record;
    return record;
}

const SZrMetadataTokenRecord *ZrCore_MetadataRuntime_ResolveSignatureRecord(SZrMetadataRuntime *runtime,
                                                                            TZrMetadataToken entityToken) {
    const SZrMetadataTokenRecord *record;

    if (runtime == ZR_NULL || entityToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(entityToken) == ZR_METADATA_TABLE_SIGNATURE) {
        return ZR_NULL;
    }

    if (runtime->signatureRecordCacheEntityToken == entityToken) {
        return runtime->signatureRecordCache;
    }

    if (runtime->metadataFunction == ZR_NULL) {
        return ZR_NULL;
    }

    record = ZrCore_Function_FindMetadataSignatureRecord(runtime->metadataFunction, entityToken);
    if (record == ZR_NULL) {
        record = ZrCore_Function_FindModuleMetadataSignatureRecord(runtime->metadataFunction, entityToken);
    }
    if (record == ZR_NULL) {
        return ZR_NULL;
    }

    runtime->signatureRecordCacheEntityToken = entityToken;
    runtime->signatureRecordCache = record;
    return record;
}
