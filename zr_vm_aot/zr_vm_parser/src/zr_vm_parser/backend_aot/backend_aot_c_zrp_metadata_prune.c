#include "backend_aot_c_zrp_metadata_prune.h"

#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_sections.h"
#include "backend_aot_c_zrp_metadata_signature.h"
#include "backend_aot_c_zrp_metadata_string_pool.h"
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
                                                     TZrUInt32 retainedMethodDefCount,
                                                     TZrUInt32 retainedGenericParamCount,
                                                     TZrUInt32 retainedGenericParamConstraintCount,
                                                     TZrUInt32 retainedMethodSpecCount,
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
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_DEFS) {
            count = retainedMethodDefCount;
            elementSize = retainedMethodDefCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow) : 0u;
            byteLength = retainedMethodDefCount * (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
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
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_SPECS) {
            count = retainedMethodSpecCount;
            elementSize = retainedMethodSpecCount > 0u ? (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow) : 0u;
            byteLength = retainedMethodSpecCount * (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
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
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrZrpMetadataFieldDefRow *fieldRows,
                                                    TZrUInt32 fieldCount,
                                                    const SZrAotFunctionTable *functionTable,
                                                    TZrUInt32 retainedMethodDefCount,
                                                    const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    SZrMetadataTokenRecord *targetRows;
    TZrUInt32 writeIndex = 0u;

    if (targetHeader->tokenRecords.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrMetadataTokenRecord *)(void *)(targetBlob + targetHeader->tokenRecords.offset);
    for (TZrUInt32 readIndex = 0u; readIndex < tokenRecordCount; readIndex++) {
        SZrMetadataTokenRecord record = tokenRecords[readIndex];
        if (!backend_aot_c_zrp_remap_token_record(&record,
                                                  methodRows,
                                                  methodCount,
                                                  fieldRows,
                                                  fieldCount,
                                                  functionTable,
                                                  retainedMethodDefCount)) {
            continue;
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

static TZrBool backend_aot_c_zrp_copy_type_defs(TZrByte *targetBlob,
                                                const TZrByte *sourceBlob,
                                                const SZrZrpMetadataHeader *sourceHeader,
                                                const SZrZrpMetadataHeader *targetHeader,
                                                const SZrZrpMetadataMethodDefRow *methodRows,
                                                TZrUInt32 methodCount,
                                                const SZrZrpMetadataFieldDefRow *fieldRows,
                                                TZrUInt32 fieldCount,
                                                const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                TZrUInt32 genericParamCount,
                                                const SZrAotFunctionTable *functionTable,
                                                TZrUInt32 retainedMethodDefCount,
                                                const SZrAotCZrpSignatureBlobRemap *signatureRemap,
                                                const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataTypeDefRow *targetRows;

    if (targetHeader->typeDefs.byteLength == 0u) {
        return ZR_TRUE;
    }

    memcpy(targetBlob + targetHeader->typeDefs.offset,
           sourceBlob + sourceHeader->typeDefs.offset,
           targetHeader->typeDefs.byteLength);
    targetRows = (SZrZrpMetadataTypeDefRow *)(void *)(targetBlob + targetHeader->typeDefs.offset);
    for (TZrUInt32 index = 0u; index < targetHeader->typeDefs.count; index++) {
        backend_aot_c_zrp_adjust_type_def_method_range(&targetRows[index],
                                                       methodRows,
                                                       methodCount,
                                                       functionTable);
        backend_aot_c_zrp_adjust_generic_param_range(&targetRows[index].firstGenericParamIndex,
                                                     &targetRows[index].genericParamCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
                                                     functionTable,
                                                     retainedMethodDefCount);
        if (!backend_aot_c_zrp_remap_type_def_string_offsets(&targetRows[index], stringRemap) ||
            !backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[index].signatureBlobOffset,
                                                           targetRows[index].signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_method_defs(TZrByte *targetBlob,
                                                  const SZrZrpMetadataHeader *targetHeader,
                                                  const SZrZrpMetadataMethodDefRow *methodRows,
                                                  TZrUInt32 methodCount,
                                                  const SZrZrpMetadataFieldDefRow *fieldRows,
                                                  TZrUInt32 fieldCount,
                                                  const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                  TZrUInt32 genericParamCount,
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
        backend_aot_c_zrp_adjust_generic_param_range(&targetRows[writeIndex].firstGenericParamIndex,
                                                     &targetRows[writeIndex].genericParamCount,
                                                     genericParamRows,
                                                     genericParamCount,
                                                     methodRows,
                                                     methodCount,
                                                     fieldRows,
                                                     fieldCount,
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
                                                 TZrUInt32 retainedMethodDefCount,
                                                 const SZrAotCZrpSignatureBlobRemap *signatureRemap,
                                                 const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataFieldDefRow *targetRows;

    if (targetHeader->fieldDefs.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataFieldDefRow *)(void *)(targetBlob + targetHeader->fieldDefs.offset);
    for (TZrUInt32 index = 0u; index < fieldCount; index++) {
        targetRows[index] = fieldRows[index];
        targetRows[index].token = backend_aot_c_zrp_compacted_field_def_token(retainedMethodDefCount, index);
        if (!backend_aot_c_zrp_remap_field_def_string_offsets(&targetRows[index], stringRemap) ||
            !backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[index].signatureBlobOffset,
                                                           targetRows[index].signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_generic_params(TZrByte *targetBlob,
                                                     const SZrZrpMetadataHeader *targetHeader,
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
                                                               methodRows,
                                                               methodCount,
                                                               fieldRows,
                                                               fieldCount,
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
                                                                  methodRows,
                                                                  methodCount,
                                                                  fieldRows,
                                                                  fieldCount,
                                                                  functionTable,
                                                                  retainedMethodDefCount)) {
            continue;
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

static TZrBool backend_aot_c_zrp_copy_type_specs(TZrByte *targetBlob,
                                                 const SZrZrpMetadataHeader *targetHeader,
                                                 const SZrZrpMetadataTypeSpecRow *typeSpecRows,
                                                 TZrUInt32 typeSpecCount,
                                                 const SZrAotCZrpSignatureBlobRemap *signatureRemap) {
    SZrZrpMetadataTypeSpecRow *targetRows;

    if (targetHeader->typeSpecs.byteLength == 0u) {
        return ZR_TRUE;
    }

    targetRows = (SZrZrpMetadataTypeSpecRow *)(void *)(targetBlob + targetHeader->typeSpecs.offset);
    for (TZrUInt32 index = 0u; index < typeSpecCount; index++) {
        targetRows[index] = typeSpecRows[index];
        if (!backend_aot_c_zrp_remap_signature_blob_offset(&targetRows[index].signatureBlobOffset,
                                                           targetRows[index].signatureBlobLength,
                                                           signatureRemap)) {
            return ZR_FALSE;
        }
        targetRows[index].signatureHash = backend_aot_c_zrp_recomputed_signature_hash(targetBlob,
                                                                                      targetHeader,
                                                                                      targetRows[index].signatureBlobOffset,
                                                                                      targetRows[index].signatureBlobLength);
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_copy_method_specs(TZrByte *targetBlob,
                                                   const SZrZrpMetadataHeader *targetHeader,
                                                   const SZrZrpMetadataMethodSpecRow *methodSpecRows,
                                                   TZrUInt32 methodSpecCount,
                                                   const SZrZrpMetadataMethodDefRow *methodRows,
                                                   TZrUInt32 methodCount,
                                                   const SZrZrpMetadataFieldDefRow *fieldRows,
                                                   TZrUInt32 fieldCount,
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
                                                     functionTable,
                                                     retainedMethodDefCount)) {
            continue;
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
                                                  const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataModuleRefRow *targetRows;

    if (targetHeader->moduleRefs.byteLength == 0u) {
        return ZR_TRUE;
    }

    memcpy(targetBlob + targetHeader->moduleRefs.offset,
           sourceBlob + sourceHeader->moduleRefs.offset,
           targetHeader->moduleRefs.byteLength);
    targetRows = (SZrZrpMetadataModuleRefRow *)(void *)(targetBlob + targetHeader->moduleRefs.offset);
    for (TZrUInt32 index = 0u; index < targetHeader->moduleRefs.count; index++) {
        if (!backend_aot_c_zrp_remap_module_ref_string_offsets(&targetRows[index], stringRemap)) {
            return ZR_FALSE;
        }
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
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeRows;
    const SZrZrpMetadataMethodDefRow *methodRows;
    const SZrZrpMetadataFieldDefRow *fieldRows;
    const SZrZrpMetadataGenericParamRow *genericParamRows;
    const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraints;
    const SZrZrpMetadataTypeSpecRow *typeSpecs;
    const SZrZrpMetadataMethodSpecRow *methodSpecs;
    const SZrZrpMetadataModuleRefRow *moduleRefs;
    SZrAotCZrpSignatureBlobRemap signatureRemap;
    SZrAotCZrpStringPoolRemap stringRemap;
    TZrUInt32 signatureRemapCapacity;
    TZrUInt32 stringRemapCapacity;
    TZrUInt32 retainedTokenRecordCount;
    TZrUInt32 retainedMethodDefCount;
    TZrUInt32 retainedGenericParamCount;
    TZrUInt32 retainedGenericParamConstraintCount;
    TZrUInt32 retainedMethodSpecCount;
    TZrUInt32 retainedConstantPoolBytes = 0u;
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
                                           &signatureBlobView)) {
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
    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodRows, methodView.count, functionTable);
    retainedTokenRecordCount =
            backend_aot_c_zrp_count_retained_token_records(tokenRecords,
                                                           tokenRecordView.count,
                                                           methodRows,
                                                           methodView.count,
                                                           fieldRows,
                                                           fieldView.count,
                                                           functionTable,
                                                           retainedMethodDefCount);
    retainedGenericParamCount =
            backend_aot_c_zrp_count_retained_generic_params(genericParamRows,
                                                            genericParamView.count,
                                                            methodRows,
                                                            methodView.count,
                                                            fieldRows,
                                                            fieldView.count,
                                                            functionTable,
                                                            retainedMethodDefCount);
    retainedGenericParamConstraintCount =
            backend_aot_c_zrp_count_retained_generic_param_constraints(genericParamConstraints,
                                                                       genericParamConstraintView.count,
                                                                       genericParamRows,
                                                                       genericParamView.count,
                                                                       methodRows,
                                                                       methodView.count,
                                                                       fieldRows,
                                                                       fieldView.count,
                                                                       functionTable,
                                                                       retainedMethodDefCount);
    retainedMethodSpecCount =
            backend_aot_c_zrp_count_retained_method_specs(methodSpecs,
                                                          methodSpecView.count,
                                                          methodRows,
                                                          methodView.count,
                                                          fieldRows,
                                                          fieldView.count,
                                                          functionTable,
                                                          retainedMethodDefCount);

    signatureRemapCapacity = tokenRecordView.count +
                             typeView.count +
                             methodView.count +
                             fieldView.count +
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
                                                      methodSpecs,
                                                      methodSpecView.count,
                                                      functionTable,
                                                      retainedMethodDefCount)) {
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    stringRemapCapacity = (typeView.count * 2u) +
                          methodView.count +
                          fieldView.count +
                          genericParamView.count +
                          (moduleRefView.count * 2u);
    if (!backend_aot_c_zrp_string_pool_remap_init(&stringRemap,
                                                  stringRemapCapacity,
                                                  (TZrUInt32)stringPoolView.byteLength)) {
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_build_string_pool_remap(&stringRemap,
                                                   stringPoolView.data,
                                                   (TZrUInt32)stringPoolView.byteLength,
                                                   typeRows,
                                                   typeView.count,
                                                   methodRows,
                                                   methodView.count,
                                                   fieldRows,
                                                   fieldView.count,
                                                   genericParamRows,
                                                   genericParamView.count,
                                                   moduleRefs,
                                                   moduleRefView.count,
                                                   functionTable,
                                                   retainedMethodDefCount)) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    if (retainedTokenRecordCount == tokenRecordView.count &&
        retainedMethodDefCount == methodView.count &&
        retainedGenericParamCount == genericParamView.count &&
        retainedGenericParamConstraintCount == genericParamConstraintView.count &&
        retainedMethodSpecCount == methodSpecView.count &&
        retainedConstantPoolBytes == sourceHeader.constantPool.byteLength &&
        backend_aot_c_zrp_signature_blob_remap_is_identity(&signatureRemap) &&
        backend_aot_c_zrp_string_pool_remap_is_identity(&stringRemap)) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_TRUE;
    }

    if (!backend_aot_c_zrp_build_pruned_header(&sourceHeader,
                                               retainedTokenRecordCount,
                                               retainedMethodDefCount,
                                               retainedGenericParamCount,
                                               retainedGenericParamConstraintCount,
                                               retainedMethodSpecCount,
                                               stringRemap.byteLength,
                                               signatureRemap.byteLength,
                                               retainedConstantPoolBytes,
                                               &targetHeader,
                                               &prunedLength)) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    prunedBlob = (TZrByte *)malloc(prunedLength);
    if (prunedBlob == ZR_NULL) {
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }
    memset(prunedBlob, 0, prunedLength);
    if (!ZrCore_ZrpMetadata_WriteHeader(prunedBlob, prunedLength, &targetHeader)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
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
    if (!backend_aot_c_zrp_rewrite_retained_method_spec_signature_blobs(prunedBlob,
                                                                        &targetHeader,
                                                                        methodSpecs,
                                                                        methodSpecView.count,
                                                                        methodRows,
                                                                        methodView.count,
                                                                        fieldRows,
                                                                        fieldView.count,
                                                                        functionTable,
                                                                        retainedMethodDefCount,
                                                                        &signatureRemap)) {
        free(prunedBlob);
        backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
        backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
        return ZR_FALSE;
    }

    for (TZrUInt32 sectionKind = 0u; sectionKind < ZR_ZRP_METADATA_SECTION_COUNT; sectionKind++) {
        if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS) {
            if (!backend_aot_c_zrp_copy_token_records(prunedBlob,
                                                      &targetHeader,
                                                      tokenRecords,
                                                      tokenRecordView.count,
                                                      methodRows,
                                                      methodView.count,
                                                      fieldRows,
                                                      fieldView.count,
                                                      functionTable,
                                                      retainedMethodDefCount,
                                                      &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TYPE_DEFS) {
            if (!backend_aot_c_zrp_copy_type_defs(prunedBlob,
                                                  options->embeddedModuleBlob,
                                                  &sourceHeader,
                                                  &targetHeader,
                                                  methodRows,
                                                  methodView.count,
                                                  fieldRows,
                                                  fieldView.count,
                                                  genericParamRows,
                                                  genericParamView.count,
                                                  functionTable,
                                                  retainedMethodDefCount,
                                                  &signatureRemap,
                                                  &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_DEFS) {
            if (!backend_aot_c_zrp_copy_method_defs(prunedBlob,
                                                    &targetHeader,
                                                    methodRows,
                                                    methodView.count,
                                                    fieldRows,
                                                    fieldView.count,
                                                    genericParamRows,
                                                    genericParamView.count,
                                                    functionTable,
                                                    retainedMethodDefCount,
                                                    &signatureRemap,
                                                    &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_FIELD_DEFS) {
            if (!backend_aot_c_zrp_copy_field_defs(prunedBlob,
                                                   &targetHeader,
                                                   fieldRows,
                                                   fieldView.count,
                                                   retainedMethodDefCount,
                                                   &signatureRemap,
                                                   &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS) {
            if (!backend_aot_c_zrp_copy_generic_params(prunedBlob,
                                                       &targetHeader,
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
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS) {
            if (!backend_aot_c_zrp_copy_generic_param_constraints(prunedBlob,
                                                                  &targetHeader,
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
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_TYPE_SPECS) {
            if (!backend_aot_c_zrp_copy_type_specs(prunedBlob,
                                                   &targetHeader,
                                                   typeSpecs,
                                                   typeSpecView.count,
                                                   &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_METHOD_SPECS) {
            if (!backend_aot_c_zrp_copy_method_specs(prunedBlob,
                                                     &targetHeader,
                                                     methodSpecs,
                                                     methodSpecView.count,
                                                     methodRows,
                                                     methodView.count,
                                                     fieldRows,
                                                     fieldView.count,
                                                     functionTable,
                                                     retainedMethodDefCount,
                                                     &signatureRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_MODULE_REFS) {
            if (!backend_aot_c_zrp_copy_module_refs(prunedBlob,
                                                    options->embeddedModuleBlob,
                                                    &sourceHeader,
                                                    &targetHeader,
                                                    &stringRemap)) {
                free(prunedBlob);
                backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
                backend_aot_c_zrp_signature_blob_remap_destroy(&signatureRemap);
                return ZR_FALSE;
            }
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_STRING_POOL) {
            /* Already copied from compacted string remap before row rewrites. */
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL) {
            /* Already copied before row rewrites so signature hashes can be recomputed from final bytes. */
        } else {
            backend_aot_c_zrp_copy_section_if_needed(prunedBlob,
                                                     options->embeddedModuleBlob,
                                                     &sourceHeader,
                                                     &targetHeader,
                                                     (EZrZrpMetadataSectionKind)sectionKind);
        }
    }

    outMetadata->blob = prunedBlob;
    outMetadata->length = prunedLength;
    outMetadata->ownedBlob = prunedBlob;
    backend_aot_c_zrp_string_pool_remap_destroy(&stringRemap);
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

    if (!enableCodeStripping || outMetadata->blob == ZR_NULL || outMetadata->length == 0u) {
        return ZR_TRUE;
    }

    return backend_aot_c_prune_method_def_metadata_blob(options, functionTable, outMetadata);
}

void backend_aot_c_release_embedded_zrp_metadata(SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL) {
        return;
    }

    if (metadata->ownedBlob != ZR_NULL) {
        free(metadata->ownedBlob);
    }

    metadata->blob = ZR_NULL;
    metadata->length = 0u;
    metadata->ownedBlob = ZR_NULL;
}
