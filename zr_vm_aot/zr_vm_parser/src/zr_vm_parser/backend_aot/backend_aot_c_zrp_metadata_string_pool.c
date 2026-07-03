#include "backend_aot_c_zrp_metadata_string_pool.h"

#include "backend_aot_c_zrp_metadata_manifest_export.h"
#include "backend_aot_c_zrp_metadata_module_ref.h"
#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_signature.h"
#include "backend_aot_c_zrp_metadata_type_def.h"

#include <stdlib.h>
#include <string.h>

#define CZrAotCZrpStringPoolSignatureScanMaxDepth 64u

TZrBool backend_aot_c_zrp_string_pool_remap_init(SZrAotCZrpStringPoolRemap *remap,
                                                 TZrUInt32 capacity,
                                                 TZrUInt32 sourceByteLength) {
    if (remap == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(remap, 0, sizeof(*remap));
    remap->capacity = capacity;
    remap->sourceByteLength = sourceByteLength;
    if (capacity == 0u) {
        return ZR_TRUE;
    }

    remap->entries = (SZrAotCZrpStringPoolRemapEntry *)malloc(
            (TZrSize)capacity * sizeof(SZrAotCZrpStringPoolRemapEntry));
    if (remap->entries == ZR_NULL) {
        memset(remap, 0, sizeof(*remap));
        return ZR_FALSE;
    }
    memset(remap->entries, 0, (TZrSize)capacity * sizeof(SZrAotCZrpStringPoolRemapEntry));
    return ZR_TRUE;
}

void backend_aot_c_zrp_string_pool_remap_destroy(SZrAotCZrpStringPoolRemap *remap) {
    if (remap == ZR_NULL) {
        return;
    }

    if (remap->entries != ZR_NULL) {
        free(remap->entries);
    }
    memset(remap, 0, sizeof(*remap));
}

TZrBool backend_aot_c_zrp_string_pool_remap_is_identity(const SZrAotCZrpStringPoolRemap *remap) {
    if (remap == ZR_NULL || remap->byteLength != remap->sourceByteLength) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset != remap->entries[index].newOffset) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_string_pool_slice_length(const TZrByte *sourceStringPool,
                                                          TZrUInt32 sourceStringPoolBytes,
                                                          TZrUInt32 oldOffset,
                                                          TZrUInt32 *outByteLength) {
    if (outByteLength == ZR_NULL) {
        return ZR_FALSE;
    }
    *outByteLength = 0u;
    if (sourceStringPool == ZR_NULL || oldOffset >= sourceStringPoolBytes) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = oldOffset; index < sourceStringPoolBytes; index++) {
        if (sourceStringPool[index] == 0u) {
            *outByteLength = index - oldOffset + 1u;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_string_pool_remap_add(SZrAotCZrpStringPoolRemap *remap,
                                                       const TZrByte *sourceStringPool,
                                                       TZrUInt32 sourceStringPoolBytes,
                                                       TZrUInt32 oldOffset,
                                                       TZrUInt32 byteLength) {
    if (remap == ZR_NULL) {
        return ZR_FALSE;
    }
    if (byteLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == oldOffset) {
            return remap->entries[index].byteLength == byteLength;
        }
    }

    if (remap->entries == ZR_NULL || remap->count >= remap->capacity) {
        TZrUInt32 newCapacity = remap->capacity == 0u ? 1u : remap->capacity * 2u;
        SZrAotCZrpStringPoolRemapEntry *newEntries;

        if (remap->capacity > 0x7FFFFFFFu) {
            return ZR_FALSE;
        }
        if (newCapacity < remap->count + 1u) {
            newCapacity = remap->count + 1u;
        }
        if (sourceStringPoolBytes != 0u && newCapacity > sourceStringPoolBytes) {
            newCapacity = sourceStringPoolBytes;
        }
        if (newCapacity < remap->count + 1u) {
            return ZR_FALSE;
        }

        newEntries = (SZrAotCZrpStringPoolRemapEntry *)realloc(
                remap->entries,
                (TZrSize)newCapacity * sizeof(SZrAotCZrpStringPoolRemapEntry));
        if (newEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(newEntries + remap->capacity,
               0,
               (TZrSize)(newCapacity - remap->capacity) * sizeof(SZrAotCZrpStringPoolRemapEntry));
        remap->entries = newEntries;
        remap->capacity = newCapacity;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        const SZrAotCZrpStringPoolRemapEntry *entry = &remap->entries[index];
        if (entry->byteLength == byteLength &&
            oldOffset <= sourceStringPoolBytes &&
            byteLength <= (TZrUInt32)(sourceStringPoolBytes - oldOffset) &&
            entry->oldOffset <= sourceStringPoolBytes &&
            entry->byteLength <= (TZrUInt32)(sourceStringPoolBytes - entry->oldOffset) &&
            memcmp(sourceStringPool + oldOffset, sourceStringPool + entry->oldOffset, byteLength) == 0) {
            remap->entries[remap->count].oldOffset = oldOffset;
            remap->entries[remap->count].byteLength = byteLength;
            remap->entries[remap->count].newOffset = entry->newOffset;
            remap->count++;
            return ZR_TRUE;
        }
    }

    if (byteLength > (TZrUInt32)(0xFFFFFFFFu - remap->byteLength)) {
        return ZR_FALSE;
    }
    remap->entries[remap->count].oldOffset = oldOffset;
    remap->entries[remap->count].byteLength = byteLength;
    remap->entries[remap->count].newOffset = remap->byteLength;
    remap->count++;
    remap->byteLength += byteLength;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_string_offset(SZrAotCZrpStringPoolRemap *remap,
                                                   const TZrByte *sourceStringPool,
                                                   TZrUInt32 sourceStringPoolBytes,
                                                   TZrUInt32 oldOffset) {
    TZrUInt32 byteLength;

    if (sourceStringPoolBytes == 0u) {
        return ZR_TRUE;
    }
    if (!backend_aot_c_zrp_string_pool_slice_length(sourceStringPool,
                                                    sourceStringPoolBytes,
                                                    oldOffset,
                                                    &byteLength)) {
        return ZR_FALSE;
    }

    return backend_aot_c_zrp_string_pool_remap_add(remap,
                                                  sourceStringPool,
                                                  sourceStringPoolBytes,
                                                  oldOffset,
                                                  byteLength);
}

static TZrBool backend_aot_c_zrp_signature_string_scan_skip_bytes(TZrUInt32 signatureBlobLength,
                                                                  TZrSize *offset,
                                                                  TZrUInt32 byteLength) {
    if (offset == ZR_NULL ||
        *offset > (TZrSize)signatureBlobLength ||
        (TZrSize)byteLength > (TZrSize)signatureBlobLength - *offset) {
        return ZR_FALSE;
    }

    *offset += (TZrSize)byteLength;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_string_scan_skip_u32(TZrUInt32 signatureBlobLength,
                                                                TZrSize *offset) {
    return backend_aot_c_zrp_signature_string_scan_skip_bytes(signatureBlobLength, offset, 4u);
}

static TZrBool backend_aot_c_zrp_signature_string_scan_read_u8(const TZrByte *signatureBlob,
                                                               TZrUInt32 signatureBlobLength,
                                                               TZrSize *offset,
                                                               TZrUInt8 *value) {
    if (signatureBlob == ZR_NULL ||
        offset == ZR_NULL ||
        value == ZR_NULL ||
        !backend_aot_c_zrp_signature_string_scan_skip_bytes(signatureBlobLength, offset, 1u)) {
        return ZR_FALSE;
    }

    *value = signatureBlob[*offset - 1u];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_signature_string_scan_read_u32(const TZrByte *signatureBlob,
                                                                TZrUInt32 signatureBlobLength,
                                                                TZrSize *offset,
                                                                TZrUInt32 *value) {
    TZrSize readOffset;

    if (signatureBlob == ZR_NULL ||
        offset == ZR_NULL ||
        value == ZR_NULL ||
        *offset > (TZrSize)signatureBlobLength ||
        (TZrSize)signatureBlobLength - *offset < 4u) {
        return ZR_FALSE;
    }

    readOffset = *offset;
    *value = ((TZrUInt32)signatureBlob[readOffset]) |
             ((TZrUInt32)signatureBlob[readOffset + 1u] << 8u) |
             ((TZrUInt32)signatureBlob[readOffset + 2u] << 16u) |
             ((TZrUInt32)signatureBlob[readOffset + 3u] << 24u);
    *offset += 4u;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 depth);

static TZrBool backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_list(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 count,
        TZrUInt32 depth) {
    for (TZrUInt32 index = 0u; index < count; index++) {
        if (!backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                             sourceStringPool,
                                                                             sourceStringPoolBytes,
                                                                             signatureBlob,
                                                                             signatureBlobLength,
                                                                             offset,
                                                                             depth + 1u)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset,
        TZrUInt32 depth) {
    TZrUInt8 node;
    TZrUInt32 count;
    TZrUInt32 stringOffset;

    if (depth > CZrAotCZrpStringPoolSignatureScanMaxDepth ||
        !backend_aot_c_zrp_signature_string_scan_read_u8(signatureBlob,
                                                         signatureBlobLength,
                                                         offset,
                                                         &node)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            return backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
            return backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                    signatureBlobLength,
                                                                    offset,
                                                                    &stringOffset) &&
                   backend_aot_c_zrp_add_string_offset(stringRemap,
                                                       sourceStringPool,
                                                       sourceStringPoolBytes,
                                                       stringOffset);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            return backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            return backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   offset,
                                                                                   depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            return backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                    signatureBlobLength,
                                                                    offset,
                                                                    &count) &&
                   backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_list(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   offset,
                                                                                   count,
                                                                                   depth);
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
            if (!backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                                 sourceStringPool,
                                                                                 sourceStringPoolBytes,
                                                                                 signatureBlob,
                                                                                 signatureBlobLength,
                                                                                 offset,
                                                                                 depth + 1u) ||
                !backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                  signatureBlobLength,
                                                                  offset,
                                                                  &count)) {
                return ZR_FALSE;
            }
            return backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_list(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   offset,
                                                                                   count,
                                                                                   depth);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            return backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset) &&
                   backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   offset,
                                                                                   depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset) ||
                !backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                  signatureBlobLength,
                                                                  offset,
                                                                  &stringOffset) ||
                !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                     sourceStringPool,
                                                     sourceStringPoolBytes,
                                                     stringOffset) ||
                !backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                  signatureBlobLength,
                                                                  offset,
                                                                  &count)) {
                return ZR_FALSE;
            }
            return backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_list(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   offset,
                                                                                   count,
                                                                                   depth);
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   offset,
                                                                                   depth + 1u);
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
            return backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset);
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            return backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                    signatureBlobLength,
                                                                    offset,
                                                                    &stringOffset) &&
                   backend_aot_c_zrp_add_string_offset(stringRemap,
                                                       sourceStringPool,
                                                       sourceStringPoolBytes,
                                                       stringOffset) &&
                   backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                                    signatureBlobLength,
                                                                    offset,
                                                                    &stringOffset) &&
                   backend_aot_c_zrp_add_string_offset(stringRemap,
                                                       sourceStringPool,
                                                       sourceStringPoolBytes,
                                                       stringOffset);
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_zrp_add_method_signature_type_ref_string_offsets(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset) {
    TZrUInt32 parameterCount;

    if (!backend_aot_c_zrp_signature_string_scan_skip_bytes(signatureBlobLength, offset, 2u) ||
        !backend_aot_c_zrp_signature_string_scan_skip_u32(signatureBlobLength, offset) ||
        !backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                         sourceStringPool,
                                                                         sourceStringPoolBytes,
                                                                         signatureBlob,
                                                                         signatureBlobLength,
                                                                         offset,
                                                                         0u) ||
        !backend_aot_c_zrp_signature_string_scan_read_u32(signatureBlob,
                                                          signatureBlobLength,
                                                          offset,
                                                          &parameterCount)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < parameterCount; index++) {
        if (!backend_aot_c_zrp_signature_string_scan_skip_bytes(signatureBlobLength, offset, 1u) ||
            !backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                             sourceStringPool,
                                                                             sourceStringPoolBytes,
                                                                             signatureBlob,
                                                                             signatureBlobLength,
                                                                             offset,
                                                                             0u)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_add_field_signature_type_ref_string_offsets(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength,
        TZrSize *offset) {
    return backend_aot_c_zrp_signature_string_scan_skip_bytes(signatureBlobLength, offset, 1u) &&
           backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                           sourceStringPool,
                                                                           sourceStringPoolBytes,
                                                                           signatureBlob,
                                                                           signatureBlobLength,
                                                                           offset,
                                                                           0u);
}

