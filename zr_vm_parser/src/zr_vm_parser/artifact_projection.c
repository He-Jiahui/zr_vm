#include "zr_vm_parser/artifact_projection.h"

#include <string.h>

#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/semantic.h"

typedef struct SZrArtifactTypeWriter {
    const SZrSemanticContext *context;
    TZrByte *buffer;
    TZrSize capacity;
    TZrSize length;
    SZrArtifactDiagnostic *diagnostic;
} SZrArtifactTypeWriter;

typedef struct SZrArtifactTypeReader {
    SZrSemanticContext *context;
    SZrString *moduleIdentity;
    const TZrByte *signature;
    TZrSize length;
    TZrSize offset;
    SZrArtifactDiagnostic *diagnostic;
} SZrArtifactTypeReader;

static EZrArtifactStatus artifact_layout_projection_fail(
        SZrArtifactDiagnostic *diagnostic,
        EZrArtifactStatus status,
        TZrUInt64 expectedHash,
        TZrUInt64 actualHash) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_LAYOUT_TABLE;
        diagnostic->expectedHash = expectedHash;
        diagnostic->actualHash = actualHash;
    }
    return status;
}

EZrArtifactStatus ZrParser_ArtifactLayout_ApplyNativeCapabilities(
        const ZrLibTypeDescriptor *typeDescriptor,
        TZrUInt64 stableSlotContractHash,
        SZrArtifactLayoutRow *layout,
        SZrArtifactDiagnostic *diagnostic) {
    TZrBool stableSlotSource;

    if (typeDescriptor == ZR_NULL || layout == ZR_NULL) {
        return artifact_layout_projection_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                0u,
                0u);
    }
    stableSlotSource = (TZrBool)(
            (typeDescriptor->protocolMask &
             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE)) != 0u);
    if (!stableSlotSource) {
        if (stableSlotContractHash != 0u ||
            (layout->capabilityFlags &
             ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE) != 0u ||
            layout->stableSlotContractHash != 0u) {
            return artifact_layout_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    0u,
                    stableSlotContractHash);
        }
    } else {
        if (stableSlotContractHash == 0u) {
            return artifact_layout_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    1u,
                    0u);
        }
        if (layout->stableSlotContractHash != 0u &&
            layout->stableSlotContractHash !=
                    stableSlotContractHash) {
            return artifact_layout_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
                    stableSlotContractHash,
                    layout->stableSlotContractHash);
        }
        layout->capabilityFlags |=
                ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE;
        layout->stableSlotContractHash = stableSlotContractHash;
    }
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_ARTIFACT_STATUS_OK;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_projection_fail(SZrArtifactDiagnostic *diagnostic,
                                                  EZrArtifactStatus status,
                                                  TZrSize offset) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_SIGNATURE_HEAP;
        diagnostic->byteOffset = (TZrUInt32)offset;
    }
    return status;
}

