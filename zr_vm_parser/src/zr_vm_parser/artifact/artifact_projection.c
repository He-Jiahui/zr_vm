#include "artifact/artifact_projection.h"

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

/**
 * @struct SZrArtifactTypeWriter
 *
 * ## Summary
 * 保存一次签名写出的借用context、输出缓冲和当前游标。 WriteSignature在栈上初始化，所有递归写出helper共享。
 *
 * ## Fields
 * context：只读canonical查询上下文；必须覆盖递归遍历的有效期。
 * buffer：调用者持有的可写签名区；与capacity配对。
 * capacity：该真实缓冲区可写字节数。
 * length：已写的前缀长度；仅完成当前字节段后推进。
 * diagnostic：可空的借用诊断输出，游标以本签名片段为基准。
 *
 * ## Value model
 * 栈上递归写出游标；length表示已经写入调用者缓冲区的前缀字节数。资源由调用者/context持有，结构复制不转移所有权。
 *
 * ## Invariants and lifecycle
 * context的canonical节点只读且在同步写出内稳定；buffer由调用者提供且不与输入重叠，length<=capacity。该状态不拥有context/buffer/diagnostic，不跨调用留存，不提供内部锁；失败保留已写前缀。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:8; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:108; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:123。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @struct SZrArtifactTypeReader
 *
 * ## Summary
 * 保存一次已验证签名的解码游标与目标semantic context。 InternSignature整体校验成功后栈上构造，递归读入helper共享。
 *
 * ## Fields
 * context：非空的可变semantic上下文，其state/类型池已初始化。
 * moduleIdentity：可空名义类型域字符串；可能存入context节点，调用方保持相应可达性。
 * signature：已由Core验证完整语法的只读字节区。
 * length：签名字节数，解码应恰好消费。
 * offset：下一未消费字节位置；普通失败不承诺回滚。
 * diagnostic：可空输出，错误偏移相对本片段。
 *
 * ## Value model
 * 栈上递归读入游标；offset表示已消费的签名字节数。上下文可能驻留新节点，游标结构本身不拥有或释放这些节点。
 *
 * ## Invariants and lifecycle
 * signature只读借用至解码返回且不能在校验后变化；offset<=length。context驻留可分配/改变canonical池，不作事务回滚；moduleIdentity可能由名义节点引用，须符合context/GC可达性规则。该状态自身无资源所有权或同步。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:16; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:396; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:406。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_layout_projection_fail( SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrUInt64 expectedHash, TZrUInt64 actualHash)
 *
 * ## Summary
 * 建立原生布局能力投影的独立失败诊断。 ApplyNativeCapabilities拒绝NULL、悬挂能力或哈希冲突时使用。
 *
 * ## Parameters
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 * @param[in] status 本拒绝分支需要传播的artifact状态，原样返回。
 * @param[in] expectedHash 本布局拒绝分支的预期值。
 * @param[in] actualHash 本布局拒绝分支的实际值。
 *
 * ## Returns
 * @return 原样返回status并交付可选诊断。
 *
 * ## Constraints and lifecycle
 * diagnostic可空；非空时清零全部诊断并填写LAYOUT_TABLE及比较值；不修改layout或取得描述符资源。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:25; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:48; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:62。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn EZrArtifactStatus ZrParser_ArtifactLayout_ApplyNativeCapabilities( const ZrLibTypeDescriptor *typeDescriptor, TZrUInt64 stableSlotContractHash, SZrArtifactLayoutRow *layout, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 把原生STABLE_SLOT_SOURCE协议位与其非零契约哈希协商到artifact布局能力。 原生Pool等描述符构造布局行后调用，保持描述符协议与持久布局一致。
 *
 * ## Parameters
 * @param[in] typeDescriptor 非空原生类型描述符，protocolMask在同步投影期间稳定。
 * @param[in] stableSlotContractHash 有STABLE_SLOT_SOURCE时必须非零，无该协议时必须为0。
 * @param[in,out] layout 非空布局行，只在全部能力门禁成功后补充能力位/哈希，其余字段不变。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK表示本能力局部一致并清零可选诊断；普通失败不修改layout，给出INVALID_ARGUMENT、ILLEGAL_TOKEN或CONTRACT_HASH_MISMATCH。
 *
 * ## Constraints and lifecycle
 * typeDescriptor/layout必填且保持独立有效。没有协议位时要求输入哈希、已有能力位与已有哈希均为零；有协议位时哈希非零，已有非零哈希必须相同。全部检查通过后才OR能力位/写哈希；其他布局字段原样，未验证完整布局。无分配和内部同步，调用方独占layout。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:40; tests/container/test_generational_pool_artifact.c:119; tests/container/test_generational_pool_artifact.c:288; tests/container/test_generational_pool_artifact.c:298。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_projection_fail(SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrSize offset)
 *
 * ## Summary
 * 把类型签名投影的错误状态与片段内游标写入诊断。 读写递归节点、缓冲边界和入口参数拒绝共用。
 *
 * ## Parameters
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 * @param[in] status 本拒绝分支需要传播的artifact状态，原样返回。
 * @param[in] offset 片段内失败字节游标；诊断保存TZrUInt32表示。
 *
 * ## Returns
 * @return 原样返回status；不回滚已写缓冲、已消费游标或context驻留结果。
 *
 * ## Constraints and lifecycle
 * diagnostic可空；清零后写SIGNATURE_HEAP与TZrUInt32偏移。offset是当前签名片段中的位置，不是文件或堆全局偏移；输入须符合签名长度上界。无分配。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:96; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:112; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:173。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_bytes(SZrArtifactTypeWriter *writer, const TZrByte *bytes, TZrSize length)
 *
 * ## Summary
 * 向调用者缓冲追加一个已确定长度的签名片段，成功后推进writer长度。 所有固定宽度整数写出共用，保持写入/游标推进的一致顺序。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] bytes length个可读字节；非零长度时不能与目标追加区重叠。
 * @param[in] length 本次追加字节数，与writer当前长度相加须可表示。
 *
 * ## Returns
 * @return OK时追加length字节并推进游标；本段容量不足返回BUFFER_TOO_SMALL且本段不写、不推进。
 *
 * ## Constraints and lifecycle
 * writer非空且已初始化，length<=capacity、加法可表示；非零length需可读bytes及可写目标且不重叠。先检查本段容量，再memcpy。只借用输入，不分配；前序片段不会因后序失败而回滚。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:108; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:124; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:133。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_u8(SZrArtifactTypeWriter *writer, TZrUInt8 value)
 *
 * ## Summary
 * 把纯整数值按1字节标签线格式追加到签名。 节点标签、字段及计数编码通过统一字节追加入口保持跨平台格式。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] value 待编码的纯整数值，按本函数固定宽度写出。
 *
 * ## Returns
 * @return 返回write_bytes状态；当前整数不足容量时不追加，但此前已写的其他字段保持。
 *
 * ## Constraints and lifecycle
 * writer借用已初始化状态，长度/容量符合write_bytes前提；栈内字节仅在同步复制期间使用，不把宿主整数地址存入artifact。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:123; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:202; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:208。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_u32(SZrArtifactTypeWriter *writer, TZrUInt32 value)
 *
 * ## Summary
 * 把纯整数值按4字节小端线格式追加到签名。 节点标签、字段及计数编码通过统一字节追加入口保持跨平台格式。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] value 待编码的纯整数值，按本函数固定宽度写出。
 *
 * ## Returns
 * @return 返回write_bytes状态；当前整数不足容量时不追加，但此前已写的其他字段保持。
 *
 * ## Constraints and lifecycle
 * writer借用已初始化状态，长度/容量符合write_bytes前提；栈内字节仅在同步复制期间使用，不把宿主整数地址存入artifact。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:127; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:168; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:187。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_u64(SZrArtifactTypeWriter *writer, TZrUInt64 value)
 *
 * ## Summary
 * 把纯整数值按8字节小端线格式追加到签名。 节点标签、字段及计数编码通过统一字节追加入口保持跨平台格式。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] value 待编码的纯整数值，按本函数固定宽度写出。
 *
 * ## Returns
 * @return 返回write_bytes状态；当前整数不足容量时不追加，但此前已写的其他字段保持。
 *
 * ## Constraints and lifecycle
 * writer借用已初始化状态，长度/容量符合write_bytes前提；栈内字节仅在同步复制期间使用，不把宿主整数地址存入artifact。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:136; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:204。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static TZrUInt8 artifact_type_ref_export_effect( const SZrSemanticContext *context, TZrTypeId returnTypeId)
 *
 * ## Summary
 * 从函数返回的canonical类型推导artifact的引用导出能力。 函数签名写出与读入后比对使用同一规则，避免仅相信线格式effect字节。
 *
 * ## Parameters
 * @param[in] context 已初始化且在查询期间稳定的semantic context，其state和canonical池有效。
 * @param[in] returnTypeId context中的返回类型ID；非REF或未找到时导出能力为NONE。
 *
 * ## Returns
 * @return 返回REF_EXPORT_NONE/READONLY/WRITABLE纯值。
 *
 * ## Constraints and lifecycle
 * context及其canonical池在查找期间稳定；returnTypeId未找到或不是REF均为NONE，REF只按readonly/writable访问区分。不分配，不推导更深包装或运行时逃逸保证。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:150; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:243; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:525; tests/parser/test_reference_callable_consumers.c:403; tests/parser/test_reference_callable_consumers.c:424。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_type_id_array(SZrArtifactTypeWriter *writer, const SZrArray *typeIds, TZrUInt32 depth)
 *
 * ## Summary
 * 写出canonical类型列表的数量及按原序排列的子签名。 TUPLE元素与UNION变体共享此格式。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] typeIds 已初始化的TZrTypeId数组，数量/条目符合canonical及签名限额。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK表示逐项写出完成；数组取项/节点/容量失败立即返回对应状态，最终对外写出仍须经过ValidateSignature。
 *
 * ## Constraints and lifecycle
 * writer/typeIds必填；数组已初始化且元素为有效TZrTypeId，数量须适合UINT32及artifact子节点限额。数组只读借用，递归子节点使用depth+1；不分配，不回滚已写计数或先前子节点。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:164; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:347; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:354。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_generic_arguments(SZrArtifactTypeWriter *writer, const SZrArray *arguments, TZrUInt32 depth)
 *
 * ## Summary
 * 写出泛型实参列表，区分类型子树、有符号常整数位模式与常参数身份。 GENERIC_INSTANCE节点写完定义类型后调用。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] arguments 已初始化的canonical泛型实参数组，类型/常量/常参数条目在调用内稳定。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK表示完整列表已写；未知kind、缺失元素或下层写出错误立即返回，缓冲可保留前缀。
 *
 * ## Constraints and lifecycle
 * arguments为已初始化的SZrCanonicalGenericArgument数组，条目kind和数量符合canonical及artifact约束；TYPE以depth+1递归，CONST_INT按TZrUInt64位模式写，CONST_PARAMETER写owner/ordinal。仅借用，不保存数组地址。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:183; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:331。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_function(SZrArtifactTypeWriter *writer, const SZrCanonicalFunctionType *function, TZrUInt32 depth)
 *
 * ## Summary
 * 把canonical可调用合同编码为artifact函数节点的effect、返回类型及逐参数合同。 write_node已写FUNCTION标签后使用；读取端以相同顺序重建。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] function 非空canonical函数合同节点，parameterContracts/返回类型/receiver/effects已建立且同步稳定。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK表示函数主体写完；首次容量/类型/数组拒绝即返回该状态。
 *
 * ## Constraints and lifecycle
 * function借用有效canonical节点；receiver、effectFlags与parameterContracts须符合canonical约束。仅映射THROWS/ASYNC/GENERATOR，refExport由真实返回类型推导；参数六项调用属性与type依序编码，acceptsTemporary转0/1。无分配；后续错误不撤销先前字节。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:229; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:383; zr_vm_parser/src/zr_vm_parser/types/canonical_type.c:907; tests/parser/test_reference_callable_consumers.c:219。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_write_node(SZrArtifactTypeWriter *writer, TZrTypeId typeId, TZrUInt32 depth)
 *
 * ## Summary
 * 递归将context内的canonical类型身份投影为无宿主指针的artifact签名节点。 WriteSignature从depth=0进入；列表、包装类型和函数合同递归共用。
 *
 * ## Parameters
 * @param[in,out] writer 非空栈上写出状态，buffer/capacity有效，当前length在容量内；共享递归游标。
 * @param[in] typeId 属于当前context分析代的canonical类型ID，不是跨context通用句柄。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK表示当前子树写完；不存在类型、深度/类别非法、token或容量错误返回具体状态，不保证buffer原值。
 *
 * ## Constraints and lifecycle
 * context/节点数组须在调用内稳定，depth不得超过MAX_DEPTH。nominal需要非零definitionToken；各种数组和参数必须为合法canonical结构。节点标签后按种类编码，局部失败可能已写标签/前缀；不分配或修改context。最终语法范围仍由Core验证。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:283; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:177; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:199。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_u8(SZrArtifactTypeReader *reader, TZrUInt8 *outValue)
 *
 * ## Summary
 * 从已验证签名游标读取1字节标签。 节点重建读取固定字段时使用，字节仍由外层签名所有者持有。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outValue 必填且不破坏reader的整数输出；仅当前整数完整读取后赋值。
 *
 * ## Returns
 * @return OK时写outValue并推进对应游标；短读返回TRUNCATED_BLOB并标注失败处。
 *
 * ## Constraints and lifecycle
 * reader/outValue必填且互不破坏，0<=offset<=length，signature在整个解码期间不可修改。先核当前字段剩余长度，短读不写outValue、不消费当前字段。 不分配。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:396; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:519; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:520。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_u32(SZrArtifactTypeReader *reader, TZrUInt32 *outValue)
 *
 * ## Summary
 * 从已验证签名游标读取4字节小端整数。 节点重建读取固定字段时使用，字节仍由外层签名所有者持有。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outValue 必填且不破坏reader的整数输出；仅当前整数完整读取后赋值。
 *
 * ## Returns
 * @return OK时写outValue并推进对应游标；短读返回TRUNCATED_BLOB并标注失败处。
 *
 * ## Constraints and lifecycle
 * reader/outValue必填且互不破坏，0<=offset<=length，signature在整个解码期间不可修改。先核当前字段剩余长度，短读不写outValue、不消费当前字段。 不分配。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:406; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:423; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:424。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_u64(SZrArtifactTypeReader *reader, TZrUInt64 *outValue)
 *
 * ## Summary
 * 从已验证签名游标读取8字节小端整数。 节点重建读取固定字段时使用，字节仍由外层签名所有者持有。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outValue 必填且不破坏reader的整数输出；仅当前整数完整读取后赋值。
 *
 * ## Returns
 * @return OK时写outValue并推进对应游标；短读返回TRUNCATED_BLOB并标注失败处。
 *
 * ## Constraints and lifecycle
 * reader/outValue必填且互不破坏，0<=offset<=length，signature在整个解码期间不可修改。分两次u32读取；若后半截短读，前半已消费但不发布outValue。 不分配。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:420; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:485。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_type_list(SZrArtifactTypeReader *reader, SZrArray *outTypes, TZrUInt32 depth)
 *
 * ## Summary
 * 把签名中的类型列表重建为临时TZrTypeId数组，供tuple/union驻留。 read_node读取容器节点后调用；成功数组由调用者在驻留后释放。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outTypes 尚未持有缓冲的临时类型数组，成功后由上层Free；早期count拒绝不初始化。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK时outTypes持有调用方需释放的原生数组；普通错误返回具体状态，已初始化的数组会清空释放。reader游标与context已驻留子节点不会回滚。
 *
 * ## Constraints and lifecycle
 * reader已通过完整ValidateSignature且保持字节稳定；outTypes尚未持有缓冲，count<=MAX_CHILD_COUNT。读取count失败时outTypes未初始化；初始化后普通子节点错误会Free。Array_Init/Push可能Throw MEMORY_ERROR，非局部退出不经过本函数普通清理；调用者须有相应异常策略。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:433; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:653; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:665; zr_vm_core/include/zr_vm_core/array.h:55; zr_vm_core/include/zr_vm_core/array.h:153; zr_vm_core/include/zr_vm_core/array.h:165。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_generic_arguments(SZrArtifactTypeReader *reader, SZrArray *outArguments, TZrUInt32 depth)
 *
 * ## Summary
 * 重建泛型类型/常整数/常参数实参的临时数组。 GENERIC_INSTANCE已读定义类型后使用，成功数组交给canonical驻留器复制。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outArguments 尚未持有缓冲的临时实参数组，成功后由上层Free；普通初始化后失败会释放。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK时交付待调用者释放的数组；count早拒绝不初始化输出，后续普通失败释放已建数组；已消费字节及context变化不回滚。
 *
 * ## Constraints and lifecycle
 * 输入已整体语法验证且保持不变，因此循环中窥看下一节点依赖已验证的元素存在。outArguments尚未拥有存储；count受子节点上限约束。普通条目错误释放数组；数组分配/扩容的MEMORY_ERROR非局部退出不承诺执行Free。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:461; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:632; zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c:189; zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c:194; zr_vm_core/include/zr_vm_core/array.h:55。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_function(SZrArtifactTypeReader *reader, TZrTypeId *outTypeId, TZrUInt32 depth)
 *
 * ## Summary
 * 读入函数合同并以canonical规则重新驻留，而非只相信二进制枚举值。 read_node遇到FUNCTION标签后使用；refExport须与重建返回类型重新推导结果一致。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outTypeId 非空输出位置；本helper早拒绝不统一赋值，外层InternSignature负责先置INVALID。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK时outTypeId为context中的函数类型ID；不兼容合同返回INVALID_SIGNATURE，早拒绝可不改outTypeId，驻留拒绝可写INVALID；外层入口负责初始INVALID。
 *
 * ## Constraints and lifecycle
 * reader已完成整体语法验证；outTypeId有效。读取receiver/refExport/effects/count及返回子树后核导出能力，再建临时参数数组；canonical驻留器再次验证参数关系并复制参数。普通参数失败和驻留完成均Free；OOM Throw不保证普通清理。无context回滚或线程同步。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:506; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:697; zr_vm_parser/src/zr_vm_parser/types/canonical_type.c:918; zr_vm_parser/src/zr_vm_parser/types/canonical_type.c:932; zr_vm_parser/src/zr_vm_parser/types/canonical_type.c:968; tests/parser/test_reference_callable_consumers.c:425。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_type_read_node(SZrArtifactTypeReader *reader, TZrTypeId *outTypeId, TZrUInt32 depth)
 *
 * ## Summary
 * 将一个已验证签名子树重建为目标context内的canonical类型ID。 InternSignature从根进入；容器、包装及函数节点递归调用。
 *
 * ## Parameters
 * @param[in,out] reader 非空已整体语法验证的签名状态，signature稳定、offset<=length，context可驻留新节点。
 * @param[out] outTypeId 非空输出位置；本helper早拒绝不统一赋值，外层InternSignature负责先置INVALID。
 * @param[in] depth 当前递归层数；子节点递增，超过artifact深度限额被拒绝。
 *
 * ## Returns
 * @return OK表示outTypeId不是INVALID；非法kind/子树/驻留关系返回具体状态，早期失败不统一清空outTypeId，入口层先设置INVALID。
 *
 * ## Constraints and lifecycle
 * 整体ValidateSignature是本解码器前提；字节不能在验证和解码之间变化。depth受上限约束；名义类型使用调用者moduleIdentity与占位名和TypeDef token驻留，不重现源名称。容器临时数组普通路径驻留后Free；OOM可Throw且不保证局部清理。已驻留子节点不回滚，ID只在该context分析代内有效。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:590; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:451; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:495; zr_vm_parser/src/zr_vm_parser/types/canonical_type.c:473; zr_vm_parser/src/zr_vm_parser/types/canonical_type_lifecycle.c:7。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn EZrArtifactStatus ZrParser_ArtifactType_WriteSignature(const SZrSemanticContext *context, TZrTypeId typeId, TZrByte *buffer, TZrSize bufferCapacity, TZrSize *outWrittenSize, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将已有canonical类型编码为通过Core语法验证的稳定artifact签名。 源合同、公共身份和可调用消费者在持久化或跨层比对前使用。
 *
 * ## Parameters
 * @param[in] context 已初始化且在查询期间稳定的semantic context，其state和canonical池有效。
 * @param[in] typeId 属于当前context分析代的canonical类型ID，不是跨context通用句柄。
 * @param[out] buffer 非空、bufferCapacity字节可写签名区；失败可能留下已写前缀。
 * @param[in] bufferCapacity 真实可写缓冲容量，不能用NULL缓冲执行计数查询。
 * @param[out] outWrittenSize 必填长度输出；入口先清0，完整写出并通过Core验证后发布长度。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return 普通入口即把非空outWrittenSize设0并清可选诊断；只有编码与ValidateSignature均OK才发布真实长度。返回参数、容量、类型、token或语法状态。
 *
 * ## Constraints and lifecycle
 * context/buffer/outWrittenSize必填；typeId属于稳定context，bufferCapacity与真实可写区一致，输出不得破坏context/input。先递归写出再整体验证；不分配或修改context。不可用NULL buffer查询长度；失败可留下buffer部分字节，重试须忽略旧部分并提供合适容量。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:719; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:813; tests/parser/test_canonical_consumers.c:1244; zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c:241; tests/parser/test_reference_callable_consumers.c:219。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn EZrArtifactStatus ZrParser_ArtifactType_InternSignature(SZrSemanticContext *context, SZrString *moduleIdentity, const TZrByte *signature, TZrSize signatureLength, TZrTypeId *outTypeId, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 验证完整artifact签名字节并在调用者semantic context中重新驻留canonical类型。 二进制签名导入和源/二进制合同对照时使用，不复用其他context的数值ID。
 *
 * ## Parameters
 * @param[in,out] context 非空可变semantic context；驻留可分配节点并更改其类型池，不提供回滚或内部同步。
 * @param[in] moduleIdentity 可空的名义类型域字符串；节点可能保留引用，须按context/GC根规则保持可达。
 * @param[in] signature signatureLength个只读字节；整体校验至递归导入结束期间不变。
 * @param[in] signatureLength 完整单根签名的字节数，受Core签名长度上界约束。
 * @param[out] outTypeId 有效ID输出存储；入口先置INVALID，内部helper早拒绝不统一赋值。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return 先把非空outTypeId设INVALID；语法失败立即返回，语义解码失败返回具体状态。OK时恰好消费全部字节并交付context内有效ID；尾部不符再重置INVALID。无新对象由调用者单独释放，context正常Reset/Free负责驻留节点容器。
 *
 * ## Constraints and lifecycle
 * context/signature/outTypeId必填；moduleIdentity可空，名义类型节点可保留该字符串引用，调用者遵循context/GC规则保持其可达。signature仅借用至返回且验证后不得并发修改。会分配临时数组、字符串和context节点；context由调用者独占，无事务回滚，OOM可能非局部Throw；普通状态拒绝也不回滚已驻留节点。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:746; tests/parser/test_artifact_schema_source_roundtrip.c:117; tests/parser/test_artifact_schema.c:984; zr_vm_parser/src/zr_vm_parser/types/canonical_type_lifecycle.c:7; zr_vm_parser/src/zr_vm_parser/types/canonical_type_lifecycle.c:37; tests/parser/test_reference_callable_consumers.c:429。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static TZrUInt64 artifact_projection_domain_hash(TZrUInt64 signatureHash, TZrUInt64 domain, TZrMetadataToken token)
 *
 * ## Summary
 * 将签名摘要、身份域与metadata token混合成TypeRef/TypeSpec的稳定域摘要。 BuildPublicIdentity对同一签名分别生成引用身份和规格身份的哈希。
 *
 * ## Parameters
 * @param[in] signatureHash 已计算的签名字节摘要纯值。
 * @param[in] domain 区分TypeRef/TypeSpec的固定纯整数域。
 * @param[in] token 用于域混合的metadata token纯值，此helper不验证其表。
 *
 * ## Returns
 * @return 返回混合后的64位值，不返回拥有的对象或失败状态。
 *
 * ## Constraints and lifecycle
 * 三个参数均纯整数，unsigned乘法按TZrUInt64回绕；域由调用者固定。该摘要不是密码学认证，也不验证token表或非零条件。无状态或资源。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:778; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:825; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:827。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn EZrArtifactStatus ZrParser_ArtifactType_BuildPublicIdentity( const SZrSemanticContext *context, TZrTypeId typeId, const SZrParserArtifactPublicContract *contract, TZrByte *signatureBuffer, TZrSize signatureBufferCapacity, TZrSize *outSignatureLength, SZrArtifactPublicIdentity *outIdentity, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将canonical签名与调用者提供的稳定表token、布局/调用/模块合同组合为公共artifact身份。 源与二进制消费者用同一合同重建身份并比较，供完整artifact校验使用。
 *
 * ## Parameters
 * @param[in] context 已初始化且在查询期间稳定的semantic context，其state和canonical池有效。
 * @param[in] typeId 属于当前context分析代的canonical类型ID，不是跨context通用句柄。
 * @param[in] contract 非空公共合同值，三token/版本和哈希满足本接口的非零/表约束。
 * @param[out] signatureBuffer 非空签名输出区，与输入及identity不重叠，失败可留部分内容。
 * @param[in] signatureBufferCapacity signatureBuffer的真实可写字节容量。
 * @param[out] outSignatureLength 必填长度输出；参数门禁早拒绝可保持旧值，进入WriteSignature后先清0。
 * @param[out] outIdentity 必填公共身份输出，入口先清零，签名成功后才完整填写。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时输出签名长度及完整identity；任何普通失败outIdentity全零。入口参数早拒绝不写outSignatureLength，进入WriteSignature后该长度先清0；signatureBuffer可留部分写出内容。
 *
 * ## Constraints and lifecycle
 * context/contract/signatureBuffer/outSignatureLength/outIdentity必填且输入输出不互相覆盖；三token须为对应TypeRef/TypeSpec/Signature表的非零RID，版本/哈希非零。先清identity再校验；只有WriteSignature成功后填身份。不验证合同哈希代表的真实布局或模块，完整图由下层另核。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:788; tests/parser/test_artifact_schema_source_roundtrip.c:106; tests/parser/test_artifact_schema_source_roundtrip.c:127; tests/parser/test_artifact_schema.c:897; tests/parser/test_artifact_schema.c:994。这些位置分别为本单元定义/使用或下层合同证据。
 */