static TZrBool backend_aot_c_zrp_add_signature_blob_type_ref_string_offsets(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlob,
        TZrUInt32 signatureBlobLength) {
    TZrSize offset;
    TZrUInt8 rootNode;
    TZrBool valid;

    if (stringRemap == ZR_NULL ||
        sourceStringPool == ZR_NULL ||
        signatureBlob == ZR_NULL ||
        signatureBlobLength == 0u) {
        return ZR_FALSE;
    }

    offset = 0u;
    if (!backend_aot_c_zrp_signature_string_scan_read_u8(signatureBlob,
                                                         signatureBlobLength,
                                                         &offset,
                                                         &rootNode)) {
        return ZR_FALSE;
    }

    switch ((EZrMetadataSignatureNode)rootNode) {
        case ZR_METADATA_SIGNATURE_NODE_METHOD_SIG:
            valid = backend_aot_c_zrp_add_method_signature_type_ref_string_offsets(stringRemap,
                                                                                   sourceStringPool,
                                                                                   sourceStringPoolBytes,
                                                                                   signatureBlob,
                                                                                   signatureBlobLength,
                                                                                   &offset);
            break;
        case ZR_METADATA_SIGNATURE_NODE_FIELD_SIG:
            valid = backend_aot_c_zrp_add_field_signature_type_ref_string_offsets(stringRemap,
                                                                                  sourceStringPool,
                                                                                  sourceStringPoolBytes,
                                                                                  signatureBlob,
                                                                                  signatureBlobLength,
                                                                                  &offset);
            break;
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST:
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
        case ZR_METADATA_SIGNATURE_NODE_UNION:
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
        case ZR_METADATA_SIGNATURE_NODE_MEMBER_REF:
        case ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF:
        case ZR_METADATA_SIGNATURE_NODE_MODULE:
            offset = 0u;
            valid = backend_aot_c_zrp_add_signature_type_ref_string_offsets_in_node(stringRemap,
                                                                                    sourceStringPool,
                                                                                    sourceStringPoolBytes,
                                                                                    signatureBlob,
                                                                                    signatureBlobLength,
                                                                                    &offset,
                                                                                    0u);
            break;
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
        case ZR_METADATA_SIGNATURE_NODE_INVALID:
        default:
            valid = ZR_FALSE;
            break;
    }

    return (TZrBool)(valid && offset == signatureBlobLength);
}