static EZrArtifactStatus artifact_type_write_bytes(SZrArtifactTypeWriter *writer,
                                                   const TZrByte *bytes,
                                                   TZrSize length) {
    if (writer->length + length > writer->capacity) {
        return artifact_projection_fail(writer->diagnostic,
                                        ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                                        writer->length);
    }
    if (length > 0u) {
        memcpy(writer->buffer + writer->length, bytes, length);
        writer->length += length;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_write_u8(SZrArtifactTypeWriter *writer, TZrUInt8 value) {
    return artifact_type_write_bytes(writer, &value, 1u);
}

static EZrArtifactStatus artifact_type_write_u32(SZrArtifactTypeWriter *writer, TZrUInt32 value) {
    TZrByte bytes[4];
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
    return artifact_type_write_bytes(writer, bytes, sizeof(bytes));
}

static EZrArtifactStatus artifact_type_write_u64(SZrArtifactTypeWriter *writer, TZrUInt64 value) {
    TZrByte bytes[8];
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        bytes[index] = (TZrByte)(value & 0xffu);
        value >>= 8u;
    }
    return artifact_type_write_bytes(writer, bytes, sizeof(bytes));
}

static EZrArtifactStatus artifact_type_write_node(SZrArtifactTypeWriter *writer,
                                                   TZrTypeId typeId,
                                                   TZrUInt32 depth);

static TZrUInt8 artifact_type_ref_export_effect(
        const SZrSemanticContext *context,
        TZrTypeId returnTypeId) {
    const SZrCanonicalTypeNode *returnType =
            ZrParser_CanonicalType_Find(context, returnTypeId);

    if (returnType == ZR_NULL || returnType->kind != ZR_CANONICAL_TYPE_REF) {
        return (TZrUInt8)ZR_ARTIFACT_REF_EXPORT_NONE;
    }
    return returnType->data.refType.access == ZR_CANONICAL_REF_READONLY
                   ? (TZrUInt8)ZR_ARTIFACT_REF_EXPORT_READONLY
                   : (TZrUInt8)ZR_ARTIFACT_REF_EXPORT_WRITABLE;
}

static EZrArtifactStatus artifact_type_write_type_id_array(SZrArtifactTypeWriter *writer,
                                                           const SZrArray *typeIds,
                                                           TZrUInt32 depth) {
    TZrSize index;
    EZrArtifactStatus status = artifact_type_write_u32(writer, (TZrUInt32)typeIds->length);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    for (index = 0u; index < typeIds->length; ++index) {
        const TZrTypeId *typeId = (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)typeIds, index);
        if (typeId == ZR_NULL) {
            return artifact_projection_fail(writer->diagnostic,
                                            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                            writer->length);
        }
        status = artifact_type_write_node(writer, *typeId, depth + 1u);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_write_generic_arguments(SZrArtifactTypeWriter *writer,
                                                               const SZrArray *arguments,
                                                               TZrUInt32 depth) {
    TZrSize index;
    EZrArtifactStatus status = artifact_type_write_u32(writer, (TZrUInt32)arguments->length);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    for (index = 0u; index < arguments->length; ++index) {
        const SZrCanonicalGenericArgument *argument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get((SZrArray *)arguments, index);
        if (argument == ZR_NULL) {
            return artifact_projection_fail(writer->diagnostic,
                                            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                            writer->length);
        }
        switch (argument->kind) {
            case ZR_CANONICAL_GENERIC_ARGUMENT_TYPE:
                status = artifact_type_write_node(writer, argument->data.typeId, depth + 1u);
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT:
                status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_CONST_INT);
                if (status == ZR_ARTIFACT_STATUS_OK) {
                    status = artifact_type_write_u64(writer, (TZrUInt64)argument->data.constIntValue);
                }
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER:
                status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_CONST_PARAMETER);
                if (status == ZR_ARTIFACT_STATUS_OK) {
                    status = artifact_type_write_u32(writer,
                                                     argument->data.constParameter.ownerSymbolId);
                }
                if (status == ZR_ARTIFACT_STATUS_OK) {
                    status = artifact_type_write_u32(writer,
                                                     argument->data.constParameter.ordinal);
                }
                break;
            default:
                status = artifact_projection_fail(writer->diagnostic,
                                                  ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                                  writer->length);
                break;
        }
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_write_function(SZrArtifactTypeWriter *writer,
                                                      const SZrCanonicalFunctionType *function,
                                                      TZrUInt32 depth) {
    TZrSize index;
    EZrArtifactStatus status;
    TZrUInt8 artifactEffectFlags = 0u;
    TZrUInt8 refExportEffect;

    if ((function->effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_THROWS) != 0u)
        artifactEffectFlags |= (TZrUInt8)ZR_ARTIFACT_CONTRACT_FLAG_THROWS;
    if ((function->effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0u)
        artifactEffectFlags |= (TZrUInt8)ZR_ARTIFACT_CONTRACT_FLAG_ASYNC;
    if ((function->effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR) != 0u)
        artifactEffectFlags |= (TZrUInt8)ZR_ARTIFACT_CONTRACT_FLAG_GENERATOR;
    refExportEffect = artifact_type_ref_export_effect(
            writer->context, function->returnTypeId);

    status = artifact_type_write_u8(writer, (TZrUInt8)function->receiverEffect);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_type_write_u8(writer, refExportEffect);
    if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_write_u8(writer, artifactEffectFlags);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_type_write_u32(writer, (TZrUInt32)function->parameterContracts.length);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_type_write_node(writer, function->returnTypeId, depth + 1u);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;

    for (index = 0u; index < function->parameterContracts.length; ++index) {
        const SZrCanonicalParameterContract *parameter =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&function->parameterContracts, index);
        if (parameter == ZR_NULL) {
            return artifact_projection_fail(writer->diagnostic,
                                            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                            writer->length);
        }
        status = artifact_type_write_u8(writer, (TZrUInt8)parameter->passingForm);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_write_u8(writer, (TZrUInt8)parameter->escapeUpperBound);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_write_u8(writer, (TZrUInt8)parameter->entryInitialization);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_write_u8(writer, (TZrUInt8)parameter->exitInitialization);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_write_u8(writer, parameter->acceptsTemporary ? 1u : 0u);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_write_u8(writer, (TZrUInt8)parameter->callSiteMarker);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_write_node(writer, parameter->typeId, depth + 1u);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_write_node(SZrArtifactTypeWriter *writer,
                                                  TZrTypeId typeId,
                                                  TZrUInt32 depth) {
    const SZrCanonicalTypeNode *node;
    EZrArtifactStatus status;

    if (depth > ZR_ARTIFACT_SIGNATURE_MAX_DEPTH) {
        return artifact_projection_fail(writer->diagnostic,
                                        ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                        writer->length);
    }
    node = ZrParser_CanonicalType_Find(writer->context, typeId);
    if (node == ZR_NULL) {
        return artifact_projection_fail(writer->diagnostic,
                                        ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                        writer->length);
    }

    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u32(writer, (TZrUInt32)node->data.primitive.valueType);
            return status;
        case ZR_CANONICAL_TYPE_NOMINAL:
            if (node->data.nominal.definitionToken == 0u) {
                return artifact_projection_fail(writer->diagnostic,
                                                ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                                                writer->length);
            }
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u32(writer, node->data.nominal.definitionToken);
            return status;
        case ZR_CANONICAL_TYPE_GENERIC_PARAMETER:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_PARAMETER);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u32(writer, node->data.genericParameter.ownerSymbolId);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u32(writer, node->data.genericParameter.ordinal);
            return status;
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_INSTANCE);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_node(writer,
                                                  node->data.genericInstance.definitionTypeId,
                                                  depth + 1u);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_generic_arguments(writer,
                                                               &node->data.genericInstance.arguments,
                                                               depth);
            return status;
        case ZR_CANONICAL_TYPE_ARRAY:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_ARRAY);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u32(writer, node->data.array.rank);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u8(writer, (TZrUInt8)node->data.array.storageKind);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_node(writer, node->data.array.elementTypeId, depth + 1u);
            return status;
        case ZR_CANONICAL_TYPE_TUPLE:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_TUPLE);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_type_id_array(writer, &node->data.typeList.elementTypeIds, depth);
            return status;
        case ZR_CANONICAL_TYPE_UNION:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_UNION);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_node(writer, node->data.unionType.definitionTypeId, depth + 1u);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_type_id_array(writer, &node->data.unionType.variantTypeIds, depth);
            return status;
        case ZR_CANONICAL_TYPE_NULLABLE:
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
            status = artifact_type_write_u8(
                    writer,
                    node->kind == ZR_CANONICAL_TYPE_NULLABLE
                            ? ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE
                            : ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_node(writer, node->data.target.targetTypeId, depth + 1u);
            return status;
        case ZR_CANONICAL_TYPE_REF:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_REF);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u8(writer, (TZrUInt8)node->data.refType.access);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_node(writer, node->data.refType.pointeeTypeId, depth + 1u);
            return status;
        case ZR_CANONICAL_TYPE_OWNER:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_OWNER);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_u8(writer, (TZrUInt8)node->data.owner.ownerKind);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_node(writer, node->data.owner.targetTypeId, depth + 1u);
            return status;
        case ZR_CANONICAL_TYPE_FUNCTION:
            status = artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_write_function(writer, &node->data.function, depth);
            return status;
        case ZR_CANONICAL_TYPE_NEVER:
            return artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_NEVER);
        case ZR_CANONICAL_TYPE_ERROR:
            return artifact_type_write_u8(writer, ZR_ARTIFACT_SIGNATURE_NODE_ERROR);
        default:
            return artifact_projection_fail(writer->diagnostic,
                                            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                            writer->length);
    }
}

