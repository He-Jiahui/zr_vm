#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

typedef struct SZrArtifactSignatureReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrSize offset;
    SZrArtifactDiagnostic *diagnostic;
} SZrArtifactSignatureReader;

static EZrArtifactStatus artifact_signature_truncated(SZrArtifactSignatureReader *reader) {
    return zr_artifact_fail(reader->diagnostic,
                            ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                            0u,
                            (TZrUInt32)reader->offset);
}

static EZrArtifactStatus artifact_signature_invalid(SZrArtifactSignatureReader *reader) {
    return zr_artifact_fail(reader->diagnostic,
                            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                            0u,
                            (TZrUInt32)reader->offset);
}

static EZrArtifactStatus artifact_signature_read_u8(SZrArtifactSignatureReader *reader,
                                                    TZrUInt8 *outValue) {
    if (reader->offset >= reader->length) {
        return artifact_signature_truncated(reader);
    }
    *outValue = reader->bytes[reader->offset++];
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_signature_read_u32(SZrArtifactSignatureReader *reader,
                                                     TZrUInt32 *outValue) {
    if (reader->length - reader->offset < 4u) {
        return artifact_signature_truncated(reader);
    }
    *outValue = zr_artifact_read_u32(reader->bytes + reader->offset);
    reader->offset += 4u;
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_signature_skip_u64(SZrArtifactSignatureReader *reader) {
    if (reader->length - reader->offset < 8u) {
        return artifact_signature_truncated(reader);
    }
    reader->offset += 8u;
    return ZR_ARTIFACT_STATUS_OK;
}

static TZrBool artifact_signature_token_is(TZrMetadataToken token, TZrUInt32 table) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == table &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static EZrArtifactStatus artifact_signature_validate_node(SZrArtifactSignatureReader *reader,
                                                          TZrUInt32 depth);

static EZrArtifactStatus artifact_signature_validate_nodes(SZrArtifactSignatureReader *reader,
                                                           TZrUInt32 count,
                                                           TZrUInt32 depth) {
    TZrUInt32 index;
    EZrArtifactStatus status;

    if (count > ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT) {
        return zr_artifact_fail(reader->diagnostic,
                                ZR_ARTIFACT_STATUS_COUNT_LIMIT,
                                ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                                0u,
                                (TZrUInt32)reader->offset);
    }
    for (index = 0u; index < count; ++index) {
        status = artifact_signature_validate_node(reader, depth + 1u);
        if (status != ZR_ARTIFACT_STATUS_OK) {
            return status;
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_signature_validate_function(SZrArtifactSignatureReader *reader,
                                                              TZrUInt32 depth) {
    TZrUInt8 receiver;
    TZrUInt8 refExport;
    TZrUInt8 effectFlags;
    TZrUInt32 parameterCount;
    TZrUInt32 index;
    EZrArtifactStatus status;

    status = artifact_signature_read_u8(reader, &receiver);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    status = artifact_signature_read_u8(reader, &refExport);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    status = artifact_signature_read_u8(reader, &effectFlags);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    status = artifact_signature_read_u32(reader, &parameterCount);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (receiver > ZR_ARTIFACT_RECEIVER_MUTABLE ||
        refExport > ZR_ARTIFACT_REF_EXPORT_WRITABLE ||
        (effectFlags & ~(ZR_ARTIFACT_CONTRACT_FLAG_THROWS |
                         ZR_ARTIFACT_CONTRACT_FLAG_ASYNC |
                         ZR_ARTIFACT_CONTRACT_FLAG_GENERATOR)) != 0u ||
        parameterCount > ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT) {
        return artifact_signature_invalid(reader);
    }

    status = artifact_signature_validate_node(reader, depth + 1u);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    for (index = 0u; index < parameterCount; ++index) {
        TZrUInt8 passing;
        TZrUInt8 escape;
        TZrUInt8 entryInitialization;
        TZrUInt8 exitInitialization;
        TZrUInt8 acceptsTemporary;
        TZrUInt8 callSiteMarker;
        status = artifact_signature_read_u8(reader, &passing);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        status = artifact_signature_read_u8(reader, &escape);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        status = artifact_signature_read_u8(reader, &entryInitialization);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        status = artifact_signature_read_u8(reader, &exitInitialization);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        status = artifact_signature_read_u8(reader, &acceptsTemporary);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        status = artifact_signature_read_u8(reader, &callSiteMarker);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        if (passing > ZR_ARTIFACT_PASSING_OUT || escape > ZR_ARTIFACT_ESCAPE_UNKNOWN ||
            entryInitialization > ZR_ARTIFACT_ENTRY_UNINITIALIZED ||
            exitInitialization > ZR_ARTIFACT_EXIT_DEFINITELY_INITIALIZED ||
            acceptsTemporary > 1u || callSiteMarker > ZR_ARTIFACT_CALL_SITE_OUT) {
            return artifact_signature_invalid(reader);
        }
        status = artifact_signature_validate_node(reader, depth + 1u);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

static EZrArtifactStatus artifact_signature_validate_node(SZrArtifactSignatureReader *reader,
                                                          TZrUInt32 depth) {
    TZrUInt8 node;
    TZrUInt8 qualifier;
    TZrUInt32 value;
    TZrUInt32 count;
    EZrArtifactStatus status;

    if (depth > ZR_ARTIFACT_SIGNATURE_MAX_DEPTH) {
        return artifact_signature_invalid(reader);
    }
    status = artifact_signature_read_u8(reader, &node);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }

    switch ((EZrArtifactSignatureNode)node) {
        case ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE:
            status = artifact_signature_read_u32(reader, &value);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            return value <= 255u ? ZR_ARTIFACT_STATUS_OK : artifact_signature_invalid(reader);
        case ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF:
            status = artifact_signature_read_u32(reader, &value);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            return artifact_signature_token_is(value, ZR_METADATA_TABLE_TYPE_DEF)
                           ? ZR_ARTIFACT_STATUS_OK
                           : artifact_signature_invalid(reader);
        case ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_PARAMETER:
        case ZR_ARTIFACT_SIGNATURE_NODE_CONST_PARAMETER:
            status = artifact_signature_read_u32(reader, &value);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            if (value == 0u) return artifact_signature_invalid(reader);
            return artifact_signature_read_u32(reader, &count);
        case ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_INSTANCE:
            status = artifact_signature_validate_node(reader, depth + 1u);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            status = artifact_signature_read_u32(reader, &count);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            return artifact_signature_validate_nodes(reader, count, depth);
        case ZR_ARTIFACT_SIGNATURE_NODE_ARRAY:
            status = artifact_signature_read_u32(reader, &value);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            status = artifact_signature_read_u8(reader, &qualifier);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            if (value == 0u || qualifier > 2u) return artifact_signature_invalid(reader);
            return artifact_signature_validate_node(reader, depth + 1u);
        case ZR_ARTIFACT_SIGNATURE_NODE_TUPLE:
            status = artifact_signature_read_u32(reader, &count);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            return artifact_signature_validate_nodes(reader, count, depth);
        case ZR_ARTIFACT_SIGNATURE_NODE_UNION:
            status = artifact_signature_validate_node(reader, depth + 1u);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            status = artifact_signature_read_u32(reader, &count);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            return artifact_signature_validate_nodes(reader, count, depth);
        case ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE:
        case ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW:
            return artifact_signature_validate_node(reader, depth + 1u);
        case ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION:
            return artifact_signature_validate_function(reader, depth);
        case ZR_ARTIFACT_SIGNATURE_NODE_REF:
            status = artifact_signature_read_u8(reader, &qualifier);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            if (qualifier > ZR_ARTIFACT_REF_READONLY) return artifact_signature_invalid(reader);
            return artifact_signature_validate_node(reader, depth + 1u);
        case ZR_ARTIFACT_SIGNATURE_NODE_OWNER:
            status = artifact_signature_read_u8(reader, &qualifier);
            if (status != ZR_ARTIFACT_STATUS_OK) return status;
            if (qualifier > ZR_ARTIFACT_OWNER_ATOMIC_SHARED) return artifact_signature_invalid(reader);
            return artifact_signature_validate_node(reader, depth + 1u);
        case ZR_ARTIFACT_SIGNATURE_NODE_NEVER:
        case ZR_ARTIFACT_SIGNATURE_NODE_ERROR:
            return ZR_ARTIFACT_STATUS_OK;
        case ZR_ARTIFACT_SIGNATURE_NODE_CONST_INT:
            return artifact_signature_skip_u64(reader);
        case ZR_ARTIFACT_SIGNATURE_NODE_INVALID:
        default:
            return artifact_signature_invalid(reader);
    }
}

EZrArtifactStatus ZrCore_Artifact_ValidateSignature(const TZrByte *signature,
                                                    TZrSize signatureLength,
                                                    SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSignatureReader reader;
    EZrArtifactStatus status;

    zr_artifact_diagnostic_clear(diagnostic);
    if (signature == ZR_NULL || signatureLength == 0u ||
        signatureLength > ZR_ARTIFACT_MAX_BYTE_LENGTH) {
        return zr_artifact_fail(diagnostic,
                                signatureLength > ZR_ARTIFACT_MAX_BYTE_LENGTH
                                        ? ZR_ARTIFACT_STATUS_COUNT_LIMIT
                                        : ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                                ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                                0u,
                                0u);
    }
    reader.bytes = signature;
    reader.length = signatureLength;
    reader.offset = 0u;
    reader.diagnostic = diagnostic;
    status = artifact_signature_validate_node(&reader, 0u);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if (reader.offset != reader.length) {
        return artifact_signature_invalid(&reader);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrCore_Artifact_ReadCallableSignatureSummary(
        const TZrByte *signature,
        TZrSize signatureLength,
        SZrArtifactCallableSignatureSummary *outSummary,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSignatureReader reader;
    TZrUInt8 node = 0U;
    TZrUInt8 receiver = 0U;
    TZrUInt8 refExport = 0U;
    TZrUInt8 effectFlags = 0U;
    TZrUInt32 parameterCount = 0U;
    TZrUInt32 index;
    EZrArtifactStatus status;

    if (outSummary != ZR_NULL) {
        memset(outSummary, 0, sizeof(*outSummary));
    }
    if (outSummary == ZR_NULL) {
        return zr_artifact_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                0U,
                0U);
    }
    status = ZrCore_Artifact_ValidateSignature(
            signature, signatureLength, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }

    reader.bytes = signature;
    reader.length = signatureLength;
    reader.offset = 0U;
    reader.diagnostic = diagnostic;
    status = artifact_signature_read_u8(&reader, &node);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if (node != ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION) {
        return artifact_signature_invalid(&reader);
    }
    status = artifact_signature_read_u8(&reader, &receiver);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_signature_read_u8(&reader, &refExport);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_signature_read_u8(&reader, &effectFlags);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_signature_read_u32(&reader, &parameterCount);
    if (status == ZR_ARTIFACT_STATUS_OK)
        status = artifact_signature_validate_node(&reader, 1U);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }

    outSummary->receiverEffect = (EZrArtifactReceiverEffect)receiver;
    outSummary->refExportEffect = (EZrArtifactRefExportEffect)refExport;
    outSummary->effectFlags = effectFlags;
    outSummary->parameterCount = parameterCount;
    for (index = 0U; index < parameterCount; index++) {
        TZrUInt8 passing = 0U;
        TZrUInt8 escape = 0U;
        TZrUInt8 ignored = 0U;

        status = artifact_signature_read_u8(&reader, &passing);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_signature_read_u8(&reader, &escape);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_signature_read_u8(&reader, &ignored);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_signature_read_u8(&reader, &ignored);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_signature_read_u8(&reader, &ignored);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_signature_read_u8(&reader, &ignored);
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = artifact_signature_validate_node(&reader, 1U);
        if (status != ZR_ARTIFACT_STATUS_OK) {
            memset(outSummary, 0, sizeof(*outSummary));
            return status;
        }
        if ((passing == ZR_ARTIFACT_PASSING_REF ||
             passing == ZR_ARTIFACT_PASSING_REF_READONLY) &&
            escape == ZR_ARTIFACT_ESCAPE_FUNCTION) {
            outSummary->hasScopedParameter = ZR_TRUE;
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}
