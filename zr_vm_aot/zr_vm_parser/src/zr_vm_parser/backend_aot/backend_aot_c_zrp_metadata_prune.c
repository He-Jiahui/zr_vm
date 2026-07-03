#include "backend_aot_c_zrp_metadata_prune.h"

#include "backend_aot_c_zrp_metadata_constant_pool.h"
#include "backend_aot_c_zrp_metadata_manifest_export.h"
#include "backend_aot_c_zrp_metadata_member_token.h"
#include "backend_aot_c_zrp_metadata_module_ref.h"
#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_sections.h"
#include "backend_aot_c_zrp_metadata_signature.h"
#include "backend_aot_c_zrp_metadata_string_pool.h"
#include "backend_aot_c_zrp_metadata_type_def.h"
#include "backend_aot_c_zrp_metadata_type_spec.h"
#include "backend_aot_internal.h"

#include "zr_vm_core/zrp_metadata.h"

#include <stdlib.h>
#include <string.h>

static TZrBool backend_aot_c_zrp_section_has_rows(const SZrZrpMetadataSection *section) {
    return (TZrBool)(section != ZR_NULL && section->byteLength > 0u && section->count > 0u);
}

static TZrBool backend_aot_c_zrp_can_prune_method_defs(const SZrZrpMetadataHeader *header) {
    if (header == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_section_has_rows(&header->methodDefs)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_build_pruned_header(const SZrZrpMetadataHeader *sourceHeader,
                                                      TZrUInt32 retainedTokenRecordCount,
                                                      TZrUInt32 retainedTypeDefCount,
                                                      TZrUInt32 retainedMethodDefCount,
                                                      TZrUInt32 retainedFieldDefCount,
                                                      TZrUInt32 retainedGenericParamCount,
                                                     TZrUInt32 retainedGenericParamConstraintCount,
                                                     TZrUInt32 retainedTypeSpecCount,
                                                     TZrUInt32 retainedMethodSpecCount,
                                                     TZrUInt32 retainedModuleRefCount,
                                                     TZrUInt32 retainedStringPoolBytes,
                                                     TZrUInt32 retainedSignatureBlobBytes,
                                                     TZrUInt32 retainedConstantPoolBytes,
                                                     SZrZrpMetadataHeader *outHeader,
                                                     TZrSize *outLength) {
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    if (sourceHeader == ZR_NULL || outHeader == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    *outHeader = *sourceHeader;
    for (TZrUInt32 sectionKind = 0u; sectionKind < ZR_ZRP_METADATA_SECTION_COUNT; sectionKind++) {
        const SZrZrpMetadataSection *sourceSection =
                backend_aot_c_zrp_metadata_section(sourceHeader, (EZrZrpMetadataSectionKind)sectionKind);
        SZrZrpMetadataSection *targetSection =
                backend_aot_c_zrp_metadata_mutable_section(outHeader, (EZrZrpMetadataSectionKind)sectionKind);
        TZrUInt32 byteLength;
        TZrUInt32 count;
        TZrUInt32 elementSize;

        if (sourceSection == ZR_NULL || targetSection == ZR_NULL) {
            return ZR_FALSE;
        }

        byteLength = sourceSection->byteLength;
        count = sourceSection->count;
        elementSize = sourceSection->elementSize;
        if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS) {
            count = retainedTokenRecordCount;
            elementSize = retainedTokenRecordCount > 0u ? (TZrUInt32)sizeof(SZrMetadataTokenRecord) : 0u;
            byteLength = retainedTokenRecordCount * (TZrUInt32)sizeof(SZrMetadataTokenRecord);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TYPE_DEFS) {
            count = retainedTypeDefCount;
            elementSize = retainedTypeDefCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow) : 0u;
            byteLength = retainedTypeDefCount * (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_DEFS) {
            count = retainedMethodDefCount;
            elementSize = retainedMethodDefCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow) : 0u;
            byteLength = retainedMethodDefCount * (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_FIELD_DEFS) {
            count = retainedFieldDefCount;
            elementSize = retainedFieldDefCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow) : 0u;
            byteLength = retainedFieldDefCount * (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS) {
            count = retainedGenericParamCount;
            elementSize = retainedGenericParamCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow) : 0u;
            byteLength = retainedGenericParamCount * (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS) {
            count = retainedGenericParamConstraintCount;
            elementSize = retainedGenericParamConstraintCount > 0u
                                  ? (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow)
                                  : 0u;
            byteLength =
                    retainedGenericParamConstraintCount * (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TYPE_SPECS) {
            count = retainedTypeSpecCount;
            elementSize = retainedTypeSpecCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow) : 0u;
            byteLength = retainedTypeSpecCount * (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_SPECS) {
            count = retainedMethodSpecCount;
            elementSize = retainedMethodSpecCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow) : 0u;
            byteLength = retainedMethodSpecCount * (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_MODULE_REFS) {
            count = retainedModuleRefCount;
            elementSize = retainedModuleRefCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow) : 0u;
            byteLength = retainedModuleRefCount * (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow);
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_STRING_POOL) {
            count = retainedStringPoolBytes;
            elementSize = retainedStringPoolBytes > 0u ? 1u : 0u;
            byteLength = retainedStringPoolBytes;
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL) {
            count = retainedSignatureBlobBytes;
            elementSize = retainedSignatureBlobBytes > 0u ? 1u : 0u;
            byteLength = retainedSignatureBlobBytes;
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_CONSTANT_POOL) {
            count = retainedConstantPoolBytes;
            elementSize = retainedConstantPoolBytes > 0u ? 1u : 0u;
            byteLength = retainedConstantPoolBytes;
        }

        backend_aot_c_zrp_set_section_layout(targetSection, &offset, byteLength, count, elementSize);
    }

    *outLength = offset;
    return ZrCore_ZrpMetadata_ValidateHeader(outHeader, *outLength);
}

static TZrBool backend_aot_c_zrp_copy_token_records(TZrByte *targetBlob,
                                                    const SZrZrpMetadataHeader *targetHeader,
                                                    const SZrMetadataTokenRecord *tokenRecords,
                                                    TZrUInt32 tokenRecordCount,
                                                    const SZrZrpMetadataTypeDefRow *typeRows,
                                                    TZrUInt32 typeCount,
                                                    const SZrZrpMetadataTypeSpecRow *typeSpecRows,
                                                    TZrUInt32 typeSpecCount,
                                                    const SZrZrpMetadataModuleRefRow *moduleRefRows,
                                                    TZrUInt32 moduleRefCount,
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrZrpMetadataFieldDefRow *fieldRows,
                                                    TZrUInt32 fieldCount,
                                                    const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                    TZrUInt32 genericParamCount,
                                                    const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                    TZrUInt32 genericParamConstraintCount,
                                                    const SZrAotFunctionTable *functionTable,
                                                    TZrUInt32 retainedMethodDefCount,
                                                    const TZrByte *sourceSignatureBlobPool,
                                                    TZrUInt32 sourceSignatureBlobPoolBytes,
                                                    const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    SZrMetadataTokenRecord *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->tokenRecords.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrMetadataTokenRecord *)(void *)(targetBlob + targetHeader->tokenRecords.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < tokenRecordCount; readIndex++) {
        SZrMetadataTokenRecord record = tokenRecords[readIndex];
        if (!backend_aot_c_zrp_remap_retained_token_record(&record,
                                                           methodRows,
                                                           methodCount,
                                                           fieldRows,
                                                           fieldCount,
                                                           typeRows,
                                                           typeCount,
                                                           tokenRecords,
                                                           tokenRecordCount,
                                                           genericParamRows,
                                                           genericParamCount,
                                                           genericParamConstraintRows,
                                                           genericParamConstraintCount,
                                                           functionTable,
                                                           retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_remap_type_def_tokens_in_record(&record,
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
        if (!backend_aot_c_zrp_remap_type_spec_tokens_in_record(&record,
                                                                typeSpecRows,
                                                                typeSpecCount,
                                                                tokenRecords,
                                                                tokenRecordCount,
                                                                methodRows,
                                                                methodCount,
                                                                fieldRows,
                                                                fieldCount,
                                                                typeRows,
                                                                typeCount,
                                                                genericParamRows,
                                                                genericParamCount,
                                                                genericParamConstraintRows,
                                                                genericParamConstraintCount,
                                                                functionTable,
                                                                retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_remap_module_ref_tokens_in_record(&record,
                                                                 moduleRefRows,
                                                                 moduleRefCount,
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
                                                                 sourceSignatureBlobPool,
                                                                 sourceSignatureBlobPoolBytes,
                                                                 signatureRemap)) {
            continue;
        }
        if (!backend_aot_c_zrp_remap_retained_signature_tokens_in_record(&record,
                                                                         tokenRecords,
                                                                         tokenRecordCount,
                                                                         typeRows,
                                                                         typeCount,
                                                                         typeSpecRows,
                                                                         typeSpecCount,
                                                                         moduleRefRows,
                                                                         moduleRefCount,
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
            return ZR_FALSE;
        }
        if (!backend_aot_c_zrp_remap_signature_blob_offset(&record.signatureBlobOffset,
                                                           record.signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        record.signatureHash = backend_aot_c_zrp_recomputed_signature_hash(targetBlob,
                                                                           targetHeader,
                                                                           record.signatureBlobOffset,
                                                                           record.signatureBlobLength);
        targetRows[writeIndex] = record;
        writeIndex++;
    }

    return ZR_TRUE;
}

static TZrUInt32 backend_aot_c_zrp_count_retained_token_records_for_pruning(
        const SZrMetadataTokenRecord *records,
        TZrUInt32 count,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrZrpMetadataModuleRefRow *moduleRefRows,
        TZrUInt32 moduleRefCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount) {
    TZrUInt32 retainedCount = 0u;

    if (records == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < count; index++) {
        SZrMetadataTokenRecord record = records[index];
        if (backend_aot_c_zrp_remap_retained_token_record(&record,
                                                          methodRows,
                                                          methodCount,
                                                          fieldRows,
                                                          fieldCount,
                                                          typeRows,
                                                          typeCount,
                                                          records,
                                                          count,
                                                          genericParamRows,
                                                          genericParamCount,
                                                          genericParamConstraintRows,
                                                          genericParamConstraintCount,
                                                          functionTable,
                                                          retainedMethodDefCount) &&
            backend_aot_c_zrp_remap_type_def_tokens_in_record(&record,
                                                              typeRows,
                                                              typeCount,
                                                              records,
                                                              count,
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
            backend_aot_c_zrp_remap_type_spec_tokens_in_record(&record,
                                                               typeSpecRows,
                                                               typeSpecCount,
                                                               records,
                                                               count,
                                                               methodRows,
                                                               methodCount,
                                                               fieldRows,
                                                               fieldCount,
                                                               typeRows,
                                                               typeCount,
                                                               genericParamRows,
                                                               genericParamCount,
                                                               genericParamConstraintRows,
                                                               genericParamConstraintCount,
                                                               functionTable,
                                                               retainedMethodDefCount) &&
            backend_aot_c_zrp_remap_module_ref_tokens_in_record(&record,
                                                                moduleRefRows,
                                                                moduleRefCount,
                                                                records,
                                                                count,
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
                                                                ZR_NULL,
                                                                0u,
                                                                ZR_NULL)) {
            retainedCount++;
        }
    }

    return retainedCount;
}

static TZrBool backend_aot_c_zrp_copy_method_defs(TZrByte *targetBlob,
                                                  const SZrZrpMetadataHeader *targetHeader,
                                                  const SZrZrpMetadataMethodDefRow *methodRows,
                                                  TZrUInt32 methodCount,
                                                  const SZrZrpMetadataTypeDefRow *typeRows,
                                                  TZrUInt32 typeCount,
                                                  const SZrMetadataTokenRecord *tokenRecords,
                                                  TZrUInt32 tokenRecordCount,
                                                  const SZrZrpMetadataFieldDefRow *fieldRows,
                                                  TZrUInt32 fieldCount,
                                                  const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                  TZrUInt32 genericParamCount,
                                                  const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
                                                  TZrUInt32 constraintCount,
                                                  const SZrAotFunctionTable *functionTable,
                                                  TZrUInt32 retainedMethodDefCount,
                                                  const SZrAotCZrpSignatureBlobRemap *signatureRemap,
                                                  const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataMethodDefRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->methodDefs.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataMethodDefRow *)(void *)(targetBlob + targetHeader->methodDefs.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < methodCount; readIndex++) {
        if (!backend_aot_c_zrp_method_def_row_is_retained(&methodRows[readIndex], functionTable)) {
            continue;
        }
        targetRows[writeIndex] = methodRows[readIndex];
        targetRows[writeIndex].token =
                backend_aot_c_zrp_compacted_method_def_token(methodRows, methodCount, readIndex, functionTable);
        if (!backend_aot_c_zrp_remap_type_def_token(&targetRows[writeIndex].ownerTypeToken,
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
                                                    constraintRows,
                                                    constraintCount,
                                                    functionTable,
                                                    retainedMethodDefCount)) {
            return ZR_FALSE;
        }
        backend_aot_c_zrp_adjust_generic_param_range(&targetRows[writeIndex].firstGenericParamIndex,
                                                     &targetRows[writeIndex].genericParamCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     typeRows,
                                                     typeCount,
                                                     tokenRecords,
                                                     tokenRecordCount,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
                                                     constraintRows,
                                                     constraintCount,
                                                     functionTable,
                                                     retainedMethodDefCount);
        if (!backend_aot_c_zrp_remap_method_def_string_offsets(&targetRows[writeIndex], stringRemap) ||
            !backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[writeIndex].signatureBlobOffset,
                                                           targetRows[writeIndex].signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        writeIndex++;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_field_defs(TZrByte *targetBlob,
                                                 const SZrZrpMetadataHeader *targetHeader,
                                                 const SZrZrpMetadataFieldDefRow *fieldRows,
                                                 TZrUInt32 fieldCount,
                                                 const SZrZrpMetadataTypeDefRow *typeRows,
                                                 TZrUInt32 typeCount,
                                                 const SZrMetadataTokenRecord *tokenRecords,
                                                 TZrUInt32 tokenRecordCount,
                                                 const SZrZrpMetadataMethodDefRow *methodRows,
                                                 TZrUInt32 methodCount,
                                                 const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                 TZrUInt32 genericParamCount,
                                                 const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
                                                 TZrUInt32 constraintCount,
                                                 const SZrAotFunctionTable *functionTable,
                                                 TZrUInt32 retainedMethodDefCount,
                                                 const SZrAotCZrpSignatureBlobRemap *signatureRemap,
                                                 const SZrAotCZrpStringPoolRemap *stringRemap,
                                                 const SZrAotCZrpConstantPoolRemap *constantPoolRemap) {
    SZrZrpMetadataFieldDefRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->fieldDefs.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataFieldDefRow *)(void *)(targetBlob + targetHeader->fieldDefs.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < fieldCount; readIndex++) {
        if (!backend_aot_c_zrp_field_def_row_is_retained(&fieldRows[readIndex],
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
                                                         constraintRows,
                                                         constraintCount,
                                                         functionTable,
                                                         retainedMethodDefCount)) {
            continue;
        }
        targetRows[writeIndex] = fieldRows[readIndex];
        targetRows[writeIndex].token =
                backend_aot_c_zrp_compacted_retained_field_def_token(retainedMethodDefCount,
                                                                     fieldRows,
                                                                     fieldCount,
                                                                     readIndex,
                                                                     typeRows,
                                                                     typeCount,
                                                                     tokenRecords,
                                                                     tokenRecordCount,
                                                                     methodRows,
                                                                     methodCount,
                                                                     genericParamRows,
                                                                     genericParamCount,
                                                                     constraintRows,
                                                                     constraintCount,
                                                                     functionTable);
        if (!backend_aot_c_zrp_remap_type_def_token(&targetRows[writeIndex].ownerTypeToken,
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
                                                    constraintRows,
                                                    constraintCount,
                                                    functionTable,
                                                    retainedMethodDefCount) ||
            !backend_aot_c_zrp_remap_field_def_string_offsets(&targetRows[writeIndex], stringRemap) ||
            !backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[writeIndex].signatureBlobOffset,
                                                           targetRows[writeIndex].signatureBlobLength,
                                                           signatureRemap) ||
            !backend_aot_c_zrp_remap_field_def_default_value_constant_pool_slice(&targetRows[writeIndex],
                                                                                 constantPoolRemap)) {
            return ZR_FALSE;
        }
        writeIndex++;
    }

    return (TZrBool)(writeIndex == targetHeader->fieldDefs.count);
}

static TZrBool backend_aot_c_zrp_copy_generic_params(TZrByte *targetBlob,
                                                     const SZrZrpMetadataHeader *targetHeader,
                                                     const SZrZrpMetadataTypeDefRow *typeRows,
                                                     TZrUInt32 typeCount,
                                                     const SZrMetadataTokenRecord *tokenRecords,
                                                     TZrUInt32 tokenRecordCount,
                                                     const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                     TZrUInt32 genericParamCount,
                                                     const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
                                                     TZrUInt32 constraintCount,
                                                     const SZrZrpMetadataMethodDefRow *methodRows,
                                                     TZrUInt32 methodCount,
                                                     const SZrZrpMetadataFieldDefRow *fieldRows,
                                                     TZrUInt32 fieldCount,
                                                     const SZrAotFunctionTable *functionTable,
                                                     TZrUInt32 retainedMethodDefCount,
                                                     const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataGenericParamRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->genericParams.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataGenericParamRow *)(void *)(targetBlob + targetHeader->genericParams.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < genericParamCount; readIndex++) {
        SZrZrpMetadataGenericParamRow row = genericParamRows[readIndex];
        if (!backend_aot_c_zrp_remap_generic_param_owner_token(&row.ownerToken,
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
                                                               constraintRows,
                                                               constraintCount,
                                                               functionTable,
                                                               retainedMethodDefCount)) {
            continue;
        }
        backend_aot_c_zrp_adjust_generic_param_constraint_range(&row.firstConstraintIndex,
                                                                &row.constraintCount,
                                                                constraintRows,
                                                                constraintCount,
                                                                genericParamRows,
                                                                genericParamCount,
                                                                typeRows,
                                                                typeCount,
                                                                tokenRecords,
                                                                tokenRecordCount,
                                                                methodRows,
                                                                methodCount,
                                                                fieldRows,
                                                                fieldCount,
                                                                functionTable,
                                                                retainedMethodDefCount);
        if (!backend_aot_c_zrp_remap_generic_param_string_offsets(&row, stringRemap)) {
            return ZR_FALSE;
        }
        targetRows[writeIndex] = row;
        writeIndex++;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_generic_param_constraints(
        TZrByte *targetBlob,
        const SZrZrpMetadataHeader *targetHeader,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrZrpMetadataTypeSpecRow *typeSpecRows,
        TZrUInt32 typeSpecCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
        TZrUInt32 constraintCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    SZrZrpMetadataGenericParamConstraintRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->genericParamConstraints.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataGenericParamConstraintRow *)(void *)(targetBlob +
                                                                     targetHeader->genericParamConstraints.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < constraintCount; readIndex++) {
        SZrZrpMetadataGenericParamConstraintRow row = constraintRows[readIndex];
        if (!backend_aot_c_zrp_remap_generic_param_constraint_row(&row,
                                                                  genericParamRows,
                                                                  genericParamCount,
                                                                  typeRows,
                                                                  typeCount,
                                                                  tokenRecords,
                                                                  tokenRecordCount,
                                                                  constraintRows,
                                                                  constraintCount,
                                                                  methodRows,
                                                                  methodCount,
                                                                  fieldRows,
                                                                  fieldCount,
                                                                  functionTable,
                                                                  retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_remap_type_def_token(&row.constraintTypeToken,
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
                                                    constraintRows,
                                                    constraintCount,
                                                    functionTable,
                                                    retainedMethodDefCount)) {
            return ZR_FALSE;
        }
        if (!backend_aot_c_zrp_remap_type_spec_token(&row.constraintTypeToken,
                                                     typeSpecRows,
                                                     typeSpecCount,
                                                     tokenRecords,
                                                     tokenRecordCount,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
                                                     typeRows,
                                                     typeCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     constraintRows,
                                                     constraintCount,
                                                     functionTable,
                                                     retainedMethodDefCount)) {
            return ZR_FALSE;
        }
        if (!backend_aot_c_zrp_remap_signature_blob_offset(&row.signatureBlobOffset,
                                                           row.signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        targetRows[writeIndex] = row;
        writeIndex++;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_method_specs(TZrByte *targetBlob,
                                                   const SZrZrpMetadataHeader *targetHeader,
                                                   const SZrZrpMetadataMethodSpecRow *methodSpecRows,
                                                   TZrUInt32 methodSpecCount,
                                                   const SZrMetadataTokenRecord *tokenRecords,
                                                   TZrUInt32 tokenRecordCount,
                                                   const SZrZrpMetadataTypeDefRow *typeRows,
                                                   TZrUInt32 typeCount,
                                                   const SZrZrpMetadataTypeSpecRow *typeSpecRows,
                                                   TZrUInt32 typeSpecCount,
                                                   const SZrZrpMetadataModuleRefRow *moduleRefRows,
                                                   TZrUInt32 moduleRefCount,
                                                   const SZrZrpMetadataMethodDefRow *methodRows,
                                                   TZrUInt32 methodCount,
                                                   const SZrZrpMetadataFieldDefRow *fieldRows,
                                                   TZrUInt32 fieldCount,
                                                   const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                   TZrUInt32 genericParamCount,
                                                   const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                   TZrUInt32 genericParamConstraintCount,
                                                   const SZrAotFunctionTable *functionTable,
                                                   TZrUInt32 retainedMethodDefCount,
                                                   const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    SZrZrpMetadataMethodSpecRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->methodSpecs.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataMethodSpecRow *)(void *)(targetBlob + targetHeader->methodSpecs.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < methodSpecCount; readIndex++) {
        SZrZrpMetadataMethodSpecRow row = methodSpecRows[readIndex];
        if (!backend_aot_c_zrp_remap_method_spec_row(&row,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
                                                     typeRows,
                                                     typeCount,
                                                     tokenRecords,
                                                     tokenRecordCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     genericParamConstraintRows,
                                                     genericParamConstraintCount,
                                                     functionTable,
                                                     retainedMethodDefCount)) {
            continue;
        }
        if (!backend_aot_c_zrp_remap_retained_signature_token(&row.token,
                                                              tokenRecords,
                                                              tokenRecordCount,
                                                              typeRows,
                                                              typeCount,
                                                              typeSpecRows,
                                                              typeSpecCount,
                                                              moduleRefRows,
                                                              moduleRefCount,
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
            return ZR_FALSE;
        }
        if (!backend_aot_c_zrp_remap_signature_blob_offset(&row.instantiationBlobOffset,
                                                           row.instantiationBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        row.instantiationHash = backend_aot_c_zrp_recomputed_signature_hash(targetBlob,
                                                                            targetHeader,
                                                                            row.instantiationBlobOffset,
                                                                            row.instantiationBlobLength);
        targetRows[writeIndex] = row;
        writeIndex++;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_module_refs(TZrByte *targetBlob,
                                                  const TZrByte *sourceBlob,
                                                  const SZrZrpMetadataHeader *sourceHeader,
                                                  const SZrZrpMetadataHeader *targetHeader,
                                                  const SZrZrpMetadataModuleRefRow *moduleRefRows,
                                                  TZrUInt32 moduleRefCount,
                                                  const SZrMetadataTokenRecord *tokenRecords,
                                                  TZrUInt32 tokenRecordCount,
                                                  const SZrZrpMetadataTypeSpecRow *typeSpecRows,
                                                  TZrUInt32 typeSpecCount,
                                                  const SZrZrpMetadataTypeDefRow *typeRows,
                                                  TZrUInt32 typeCount,
                                                  const SZrZrpMetadataMethodDefRow *methodRows,
                                                  TZrUInt32 methodCount,
                                                  const SZrZrpMetadataFieldDefRow *fieldRows,
                                                  TZrUInt32 fieldCount,
                                                  const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                  TZrUInt32 genericParamCount,
                                                  const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
                                                  TZrUInt32 genericParamConstraintCount,
                                                  const SZrAotFunctionTable *functionTable,
                                                  TZrUInt32 retainedMethodDefCount,
                                                  const TZrByte *sourceSignatureBlobPool,
                                                  TZrUInt32 sourceSignatureBlobPoolBytes,
                                                  const SZrAotCZrpSignatureBlobRemap *signatureRemap,
                                                  const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataModuleRefRow *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->moduleRefs.byteLength == 0u) {
        return ZR_TRUE;
    }

    (void)sourceBlob;
    (void)sourceHeader;
    targetRows = (SZrZrpMetadataModuleRefRow *)(void *)(targetBlob + targetHeader->moduleRefs.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < moduleRefCount; readIndex++) {
        if (!backend_aot_c_zrp_module_ref_row_is_retained(&moduleRefRows[readIndex],
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
                                                          sourceSignatureBlobPool,
                                                          sourceSignatureBlobPoolBytes,
                                                          signatureRemap)) {
            continue;
        }
        targetRows[writeIndex] = moduleRefRows[readIndex];
        targetRows[writeIndex].token =
                backend_aot_c_zrp_compacted_module_ref_token(moduleRefRows,
                                                             moduleRefCount,
                                                             readIndex,
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
                                                             sourceSignatureBlobPool,
                                                             sourceSignatureBlobPoolBytes,
                                                             signatureRemap);
        if (!backend_aot_c_zrp_remap_module_ref_string_offsets(&targetRows[writeIndex], stringRemap)) {
            return ZR_FALSE;
        }
        writeIndex++;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_prune_method_def_metadata_blob(const SZrAotWriterOptions *options,
                                                            const SZrAotFunctionTable *functionTable,
                                                            SZrAotCEmbeddedZrpMetadata *outMetadata) {
    SZrZrpMetadataHeader sourceHeader;
    SZrZrpMetadataHeader targetHeader;
    SZrZrpMetadataSectionView tokenRecordView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView fieldView;
    SZrZrpMetadataSectionView genericParamView;
    SZrZrpMetadataSectionView genericParamConstraintView;
    SZrZrpMetadataSectionView typeSpecView;
    SZrZrpMetadataSectionView methodSpecView;
    SZrZrpMetadataSectionView moduleRefView;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView signatureBlobView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeRows;
    const SZrZrpMetadataMethodDefRow *methodRows;
    const SZrZrpMetadataFieldDefRow *fieldRows;
    const SZrZrpMetadataGenericParamRow *genericParamRows;
    const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraints;
    const SZrZrpMetadataTypeSpecRow *typeSpecs;
    const SZrZrpMetadataMethodSpecRow *methodSpecs;
    const SZrZrpMetadataModuleRefRow *moduleRefs;
    const SZrZrpMetadataManifestExportRow *manifestExports;
    SZrAotCZrpConstantPoolRemap constantPoolRemap;
    SZrAotCZrpSignatureBlobRemap signatureRemap;
    SZrAotCZrpStringPoolRemap stringRemap;
    TZrUInt32 constantPoolRemapCapacity;
    TZrUInt32 signatureRemapCapacity;
    TZrUInt32 stringRemapCapacity;
    TZrUInt32 retainedTokenRecordCount;
    TZrUInt32 retainedTypeDefCount;
    TZrUInt32 retainedMethodDefCount;
    TZrUInt32 retainedFieldDefCount;
    TZrUInt32 retainedGenericParamCount;
    TZrUInt32 retainedGenericParamConstraintCount;
    TZrUInt32 retainedTypeSpecCount;
    TZrUInt32 retainedMethodSpecCount;
    TZrUInt32 retainedModuleRefCount;
    TZrSize prunedLength = 0u;
    TZrByte *prunedBlob;

    if (options == ZR_NULL ||
        options->embeddedModuleBlob == ZR_NULL ||
        options->embeddedModuleBlobLength == 0u ||
        !ZrCore_ZrpMetadata_ReadHeader(options->embeddedModuleBlob,
                                       options->embeddedModuleBlobLength,
                                       &sourceHeader) ||
        !backend_aot_c_zrp_can_prune_method_defs(&sourceHeader) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                           &tokenRecordView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                           &typeView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
                                           &methodView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                           &fieldView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS,
                                           &genericParamView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                                 options->embeddedModuleBlobLength,
                                                 &sourceHeader,
                                                 ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                                 &genericParamConstraintView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                           &typeSpecView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_METHOD_SPECS,
                                           &methodSpecView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_MODULE_REFS,
                                           &moduleRefView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                           &stringPoolView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                           &signatureBlobView) ||
        !ZrCore_ZrpMetadata_GetSectionView(options->embeddedModuleBlob,
                                           options->embeddedModuleBlobLength,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                           &manifestExportView)) {
        return ZR_TRUE;
    }

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenRecordView.data;
    typeRows = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodRows = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    fieldRows = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;
    genericParamRows = (const SZrZrpMetadataGenericParamRow *)(const void *)genericParamView.data;
    genericParamConstraints =
            (const SZrZrpMetadataGenericParamConstraintRow *)(const void *)genericParamConstraintView.data;
    typeSpecs = (const SZrZrpMetadataTypeSpecRow *)(const void *)typeSpecView.data;
    methodSpecs = (const SZrZrpMetadataMethodSpecRow *)(const void *)methodSpecView.data;
    moduleRefs = (const SZrZrpMetadataModuleRefRow *)(const void *)moduleRefView.data;
    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;
    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodRows, methodView.count, functionTable);
    retainedTypeDefCount =
            backend_aot_c_zrp_count_retained_type_defs(typeRows,
                                                       typeView.count,
                                                       tokenRecords,
                                                       tokenRecordView.count,
                                                       methodRows,
                                                       methodView.count,
                                                       fieldRows,
                                                       fieldView.count,
                                                       genericParamRows,
                                                       genericParamView.count,
                                                       genericParamConstraints,
                                                       genericParamConstraintView.count,
                                                       functionTable,
                                                       retainedMethodDefCount);
    retainedFieldDefCount =
            backend_aot_c_zrp_count_retained_field_defs(fieldRows,
                                                        fieldView.count,
                                                        typeRows,
                                                        typeView.count,
                                                        tokenRecords,
                                                        tokenRecordView.count,
                                                        methodRows,
                                                        methodView.count,
                                                        genericParamRows,
                                                        genericParamView.count,
                                                        genericParamConstraints,
                                                        genericParamConstraintView.count,
                                                        functionTable,
                                                        retainedMethodDefCount);
    retainedGenericParamCount =
            backend_aot_c_zrp_count_retained_generic_params(genericParamRows,
                                                            genericParamView.count,
                                                            typeRows,
                                                            typeView.count,
                                                            tokenRecords,
                                                            tokenRecordView.count,
                                                            methodRows,
                                                            methodView.count,
                                                            fieldRows,
                                                            fieldView.count,
                                                            genericParamRows,
                                                            genericParamView.count,
                                                            genericParamConstraints,
                                                            genericParamConstraintView.count,
                                                            functionTable,
                                                            retainedMethodDefCount);
    retainedGenericParamConstraintCount =
            backend_aot_c_zrp_count_retained_generic_param_constraints(genericParamConstraints,
                                                                       genericParamConstraintView.count,
                                                                       genericParamRows,
                                                                       genericParamView.count,
                                                                       typeRows,
                                                                       typeView.count,
                                                                       tokenRecords,
                                                                       tokenRecordView.count,
                                                                       methodRows,
                                                                       methodView.count,
                                                                       fieldRows,
                                                                       fieldView.count,
                                                                       genericParamConstraints,
                                                                       genericParamConstraintView.count,
                                                                       functionTable,
                                                                       retainedMethodDefCount);
    retainedTypeSpecCount =
            backend_aot_c_zrp_count_retained_type_specs(typeSpecs,
                                                        typeSpecView.count,
                                                        tokenRecords,
                                                        tokenRecordView.count,
                                                        methodRows,
                                                        methodView.count,
                                                        fieldRows,
                                                        fieldView.count,
                                                        typeRows,
                                                        typeView.count,
                                                        genericParamRows,
                                                        genericParamView.count,
                                                        genericParamConstraints,
                                                        genericParamConstraintView.count,
                                                        functionTable,
                                                        retainedMethodDefCount);
    retainedTokenRecordCount =
            backend_aot_c_zrp_count_retained_token_records_for_pruning(tokenRecords,
                                                                       tokenRecordView.count,
                                                                       typeRows,
                                                                       typeView.count,
                                                                       typeSpecs,
                                                                       typeSpecView.count,
                                                                       moduleRefs,
                                                                       moduleRefView.count,
                                                                       methodRows,
                                                                       methodView.count,
                                                                       fieldRows,
                                                                       fieldView.count,
                                                                       genericParamRows,
                                                                       genericParamView.count,
                                                                       genericParamConstraints,
                                                                       genericParamConstraintView.count,
                                                                       functionTable,
                                                                       retainedMethodDefCount);
    retainedMethodSpecCount =
            backend_aot_c_zrp_count_retained_method_specs(methodSpecs,
                                                          methodSpecView.count,
                                                          methodRows,
                                                          methodView.count,
                                                          fieldRows,
                                                          fieldView.count,
                                                          typeRows,
                                                          typeView.count,
                                                          tokenRecords,
                                                          tokenRecordView.count,
                                                          genericParamRows,
                                                          genericParamView.count,
                                                          genericParamConstraints,
                                                          genericParamConstraintView.count,
                                                          functionTable,
                                                          retainedMethodDefCount);

    signatureRemapCapacity = tokenRecordView.count +
                             retainedTypeDefCount +
                             methodView.count +
                             retainedFieldDefCount +
                             genericParamConstraintView.count +
                             typeSpecView.count +
                             methodSpecView.count;
    if (!backend_aot_c_zrp_signature_blob_remap_init(&signatureRemap,
                                                     signatureRemapCapacity,
                                                     (TZrUInt32)signatureBlobView.byteLength)) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_build_signature_blob_remap(&signatureRemap,
                                                      tokenRecords,
                                                       tokenRecordView.count,
                                                       typeRows,
                                                       typeView.count,
                                                       retainedTypeDefCount,
                                                       methodRows,
                                                       methodView.count,
                                                      fieldRows,
                                                      fieldView.count,
                                                      genericParamRows,
                                                      genericParamView.count,
                                                      genericParamConstraints,
                                                      genericParamConstraintView.count,
                                                      typeSpecs,
                                                      typeSpecView.count,
                                                      moduleRefs,
                                                      moduleRefView.count,
                                                      methodSpecs,
                                                      methodSpecView.count,
                                                      functionTable,
                                                      retainedMethodDefCount)) {
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    retainedModuleRefCount =
            backend_aot_c_zrp_count_retained_module_refs(moduleRefs,
                                                         moduleRefView.count,
                                                         tokenRecords,
                                                          tokenRecordView.count,
                                                          typeSpecs,
                                                          typeSpecView.count,
                                                          typeRows,
                                                          typeView.count,
                                                          methodRows,
                                                         methodView.count,
                                                         fieldRows,
                                                         fieldView.count,
                                                         genericParamRows,
                                                         genericParamView.count,
                                                         genericParamConstraints,
                                                         genericParamConstraintView.count,
                                                         functionTable,
                                                         retainedMethodDefCount,
                                                         signatureBlobView.data,
                                                         (TZrUInt32)signatureBlobView.byteLength,
                                                         &signatureRemap);

    constantPoolRemapCapacity = retainedFieldDefCount;
    if (!backend_aot_c_zrp_constant_pool_remap_init(&constantPoolRemap,
                                                    constantPoolRemapCapacity,
                                                    sourceHeader.constantPool.byteLength)) {
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_build_constant_pool_remap(&constantPoolRemap,
                                                     fieldRows,
                                                     fieldView.count,
                                                     typeRows,
                                                     typeView.count,
                                                     tokenRecords,
                                                     tokenRecordView.count,
                                                     methodRows,
                                                     methodView.count,
                                                     genericParamRows,
                                                     genericParamView.count,
                                                     genericParamConstraints,
                                                     genericParamConstraintView.count,
                                                     functionTable,
                                                     retainedMethodDefCount)) {
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    stringRemapCapacity = (retainedTypeDefCount * 2u) +
                           methodView.count +
                           retainedFieldDefCount +
                          genericParamView.count +
                          (retainedModuleRefCount * 2u) +
                          manifestExportView.count;
    if (!backend_aot_c_zrp_string_pool_remap_init(&stringRemap,
                                                  stringRemapCapacity,
                                                  (TZrUInt32)stringPoolView.byteLength)) {
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_build_string_pool_remap(&stringRemap,
                                                    stringPoolView.data,
                                                    (TZrUInt32)stringPoolView.byteLength,
                                                    typeRows,
                                                    typeView.count,
                                                    retainedTypeDefCount,
                                                    methodRows,
                                                    methodView.count,
                                                   fieldRows,
                                                   fieldView.count,
                                                   genericParamRows,
                                                   genericParamView.count,
                                                   genericParamConstraints,
                                                   genericParamConstraintView.count,
                                                   moduleRefs,
                                                   moduleRefView.count,
                                                   manifestExports,
                                                   manifestExportView.count,
                                                   tokenRecords,
                                                   tokenRecordView.count,
                                                   typeSpecs,
                                                   typeSpecView.count,
                                                   functionTable,
                                                   retainedMethodDefCount,
                                                   signatureBlobView.data,
                                                   (TZrUInt32)signatureBlobView.byteLength,
                                                   &signatureRemap)) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    if (retainedTokenRecordCount == tokenRecordView.count &&
        retainedTypeDefCount == typeView.count &&
        retainedMethodDefCount == methodView.count &&
        retainedFieldDefCount == fieldView.count &&
        retainedGenericParamCount == genericParamView.count &&
        retainedGenericParamConstraintCount == genericParamConstraintView.count &&
        retainedTypeSpecCount == typeSpecView.count &&
        retainedMethodSpecCount == methodSpecView.count &&
        retainedModuleRefCount == moduleRefView.count &&
        backend_aot_c_zrp_constant_pool_remap_is_identity(&constantPoolRemap) &&
        backend_aot_c_zrp_signature_blob_remap_is_identity(&signatureRemap) &&
        backend_aot_c_zrp_string_pool_remap_is_identity(&stringRemap)) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_TRUE;
    }

    if (!backend_aot_c_zrp_build_pruned_header(&sourceHeader,
                                               retainedTokenRecordCount,
                                               retainedTypeDefCount,
                                               retainedMethodDefCount,
                                               retainedFieldDefCount,
                                               retainedGenericParamCount,
                                               retainedGenericParamConstraintCount,
                                               retainedTypeSpecCount,
                                               retainedMethodSpecCount,
                                               retainedModuleRefCount,
                                               stringRemap.byteLength,
                                               signatureRemap.byteLength,
                                               constantPoolRemap.byteLength,
                                               &targetHeader,
                                               &prunedLength)) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    prunedBlob = (TZrByte *)malloc(prunedLength);
    if (prunedBlob == ZR_NULL) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    memset(prunedBlob, 0, prunedLength);
    if (!ZrCore_ZrpMetadata_WriteHeader(prunedBlob, prunedLength, &targetHeader)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    backend_aot_c_zrp_copy_string_pool(prunedBlob,
                                       options->embeddedModuleBlob,
                                       &sourceHeader,
                                       &targetHeader,
                                       &stringRemap);
    backend_aot_c_zrp_copy_signature_blob_pool(prunedBlob,
                                               options->embeddedModuleBlob,
                                               &sourceHeader,
                                               &targetHeader,
                                               &signatureRemap);
    if (!backend_aot_c_zrp_copy_constant_pool(prunedBlob,
                                              options->embeddedModuleBlob,
                                              &sourceHeader,
                                              &targetHeader,
                                              &constantPoolRemap)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_rewrite_retained_signature_type_def_tokens(prunedBlob,
                                                                       &targetHeader,
                                                                       typeRows,
                                                                       typeView.count,
                                                                       typeSpecs,
                                                                       typeSpecView.count,
                                                                       moduleRefs,
                                                                       moduleRefView.count,
                                                                       tokenRecords,
                                                                       tokenRecordView.count,
                                                                       methodRows,
                                                                      methodView.count,
                                                                      fieldRows,
                                                                      fieldView.count,
                                                                      genericParamRows,
                                                                      genericParamView.count,
                                                                      genericParamConstraints,
                                                                      genericParamConstraintView.count,
                                                                      functionTable,
                                                                     retainedMethodDefCount,
                                                                     signatureBlobView.data,
                                                                     (TZrUInt32)signatureBlobView.byteLength,
                                                                     &signatureRemap,
                                                                     &stringRemap)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_rewrite_retained_method_spec_signature_blobs(prunedBlob,
                                                                        &targetHeader,
                                                                        methodSpecs,
                                                                        methodSpecView.count,
                                                                        methodRows,
                                                                        methodView.count,
                                                                        fieldRows,
                                                                        fieldView.count,
                                                                        typeRows,
                                                                        typeView.count,
                                                                        tokenRecords,
                                                                        tokenRecordView.count,
                                                                        genericParamRows,
                                                                        genericParamView.count,
                                                                        genericParamConstraints,
                                                                        genericParamConstraintView.count,
                                                                        functionTable,
                                                                        retainedMethodDefCount,
                                                                        &signatureRemap)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    for (TZrUInt32 sectionKind = 0u; sectionKind < ZR_ZRP_METADATA_SECTION_COUNT; sectionKind++) {
        if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS) {
            if (!backend_aot_c_zrp_copy_token_records(prunedBlob,
                                                      &targetHeader,
                                                      tokenRecords,
                                                      tokenRecordView.count,
                                                      typeRows,
                                                      typeView.count,
                                                      typeSpecs,
                                                      typeSpecView.count,
                                                      moduleRefs,
                                                      moduleRefView.count,
                                                      methodRows,
                                                      methodView.count,
                                                      fieldRows,
                                                      fieldView.count,
                                                      genericParamRows,
                                                      genericParamView.count,
                                                      genericParamConstraints,
                                                      genericParamConstraintView.count,
                                                      functionTable,
                                                      retainedMethodDefCount,
                                                      signatureBlobView.data,
                                                      (TZrUInt32)signatureBlobView.byteLength,
                                                      &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TYPE_DEFS) {
            if (!backend_aot_c_zrp_copy_type_defs(prunedBlob,
                                                  &targetHeader,
                                                  typeRows,
                                                  typeView.count,
                                                  tokenRecords,
                                                  tokenRecordView.count,
                                                  methodRows,
                                                  methodView.count,
                                                  fieldRows,
                                                  fieldView.count,
                                                  genericParamRows,
                                                  genericParamView.count,
                                                  genericParamConstraints,
                                                  genericParamConstraintView.count,
                                                  functionTable,
                                                  retainedMethodDefCount,
                                                  &signatureRemap,
                                                  &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_DEFS) {
            if (!backend_aot_c_zrp_copy_method_defs(prunedBlob,
                                                    &targetHeader,
                                                    methodRows,
                                                    methodView.count,
                                                    typeRows,
                                                    typeView.count,
                                                    tokenRecords,
                                                    tokenRecordView.count,
                                                    fieldRows,
                                                    fieldView.count,
                                                    genericParamRows,
                                                    genericParamView.count,
                                                    genericParamConstraints,
                                                    genericParamConstraintView.count,
                                                    functionTable,
                                                    retainedMethodDefCount,
                                                    &signatureRemap,
                                                    &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_FIELD_DEFS) {
            if (!backend_aot_c_zrp_copy_field_defs(prunedBlob,
                                                   &targetHeader,
                                                   fieldRows,
                                                   fieldView.count,
                                                   typeRows,
                                                   typeView.count,
                                                   tokenRecords,
                                                   tokenRecordView.count,
                                                   methodRows,
                                                   methodView.count,
                                                   genericParamRows,
                                                   genericParamView.count,
                                                   genericParamConstraints,
                                                   genericParamConstraintView.count,
                                                   functionTable,
                                                   retainedMethodDefCount,
                                                   &signatureRemap,
                                                   &stringRemap,
                                                   &constantPoolRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS) {
            if (!backend_aot_c_zrp_copy_generic_params(prunedBlob,
                                                       &targetHeader,
                                                       typeRows,
                                                       typeView.count,
                                                       tokenRecords,
                                                       tokenRecordView.count,
                                                       genericParamRows,
                                                       genericParamView.count,
                                                       genericParamConstraints,
                                                       genericParamConstraintView.count,
                                                       methodRows,
                                                       methodView.count,
                                                       fieldRows,
                                                       fieldView.count,
                                                       functionTable,
                                                       retainedMethodDefCount,
                                                       &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS) {
            if (!backend_aot_c_zrp_copy_generic_param_constraints(prunedBlob,
                                                                  &targetHeader,
                                                                  typeRows,
                                                                  typeView.count,
                                                                  typeSpecs,
                                                                  typeSpecView.count,
                                                                  tokenRecords,
                                                                  tokenRecordView.count,
                                                                  genericParamConstraints,
                                                                  genericParamConstraintView.count,
                                                                  genericParamRows,
                                                                  genericParamView.count,
                                                                  methodRows,
                                                                  methodView.count,
                                                                  fieldRows,
                                                                  fieldView.count,
                                                                  functionTable,
                                                                  retainedMethodDefCount,
                                                                  &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TYPE_SPECS) {
            if (!backend_aot_c_zrp_copy_type_specs(prunedBlob,
                                                   &targetHeader,
                                                   typeSpecs,
                                                   typeSpecView.count,
                                                   tokenRecords,
                                                   tokenRecordView.count,
                                                   methodRows,
                                                   methodView.count,
                                                   fieldRows,
                                                   fieldView.count,
                                                   typeRows,
                                                   typeView.count,
                                                   genericParamRows,
                                                   genericParamView.count,
                                                   genericParamConstraints,
                                                   genericParamConstraintView.count,
                                                   functionTable,
                                                   retainedMethodDefCount,
                                                   &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_SPECS) {
            if (!backend_aot_c_zrp_copy_method_specs(prunedBlob,
                                                    &targetHeader,
                                                    methodSpecs,
                                                    methodSpecView.count,
                                                    tokenRecords,
                                                    tokenRecordView.count,
                                                    typeRows,
                                                    typeView.count,
                                                    typeSpecs,
                                                    typeSpecView.count,
                                                    moduleRefs,
                                                    moduleRefView.count,
                                                    methodRows,
                                                    methodView.count,
                                                    fieldRows,
                                                    fieldView.count,
                                                    genericParamRows,
                                                    genericParamView.count,
                                                    genericParamConstraints,
                                                    genericParamConstraintView.count,
                                                    functionTable,
                                                    retainedMethodDefCount,
                                                    &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_MODULE_REFS) {
            if (!backend_aot_c_zrp_copy_module_refs(prunedBlob,
                                                    options->embeddedModuleBlob,
                                                    &sourceHeader,
                                                    &targetHeader,
                                                    moduleRefs,
                                                    moduleRefView.count,
                                                    tokenRecords,
                                                     tokenRecordView.count,
                                                     typeSpecs,
                                                     typeSpecView.count,
                                                     typeRows,
                                                     typeView.count,
                                                     methodRows,
                                                    methodView.count,
                                                    fieldRows,
                                                    fieldView.count,
                                                    genericParamRows,
                                                    genericParamView.count,
                                                    genericParamConstraints,
                                                    genericParamConstraintView.count,
                                                    functionTable,
                                                    retainedMethodDefCount,
                                                    signatureBlobView.data,
                                                    (TZrUInt32)signatureBlobView.byteLength,
                                                    &signatureRemap,
                                                    &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_STRING_POOL) {
            /* Already copied from compacted string remap before row rewrites. */
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL) {
            /* Already copied before row rewrites so signature hashes can be recomputed from final bytes. */
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_CONSTANT_POOL) {
            /* Already copied from compacted constant remap before row rewrites. */
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS) {
            if (!backend_aot_c_zrp_copy_manifest_exports(prunedBlob,
                                                         &targetHeader,
                                                         manifestExports,
                                                         manifestExportView.count,
                                                         tokenRecords,
                                                         tokenRecordView.count,
                                                         typeRows,
                                                         typeView.count,
                                                         methodRows,
                                                         methodView.count,
                                                         fieldRows,
                                                         fieldView.count,
                                                         genericParamRows,
                                                         genericParamView.count,
                                                         genericParamConstraints,
                                                         genericParamConstraintView.count,
                                                         functionTable,
                                                         retainedMethodDefCount,
                                                         &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else {
            backend_aot_c_zrp_copy_section_if_needed(prunedBlob,
                                                     options->embeddedModuleBlob,
                                                     &sourceHeader,
                                                     &targetHeader,
                                                     (EZrZrpMetadataSectionKind)sectionKind);
        }
    }

    if (!backend_aot_c_zrp_member_token_remap_build(outMetadata,
                                                    methodRows,
                                                    methodView.count,
                                                    fieldRows,
                                                    fieldView.count,
                                                    typeRows,
                                                    typeView.count,
                                                    tokenRecords,
                                                    tokenRecordView.count,
                                                    genericParamRows,
                                                    genericParamView.count,
                                                    genericParamConstraints,
                                                    genericParamConstraintView.count,
                                                    functionTable,
                                                    retainedMethodDefCount,
                                                    retainedFieldDefCount)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_type_def_token_remap_build(outMetadata,
                                                      typeRows,
                                                      typeView.count,
                                                      tokenRecords,
                                                      tokenRecordView.count,
                                                      methodRows,
                                                      methodView.count,
                                                      fieldRows,
                                                      fieldView.count,
                                                      genericParamRows,
                                                      genericParamView.count,
                                                      genericParamConstraints,
                                                      genericParamConstraintView.count,
                                                      functionTable,
                                                      retainedMethodDefCount,
                                                      retainedTypeDefCount)) {
        free(prunedBlob);
        backend_aot_c_zrp_member_token_remap_destroy(outMetadata);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    outMetadata->blob = prunedBlob;
    outMetadata->length = prunedLength;
    outMetadata->ownedBlob = prunedBlob;
    backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
    backend_aot_c_zrp_constant_pool_remap_destroy(&constantPoolRemap);
    backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
    return ZR_TRUE;
}

TZrBool backend_aot_c_prepare_embedded_zrp_metadata(const SZrAotWriterOptions *options,
                                                    TZrBool enableCodeStripping,
                                                    const SZrAotFunctionTable *functionTable,
                                                    SZrAotCEmbeddedZrpMetadata *outMetadata) {
    if (outMetadata == ZR_NULL) {
        return ZR_FALSE;
    }

    outMetadata->blob = options != ZR_NULL ? options->embeddedModuleBlob : ZR_NULL;
    outMetadata->length = options != ZR_NULL ? options->embeddedModuleBlobLength : 0u;
    outMetadata->ownedBlob = ZR_NULL;
    outMetadata->hasTypeDefTokenRemap = ZR_FALSE;
    outMetadata->typeDefTokenRemapEntries = ZR_NULL;
    outMetadata->typeDefTokenRemapCount = 0u;
    outMetadata->ownedTypeDefTokenRemapEntries = ZR_NULL;
    outMetadata->memberTokenRemapEntries = ZR_NULL;
    outMetadata->memberTokenRemapCount = 0u;
    outMetadata->ownedMemberTokenRemapEntries = ZR_NULL;
    outMetadata->manifestExportEntries = ZR_NULL;
    outMetadata->manifestExportCount = 0u;
    outMetadata->ownedManifestExportEntries = ZR_NULL;

    if (enableCodeStripping &&
        outMetadata->blob != ZR_NULL &&
        outMetadata->length != 0u &&
        !backend_aot_c_prune_method_def_metadata_blob(options, functionTable, outMetadata)) {
        backend_aot_c_release_embedded_zrp_metadata(outMetadata);
        return ZR_FALSE;
    }

    if (!backend_aot_c_zrp_publish_manifest_export_declarations(
                outMetadata,
                options != ZR_NULL ? options->manifestExportDeclarations : ZR_NULL,
                options != ZR_NULL ? options->manifestExportDeclarationCount : 0u)) {
        backend_aot_c_release_embedded_zrp_metadata(outMetadata);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

void backend_aot_c_release_embedded_zrp_metadata(SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL) {
        return;
    }

    if (metadata->ownedBlob != ZR_NULL) {
        free(metadata->ownedBlob);
    }
    backend_aot_c_zrp_type_def_token_remap_destroy(metadata);
    backend_aot_c_zrp_member_token_remap_destroy(metadata);
    backend_aot_c_zrp_manifest_export_table_destroy(metadata);

    metadata->blob = ZR_NULL;
    metadata->length = 0u;
    metadata->ownedBlob = ZR_NULL;
}