static EZrArtifactStatus artifact_type_read_u8(SZrArtifactTypeReader *reader, TZrUInt8 *outValue) {
    if (reader->offset >= reader->length) {
        return artifact_projection_fail(reader->diagnostic,
                                        ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                                        reader->offset);
    }
    *outValue = reader->signature[reader->offset++];
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_read_u32(SZrArtifactTypeReader *reader, TZrUInt32 *outValue) {
    const TZrByte *bytes;
    if (reader->length - reader->offset < 4u) {
        return artifact_projection_fail(reader->diagnostic,
                                        ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                                        reader->offset);
    }
    bytes = reader->signature + reader->offset;
    *outValue = (TZrUInt32)bytes[0] | ((TZrUInt32)bytes[1] << 8u) |
                ((TZrUInt32)bytes[2] << 16u) | ((TZrUInt32)bytes[3] << 24u);
    reader->offset += 4u;
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_read_u64(SZrArtifactTypeReader *reader, TZrUInt64 *outValue) {
    TZrUInt32 low = 0u;
    TZrUInt32 high = 0u;
    EZrArtifactStatus status = artifact_type_read_u32(reader, &low);
    if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u32(reader, &high);
    if (status == ZR_ARTIFACT_STATUS_OK) *outValue = (TZrUInt64)low | ((TZrUInt64)high << 32u);
    return status;
}

static EZrArtifactStatus artifact_type_read_node(SZrArtifactTypeReader *reader,
                                                 TZrTypeId *outTypeId,
                                                 TZrUInt32 depth);

static EZrArtifactStatus artifact_type_read_type_list(SZrArtifactTypeReader *reader,
                                                      SZrArray *outTypes,
                                                      TZrUInt32 depth) {
    TZrUInt32 count;
    TZrUInt32 index;
    EZrArtifactStatus status = artifact_type_read_u32(reader, &count);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (count > ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT) {
        return artifact_projection_fail(reader->diagnostic,
                                        ZR_ARTIFACT_STATUS_COUNT_LIMIT,
                                        reader->offset);
    }
    ZrCore_Array_Init(reader->context->state,
                      outTypes,
                      sizeof(TZrTypeId),
                      count > 0u ? count : 1u);
    for (index = 0u; index < count; ++index) {
        TZrTypeId typeId;
        status = artifact_type_read_node(reader, &typeId, depth + 1u);
        if (status != ZR_ARTIFACT_STATUS_OK) {
            ZrCore_Array_Free(reader->context->state, outTypes);
            return status;
        }
        ZrCore_Array_Push(reader->context->state, outTypes, &typeId);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_read_generic_arguments(SZrArtifactTypeReader *reader,
                                                              SZrArray *outArguments,
                                                              TZrUInt32 depth) {
    TZrUInt32 count;
    TZrUInt32 index;
    EZrArtifactStatus status = artifact_type_read_u32(reader, &count);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (count > ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT) {
        return artifact_projection_fail(reader->diagnostic,
                                        ZR_ARTIFACT_STATUS_COUNT_LIMIT,
                                        reader->offset);
    }
    ZrCore_Array_Init(reader->context->state,
                      outArguments,
                      sizeof(SZrCanonicalGenericArgument),
                      count > 0u ? count : 1u);
    for (index = 0u; index < count; ++index) {
        SZrCanonicalGenericArgument argument;
        TZrUInt8 node = reader->signature[reader->offset];
        memset(&argument, 0, sizeof(argument));
        if (node == ZR_ARTIFACT_SIGNATURE_NODE_CONST_INT) {
            TZrUInt64 value;
            ++reader->offset;
            argument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;
            status = artifact_type_read_u64(reader, &value);
            argument.data.constIntValue = (TZrInt64)value;
        } else if (node == ZR_ARTIFACT_SIGNATURE_NODE_CONST_PARAMETER) {
            ++reader->offset;
            argument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
            status = artifact_type_read_u32(reader, &argument.data.constParameter.ownerSymbolId);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_read_u32(reader, &argument.data.constParameter.ordinal);
        } else {
            argument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
            status = artifact_type_read_node(reader, &argument.data.typeId, depth + 1u);
        }
        if (status != ZR_ARTIFACT_STATUS_OK) {
            ZrCore_Array_Free(reader->context->state, outArguments);
            return status;
        }
        ZrCore_Array_Push(reader->context->state, outArguments, &argument);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_type_read_function(SZrArtifactTypeReader *reader,
                                                     TZrTypeId *outTypeId,
                                                     TZrUInt32 depth) {
    TZrUInt8 receiver = 0u;
    TZrUInt8 refExport = 0u;
    TZrUInt8 effectFlags = 0u;
    TZrUInt32 parameterCount = 0u;
    TZrUInt32 index;
    TZrTypeId returnTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrUInt32 canonicalEffects = 0u;
    SZrArray parameters = {0};
    EZrArtifactStatus status;

    status = artifact_type_read_u8(reader, &receiver);
    if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u8(reader, &refExport);
    if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u8(reader, &effectFlags);
    if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u32(reader, &parameterCount);
    if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_node(reader, &returnTypeId, depth + 1u);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (refExport != artifact_type_ref_export_effect(reader->context, returnTypeId) ||
        parameterCount > ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT) {
        return artifact_projection_fail(reader->diagnostic,
                                        ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                        reader->offset);
    }
    if ((effectFlags & ZR_ARTIFACT_CONTRACT_FLAG_THROWS) != 0u)
        canonicalEffects |= ZR_CANONICAL_CALLABLE_EFFECT_THROWS;
    if ((effectFlags & ZR_ARTIFACT_CONTRACT_FLAG_ASYNC) != 0u)
        canonicalEffects |= ZR_CANONICAL_CALLABLE_EFFECT_ASYNC;
    if ((effectFlags & ZR_ARTIFACT_CONTRACT_FLAG_GENERATOR) != 0u)
        canonicalEffects |= ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR;

    ZrCore_Array_Init(reader->context->state,
                      &parameters,
                      sizeof(SZrCanonicalParameterContract),
                      parameterCount > 0u ? parameterCount : 1u);
    for (index = 0u; index < parameterCount; ++index) {
        SZrCanonicalParameterContract parameter;
        TZrUInt8 passing = 0u;
        TZrUInt8 escape = 0u;
        TZrUInt8 entryInitialization = 0u;
        TZrUInt8 exitInitialization = 0u;
        TZrUInt8 acceptsTemporary = 0u;
        TZrUInt8 callSiteMarker = 0u;
        memset(&parameter, 0, sizeof(parameter));
        status = artifact_type_read_u8(reader, &passing);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_read_u8(reader, &escape);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_read_u8(reader, &entryInitialization);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_read_u8(reader, &exitInitialization);
        if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u8(reader, &acceptsTemporary);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_read_u8(reader, &callSiteMarker);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_type_read_node(reader, &parameter.typeId, depth + 1u);
        if (status != ZR_ARTIFACT_STATUS_OK) {
            ZrCore_Array_Free(reader->context->state, &parameters);
            return status;
        }
        parameter.passingForm = (EZrCanonicalPassingForm)passing;
        parameter.escapeUpperBound = (EZrCanonicalEscapeUpperBound)escape;
        parameter.entryInitialization = (EZrCanonicalEntryInitialization)entryInitialization;
        parameter.exitInitialization = (EZrCanonicalExitInitialization)exitInitialization;
        parameter.acceptsTemporary = acceptsTemporary ? ZR_TRUE : ZR_FALSE;
        parameter.callSiteMarker = (EZrCanonicalCallSiteMarker)callSiteMarker;
        ZrCore_Array_Push(reader->context->state, &parameters, &parameter);
    }
    *outTypeId = ZrParser_CanonicalType_InternFunction(
            reader->context,
            (const SZrCanonicalParameterContract *)parameters.head,
            parameters.length,
            returnTypeId,
            (EZrCanonicalReceiverEffect)receiver,
            canonicalEffects);
    ZrCore_Array_Free(reader->context->state, &parameters);
    return *outTypeId != ZR_SEMANTIC_ID_INVALID
                   ? ZR_ARTIFACT_STATUS_OK
                   : artifact_projection_fail(reader->diagnostic,
                                              ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                              reader->offset);
}

static EZrArtifactStatus artifact_type_read_node(SZrArtifactTypeReader *reader,
                                                 TZrTypeId *outTypeId,
                                                 TZrUInt32 depth) {
    TZrUInt8 node = 0u;
    TZrUInt8 qualifier = 0u;
    TZrUInt32 first = 0u;
    TZrUInt32 second = 0u;
    TZrTypeId child = ZR_SEMANTIC_ID_INVALID;
    EZrArtifactStatus status;

    if (depth > ZR_ARTIFACT_SIGNATURE_MAX_DEPTH) {
        return artifact_projection_fail(reader->diagnostic,
                                        ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                        reader->offset);
    }
    status = artifact_type_read_u8(reader, &node);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    switch ((EZrArtifactSignatureNode)node) {
        case ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE:
            status = artifact_type_read_u32(reader, &first);
            if (status == ZR_ARTIFACT_STATUS_OK)
                *outTypeId = ZrParser_CanonicalType_InternPrimitive(reader->context, (EZrValueType)first);
            break;
        case ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF:
            status = artifact_type_read_u32(reader, &first);
            if (status == ZR_ARTIFACT_STATUS_OK) {
                SZrString *artifactName = ZrCore_String_Create(
                        reader->context->state, "<artifact-type>", 15u);
                *outTypeId = ZrParser_CanonicalType_InternNominal(
                        reader->context, reader->moduleIdentity, artifactName, first);
            }
            break;
        case ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_PARAMETER:
            status = artifact_type_read_u32(reader, &first);
            if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u32(reader, &second);
            if (status == ZR_ARTIFACT_STATUS_OK)
                *outTypeId = ZrParser_CanonicalType_InternGenericParameter(reader->context, first, second);
            break;
        case ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_INSTANCE: {
            SZrArray arguments = {0};
            status = artifact_type_read_node(reader, &child, depth + 1u);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_read_generic_arguments(reader, &arguments, depth);
            if (status == ZR_ARTIFACT_STATUS_OK) {
                *outTypeId = ZrParser_CanonicalType_InternGenericInstanceEx(
                        reader->context,
                        child,
                        (const SZrCanonicalGenericArgument *)arguments.head,
                        arguments.length);
                ZrCore_Array_Free(reader->context->state, &arguments);
            }
            break;
        }
        case ZR_ARTIFACT_SIGNATURE_NODE_ARRAY:
            status = artifact_type_read_u32(reader, &first);
            if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_u8(reader, &qualifier);
            if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_node(reader, &child, depth + 1u);
            if (status == ZR_ARTIFACT_STATUS_OK)
                *outTypeId = ZrParser_CanonicalType_InternArray(
                        reader->context, child, first, (EZrCanonicalArrayStorageKind)qualifier);
            break;
        case ZR_ARTIFACT_SIGNATURE_NODE_TUPLE: {
            SZrArray types = {0};
            status = artifact_type_read_type_list(reader, &types, depth);
            if (status == ZR_ARTIFACT_STATUS_OK) {
                *outTypeId = ZrParser_CanonicalType_InternTuple(
                        reader->context, (const TZrTypeId *)types.head, types.length);
                ZrCore_Array_Free(reader->context->state, &types);
            }
            break;
        }
        case ZR_ARTIFACT_SIGNATURE_NODE_UNION: {
            SZrArray variants = {0};
            status = artifact_type_read_node(reader, &child, depth + 1u);
            if (status == ZR_ARTIFACT_STATUS_OK)
                status = artifact_type_read_type_list(reader, &variants, depth);
            if (status == ZR_ARTIFACT_STATUS_OK) {
                *outTypeId = ZrParser_CanonicalType_InternUnion(
                        reader->context, child, (const TZrTypeId *)variants.head, variants.length);
                ZrCore_Array_Free(reader->context->state, &variants);
            }
            break;
        }
        case ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE:
        case ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW:
        case ZR_ARTIFACT_SIGNATURE_NODE_REF:
        case ZR_ARTIFACT_SIGNATURE_NODE_OWNER:
            qualifier = 0u;
            if (node == ZR_ARTIFACT_SIGNATURE_NODE_REF || node == ZR_ARTIFACT_SIGNATURE_NODE_OWNER)
                status = artifact_type_read_u8(reader, &qualifier);
            else
                status = ZR_ARTIFACT_STATUS_OK;
            if (status == ZR_ARTIFACT_STATUS_OK) status = artifact_type_read_node(reader, &child, depth + 1u);
            if (status == ZR_ARTIFACT_STATUS_OK) {
                if (node == ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE)
                    *outTypeId = ZrParser_CanonicalType_InternNullable(reader->context, child);
                else if (node == ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW)
                    *outTypeId = ZrParser_CanonicalType_InternReadonlyView(reader->context, child);
                else if (node == ZR_ARTIFACT_SIGNATURE_NODE_REF)
                    *outTypeId = ZrParser_CanonicalType_InternRef(
                            reader->context, child, (EZrCanonicalRefAccess)qualifier);
                else
                    *outTypeId = ZrParser_CanonicalType_InternOwner(
                            reader->context, child, (EZrCanonicalOwnerKind)qualifier);
            }
            break;
        case ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION:
            return artifact_type_read_function(reader, outTypeId, depth);
        case ZR_ARTIFACT_SIGNATURE_NODE_NEVER:
            *outTypeId = ZrParser_CanonicalType_InternNever(reader->context);
            status = ZR_ARTIFACT_STATUS_OK;
            break;
        case ZR_ARTIFACT_SIGNATURE_NODE_ERROR:
            *outTypeId = ZrParser_CanonicalType_InternError(reader->context);
            status = ZR_ARTIFACT_STATUS_OK;
            break;
        default:
            return artifact_projection_fail(reader->diagnostic,
                                            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                            reader->offset - 1u);
    }
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    return *outTypeId != ZR_SEMANTIC_ID_INVALID
                   ? ZR_ARTIFACT_STATUS_OK
                   : artifact_projection_fail(reader->diagnostic,
                                              ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                              reader->offset);
}

EZrArtifactStatus ZrParser_ArtifactType_WriteSignature(const SZrSemanticContext *context,
                                                       TZrTypeId typeId,
                                                       TZrByte *buffer,
                                                       TZrSize bufferCapacity,
                                                       TZrSize *outWrittenSize,
                                                       SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactTypeWriter writer;
    EZrArtifactStatus status;

    if (outWrittenSize != ZR_NULL) *outWrittenSize = 0u;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (context == ZR_NULL || buffer == ZR_NULL || outWrittenSize == ZR_NULL) {
        return artifact_projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    }
    writer.context = context;
    writer.buffer = buffer;
    writer.capacity = bufferCapacity;
    writer.length = 0u;
    writer.diagnostic = diagnostic;
    status = artifact_type_write_node(&writer, typeId, 0u);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    status = ZrCore_Artifact_ValidateSignature(buffer, writer.length, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    *outWrittenSize = writer.length;
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrParser_ArtifactType_InternSignature(SZrSemanticContext *context,
                                                        SZrString *moduleIdentity,
                                                        const TZrByte *signature,
                                                        TZrSize signatureLength,
                                                        TZrTypeId *outTypeId,
                                                        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactTypeReader reader;
    EZrArtifactStatus status;

    if (outTypeId != ZR_NULL) *outTypeId = ZR_SEMANTIC_ID_INVALID;
    if (context == ZR_NULL || signature == ZR_NULL || outTypeId == ZR_NULL) {
        return artifact_projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    }
    status = ZrCore_Artifact_ValidateSignature(signature, signatureLength, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    reader.context = context;
    reader.moduleIdentity = moduleIdentity;
    reader.signature = signature;
    reader.length = signatureLength;
    reader.offset = 0u;
    reader.diagnostic = diagnostic;
    status = artifact_type_read_node(&reader, outTypeId, 0u);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (reader.offset != reader.length) {
        *outTypeId = ZR_SEMANTIC_ID_INVALID;
        return artifact_projection_fail(diagnostic,
                                        ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                        reader.offset);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static TZrUInt64 artifact_projection_domain_hash(TZrUInt64 signatureHash,
                                                 TZrUInt64 domain,
                                                 TZrMetadataToken token) {
    TZrUInt64 hash = signatureHash ^ domain;
    hash *= 1099511628211ULL;
    hash ^= token;
    hash *= 1099511628211ULL;
    return hash;
}

EZrArtifactStatus ZrParser_ArtifactType_BuildPublicIdentity(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrParserArtifactPublicContract *contract,
        TZrByte *signatureBuffer,
        TZrSize signatureBufferCapacity,
        TZrSize *outSignatureLength,
        SZrArtifactPublicIdentity *outIdentity,
        SZrArtifactDiagnostic *diagnostic) {
    TZrUInt64 signatureHash;
    EZrArtifactStatus status;

    if (outIdentity != ZR_NULL) memset(outIdentity, 0, sizeof(*outIdentity));
    if (context == ZR_NULL || contract == ZR_NULL || signatureBuffer == ZR_NULL ||
        outSignatureLength == ZR_NULL || outIdentity == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(contract->typeRefToken) != ZR_METADATA_TABLE_TYPE_REF ||
        ZR_METADATA_TOKEN_RID(contract->typeRefToken) == 0u ||
        ZR_METADATA_TOKEN_TABLE(contract->typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC ||
        ZR_METADATA_TOKEN_RID(contract->typeSpecToken) == 0u ||
        ZR_METADATA_TOKEN_TABLE(contract->signatureToken) != ZR_METADATA_TABLE_SIGNATURE ||
        ZR_METADATA_TOKEN_RID(contract->signatureToken) == 0u || contract->layoutVersion == 0u ||
        contract->layoutHash == 0u || contract->callableContractHash == 0u ||
        contract->moduleHash == 0u) {
        return artifact_projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    }
    status = ZrParser_ArtifactType_WriteSignature(context,
                                                  typeId,
                                                  signatureBuffer,
                                                  signatureBufferCapacity,
                                                  outSignatureLength,
                                                  diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    signatureHash = ZrCore_Artifact_HashBytes(signatureBuffer, *outSignatureLength);
    outIdentity->canonicalTypeId = typeId;
    outIdentity->typeRefToken = contract->typeRefToken;
    outIdentity->typeSpecToken = contract->typeSpecToken;
    outIdentity->signatureToken = contract->signatureToken;
    outIdentity->typeRefHash = artifact_projection_domain_hash(
            signatureHash, 0x5459504552454601ULL, contract->typeRefToken);
    outIdentity->typeSpecHash = artifact_projection_domain_hash(
            signatureHash, 0x5459504553504543ULL, contract->typeSpecToken);
    outIdentity->signatureHash = signatureHash;
    outIdentity->layoutVersion = contract->layoutVersion;
    outIdentity->layoutHash = contract->layoutHash;
    outIdentity->callableContractHash = contract->callableContractHash;
    outIdentity->moduleHash = contract->moduleHash;
    return ZR_ARTIFACT_STATUS_OK;
}