static TZrBool backend_aot_c_zrp_add_retained_signature_type_ref_string_offsets(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    if (signatureRemap == ZR_NULL ||
        (signatureRemap->count != 0u && signatureRemap->entries == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (signatureBlobPoolBytes == 0u || signatureRemap->count == 0u) {
        return ZR_TRUE;
    }
    if (signatureBlobPool == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < signatureRemap->count; index++) {
        const SZrAotCZrpSignatureBlobRemapEntry *entry = &signatureRemap->entries[index];
        if (entry->byteLength == 0u) {
            continue;
        }
        if (entry->oldOffset > signatureBlobPoolBytes ||
            entry->byteLength > signatureBlobPoolBytes - entry->oldOffset ||
            !backend_aot_c_zrp_add_signature_blob_type_ref_string_offsets(
                    stringRemap,
                    sourceStringPool,
                    sourceStringPoolBytes,
                    signatureBlobPool + entry->oldOffset,
                    entry->byteLength)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_string_pool_inputs_are_valid(
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrZrpMetadataManifestExportRow *manifestExportRows,
        TZrUInt32 manifestExportCount) {
    if (sourceStringPoolBytes != 0u && sourceStringPool == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)((typeCount == 0u || typeRows != ZR_NULL) &&
                     (methodCount == 0u || methodRows != ZR_NULL) &&
                     (fieldCount == 0u || fieldRows != ZR_NULL) &&
                     (genericParamCount == 0u || genericParamRows != ZR_NULL) &&
                     (moduleRefCount == 0u || moduleRefRows != ZR_NULL) &&
                     (manifestExportCount == 0u || manifestExportRows != ZR_NULL));
}

TZrBool backend_aot_c_zrp_build_string_pool_remap(
        SZrAotCZrpStringPoolRemap *stringRemap,
        const TZrByte *sourceStringPool,
        TZrUInt32 sourceStringPoolBytes,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        TZrUInt32 retainedTypeDefCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrZrpMetadataManifestExportRow *manifestExportRows,
        TZrUInt32 manifestExportCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        const TZrByte *signatureBlobPool,
        TZrUInt32 signatureBlobPoolBytes,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    if (stringRemap == ZR_NULL ||
        !backend_aot_c_zrp_string_pool_inputs_are_valid(sourceStringPool,
                                                        sourceStringPoolBytes,
                                                        typeRows,
                                                        typeCount,
                                                        methodRows,
                                                        methodCount,
                                                        fieldRows,
                                                        fieldCount,
                                                        genericParamRows,
                                                        genericParamCount,
                                                        moduleRefRows,
                                                        moduleRefCount,
                                                        manifestExportRows,
                                                        manifestExportCount)) {
        return ZR_FALSE;
    }
    if (sourceStringPoolBytes == 0u) {
        return ZR_TRUE;
    }

    (void)retainedTypeDefCount;

    for (TZrUInt32 index = 0u; index < typeCount; index++) {
        if (!backend_aot_c_zrp_type_def_row_is_retained(&typeRows[index],
                                                        typeRows,
                                                        typeCount,
                                                        tokenRecords,
                                                        tokenRecordCount,
                                                        methodRows,
                                                        methodCount,
                                                        fieldRows,
                                                        fieldCount,
                                                        genericParamRows,
                                                        genericParamCount,
                                                        genericParamConstraintRows,
                                                        genericParamConstraintCount,
                                                        functionTable,
                                                        retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 typeRows[index].nameStringOffset) ||
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 typeRows[index].namespaceStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < methodCount; index++) {
        if (backend_aot_c_zrp_method_def_row_is_retained(&methodRows[index], functionTable) &&
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 methodRows[index].nameStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < fieldCount; index++) {
        if (!backend_aot_c_zrp_field_def_row_is_retained(&fieldRows[index],
                                                         typeRows,
                                                         typeCount,
                                                         tokenRecords,
                                                         tokenRecordCount,
                                                         methodRows,
                                                         methodCount,
                                                         fieldRows,
                                                         fieldCount,
                                                         genericParamRows,
                                                         genericParamCount,
                                                         genericParamConstraintRows,
                                                         genericParamConstraintCount,
                                                         functionTable,
                                                         retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 fieldRows[index].nameStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < genericParamCount; index++) {
        if (backend_aot_c_zrp_generic_param_row_is_retained(&genericParamRows[index],
                                                            typeRows,
                                                            typeCount,
                                                            tokenRecords,
                                                            tokenRecordCount,
                                                            methodRows,
                                                            methodCount,
                                                            fieldRows,
                                                            fieldCount,
                                                            genericParamRows,
                                                            genericParamCount,
                                                            genericParamConstraintRows,
                                                            genericParamConstraintCount,
                                                            functionTable,
                                                            retainedMethodDefCount) &&
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 genericParamRows[index].nameStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < moduleRefCount; index++) {
        if (!backend_aot_c_zrp_module_ref_row_is_retained(&moduleRefRows[index],
                                                          tokenRecords,
                                                          tokenRecordCount,
                                                          typeSpecRows,
                                                          typeSpecCount,
                                                          typeRows,
                                                          typeCount,
                                                          methodRows,
                                                          methodCount,
                                                          fieldRows,
                                                          fieldCount,
                                                          genericParamRows,
                                                          genericParamCount,
                                                          genericParamConstraintRows,
                                                          genericParamConstraintCount,
                                                          functionTable,
                                                          retainedMethodDefCount,
                                                          signatureBlobPool,
                                                          signatureBlobPoolBytes,
                                                          signatureRemap)) {
            continue;
        }
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 moduleRefRows[index].nameStringOffset) ||
            !backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 moduleRefRows[index].versionStringOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < manifestExportCount; index++) {
        if (!backend_aot_c_zrp_manifest_export_row_is_retained(&manifestExportRows[index],
                                                               tokenRecords,
                                                               tokenRecordCount,
                                                               typeRows,
                                                               typeCount,
                                                               methodRows,
                                                               methodCount,
                                                               fieldRows,
                                                               fieldCount,
                                                               genericParamRows,
                                                               genericParamCount,
                                                               genericParamConstraintRows,
                                                               genericParamConstraintCount,
                                                               functionTable,
                                                               retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_add_string_offset(stringRemap,
                                                 sourceStringPool,
                                                 sourceStringPoolBytes,
                                                 manifestExportRows[index].targetStringOffset)) {
            return ZR_FALSE;
        }
    }

    return backend_aot_c_zrp_add_retained_signature_type_ref_string_offsets(stringRemap,
                                                                           sourceStringPool,
                                                                           sourceStringPoolBytes,
                                                                           signatureBlobPool,
                                                                           signatureBlobPoolBytes,
                                                                           signatureRemap);
}

void backend_aot_c_zrp_copy_string_pool(TZrByte *targetBlob,
                                        const TZrByte *sourceBlob,
                                        const SZrZrpMetadataHeader *sourceHeader,
                                        const SZrZrpMetadataHeader *targetHeader,
                                        const SZrAotCZrpStringPoolRemap *stringRemap) {
    TZrByte *targetPool;
    const TZrByte *sourcePool;

    if (targetBlob == ZR_NULL ||
        sourceBlob == ZR_NULL ||
        sourceHeader == ZR_NULL ||
        targetHeader == ZR_NULL ||
        stringRemap == ZR_NULL ||
        targetHeader->stringPool.byteLength == 0u) {
        return;
    }

    targetPool = targetBlob + targetHeader->stringPool.offset;
    sourcePool = sourceBlob + sourceHeader->stringPool.offset;
    for (TZrUInt32 index = 0u; index < stringRemap->count; index++) {
        memcpy(targetPool + stringRemap->entries[index].newOffset,
               sourcePool + stringRemap->entries[index].oldOffset,
               stringRemap->entries[index].byteLength);
    }
}

TZrBool backend_aot_c_zrp_remap_string_offset(TZrUInt32 *stringOffset,
                                              const SZrAotCZrpStringPoolRemap *remap) {
    if (stringOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    if (remap == ZR_NULL || remap->sourceByteLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < remap->count; index++) {
        if (remap->entries[index].oldOffset == *stringOffset) {
            *stringOffset = remap->entries[index].newOffset;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_remap_type_def_string_offsets(SZrZrpMetadataTypeDefRow *row,
                                                        const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap) &&
                     backend_aot_c_zrp_remap_string_offset(&row->namespaceStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_method_def_string_offsets(SZrZrpMetadataMethodDefRow *row,
                                                          const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_field_def_string_offsets(SZrZrpMetadataFieldDefRow *row,
                                                         const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_generic_param_string_offsets(SZrZrpMetadataGenericParamRow *row,
                                                            const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap));
}

TZrBool backend_aot_c_zrp_remap_module_ref_string_offsets(SZrZrpMetadataModuleRefRow *row,
                                                          const SZrAotCZrpStringPoolRemap *stringRemap) {
    return (TZrBool)(row != ZR_NULL &&
                     backend_aot_c_zrp_remap_string_offset(&row->nameStringOffset, stringRemap) &&
                     backend_aot_c_zrp_remap_string_offset(&row->versionStringOffset, stringRemap));
}
