#include "backend_aot_c_zrp_metadata_manifest_export.h"

#include "backend_aot_c_zrp_metadata_member_token.h"
#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_sections.h"
#include "backend_aot_c_zrp_metadata_type_def.h"

#include "zr_vm_common/zr_aot_abi.h"

#include <stdlib.h>
#include <string.h>

static TZrBool backend_aot_c_zrp_manifest_export_member_token_is_member_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool backend_aot_c_zrp_manifest_export_type_token_is_type_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool backend_aot_c_zrp_manifest_export_declaration_is_publishable(
        const SZrAotManifestExportDeclaration *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (declaration->kind == ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE) {
        return (TZrBool)!declaration->hasMemberTokenBinding;
    }
    if (declaration->kind == ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD ||
        declaration->kind == ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD) {
        return (TZrBool)!declaration->hasTypeTokenBinding;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_zrp_manifest_export_string_find(const TZrByte *stringPool,
                                                             TZrUInt32 stringPoolBytes,
                                                             const TZrChar *target,
                                                             TZrBool *outFound,
                                                             TZrUInt32 *outOffset) {
    size_t targetBytes;
    TZrUInt32 offset = 0u;

    if (outFound == ZR_NULL || outOffset == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFound = ZR_FALSE;
    *outOffset = 0u;
    targetBytes = strlen(target) + 1u;
    if (targetBytes > (size_t)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    while (offset < stringPoolBytes) {
        TZrUInt32 end = offset;
        TZrUInt32 candidateBytes;

        while (end < stringPoolBytes && stringPool[end] != 0u) {
            end++;
        }
        if (end >= stringPoolBytes) {
            return ZR_FALSE;
        }

        candidateBytes = end - offset + 1u;
        if (candidateBytes == (TZrUInt32)targetBytes &&
            memcmp(stringPool + offset, target, targetBytes) == 0) {
            *outFound = ZR_TRUE;
            *outOffset = offset;
            return ZR_TRUE;
        }

        offset = end + 1u;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_manifest_export_append_string(TZrByte *extraStringPool,
                                                               TZrUInt32 extraStringPoolCapacity,
                                                               TZrUInt32 *extraStringPoolBytes,
                                                               const TZrChar *target,
                                                               TZrUInt32 baseOffset,
                                                               TZrUInt32 *outOffset) {
    size_t targetBytes;

    if (extraStringPool == ZR_NULL ||
        extraStringPoolBytes == ZR_NULL ||
        outOffset == ZR_NULL ||
        target == ZR_NULL) {
        return ZR_FALSE;
    }

    targetBytes = strlen(target) + 1u;
    if (targetBytes > (size_t)0xFFFFFFFFu ||
        *extraStringPoolBytes > extraStringPoolCapacity ||
        (TZrUInt32)targetBytes > extraStringPoolCapacity - *extraStringPoolBytes ||
        baseOffset > 0xFFFFFFFFu - *extraStringPoolBytes) {
        return ZR_FALSE;
    }

    *outOffset = baseOffset + *extraStringPoolBytes;
    memcpy(extraStringPool + *extraStringPoolBytes, target, targetBytes);
    *extraStringPoolBytes += (TZrUInt32)targetBytes;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_manifest_export_get_target_string_offset(
        const SZrZrpMetadataSectionView *stringPoolView,
        TZrByte *extraStringPool,
        TZrUInt32 extraStringPoolCapacity,
        TZrUInt32 *extraStringPoolBytes,
        const TZrChar *target,
        TZrUInt32 *outOffset) {
    TZrBool found;

    if (stringPoolView == ZR_NULL ||
        extraStringPoolBytes == ZR_NULL ||
        outOffset == ZR_NULL ||
        target == ZR_NULL ||
        stringPoolView->byteLength > (TZrSize)0xFFFFFFFFu) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_zrp_manifest_export_string_find(stringPoolView->data,
                                                       (TZrUInt32)stringPoolView->byteLength,
                                                       target,
                                                       &found,
                                                       outOffset)) {
        return ZR_FALSE;
    }
    if (found) {
        return ZR_TRUE;
    }

    if (!backend_aot_c_zrp_manifest_export_string_find(extraStringPool,
                                                       *extraStringPoolBytes,
                                                       target,
                                                       &found,
                                                       outOffset)) {
        return ZR_FALSE;
    }
    if (found) {
        *outOffset += (TZrUInt32)stringPoolView->byteLength;
        return ZR_TRUE;
    }

    return backend_aot_c_zrp_manifest_export_append_string(
            extraStringPool,
            extraStringPoolCapacity,
            extraStringPoolBytes,
            target,
            (TZrUInt32)stringPoolView->byteLength,
            outOffset);
}

static TZrBool backend_aot_c_zrp_manifest_export_build_published_row(
        const SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrAotManifestExportDeclaration *declaration,
        const SZrZrpMetadataSectionView *stringPoolView,
        TZrByte *extraStringPool,
        TZrUInt32 extraStringPoolCapacity,
        TZrUInt32 *extraStringPoolBytes,
        SZrZrpMetadataManifestExportRow *outRow) {
    TZrMetadataToken typeToken;
    TZrMetadataToken memberToken;

    if (metadata == ZR_NULL ||
        declaration == ZR_NULL ||
        outRow == ZR_NULL ||
        declaration->target == ZR_NULL ||
        !backend_aot_c_zrp_manifest_export_declaration_is_publishable(declaration)) {
        return ZR_FALSE;
    }

    memset(outRow, 0, sizeof(*outRow));
    outRow->kind = (TZrUInt32)declaration->kind;
    if (declaration->hasTypeTokenBinding) {
        typeToken = declaration->typeToken;
        if (!backend_aot_c_zrp_manifest_export_type_token_is_type_def(typeToken) ||
            !backend_aot_c_embedded_zrp_metadata_remap_type_def_token(metadata, &typeToken) ||
            !backend_aot_c_zrp_manifest_export_type_token_is_type_def(typeToken)) {
            return ZR_FALSE;
        }
        outRow->flags = ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN;
        outRow->typeToken = typeToken;
    }
    if (declaration->hasMemberTokenBinding) {
        memberToken = declaration->memberToken;
        if (!backend_aot_c_zrp_manifest_export_member_token_is_member_def(memberToken) ||
            !backend_aot_c_embedded_zrp_metadata_remap_member_token(metadata, &memberToken) ||
            !backend_aot_c_zrp_manifest_export_member_token_is_member_def(memberToken)) {
            return ZR_FALSE;
        }
        outRow->flags |= ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN;
        outRow->memberToken = memberToken;
    }
    return backend_aot_c_zrp_manifest_export_get_target_string_offset(stringPoolView,
                                                                      extraStringPool,
                                                                      extraStringPoolCapacity,
                                                                      extraStringPoolBytes,
                                                                      declaration->target,
                                                                      &outRow->targetStringOffset);
}

static TZrBool backend_aot_c_zrp_manifest_export_build_published_header(
        const SZrZrpMetadataHeader *sourceHeader,
        TZrUInt32 publishedStringPoolBytes,
        TZrUInt32 publishedManifestExportCount,
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
        if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_STRING_POOL) {
            byteLength = publishedStringPoolBytes;
            count = publishedStringPoolBytes;
            elementSize = publishedStringPoolBytes > 0u ? 1u : 0u;
        } else if (sectionKind == (TZrUInt32)ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS) {
            if (publishedManifestExportCount >
                0xFFFFFFFFu / (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow)) {
                return ZR_FALSE;
            }
            count = publishedManifestExportCount;
            elementSize = publishedManifestExportCount > 0u
                          ? (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow)
                          : 0u;
            byteLength = publishedManifestExportCount * (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow);
        }

        backend_aot_c_zrp_set_section_layout(targetSection,
                                             &offset,
                                             byteLength,
                                             count,
                                             elementSize);
    }

    *outLength = offset;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_zrp_manifest_export_copy_published_sections(
        TZrByte *targetBlob,
        TZrSize targetLength,
        const SZrZrpMetadataHeader *targetHeader,
        const TZrByte *sourceBlob,
        TZrSize sourceLength,
        const SZrZrpMetadataHeader *sourceHeader,
        const TZrByte *extraStringPool,
        TZrUInt32 extraStringPoolBytes,
        const SZrZrpMetadataManifestExportRow *manifestExportRows,
        TZrUInt32 manifestExportCount) {
    const SZrZrpMetadataSection *sourceStringPool;
    const SZrZrpMetadataSection *targetStringPool;

    if (targetBlob == ZR_NULL ||
        targetHeader == ZR_NULL ||
        sourceBlob == ZR_NULL ||
        sourceHeader == ZR_NULL ||
        (extraStringPoolBytes != 0u && extraStringPool == ZR_NULL) ||
        (manifestExportCount != 0u && manifestExportRows == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 sectionKind = 0u; sectionKind < ZR_ZRP_METADATA_SECTION_COUNT; sectionKind++) {
        if (sectionKind != (TZrUInt32)ZR_ZRP_METADATA_SECTION_STRING_POOL &&
            sectionKind != (TZrUInt32)ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS) {
            backend_aot_c_zrp_copy_section_if_needed(targetBlob,
                                                     sourceBlob,
                                                     sourceHeader,
                                                     targetHeader,
                                                     (EZrZrpMetadataSectionKind)sectionKind);
        }
    }

    sourceStringPool = backend_aot_c_zrp_metadata_section(sourceHeader, ZR_ZRP_METADATA_SECTION_STRING_POOL);
    targetStringPool = backend_aot_c_zrp_metadata_section(targetHeader, ZR_ZRP_METADATA_SECTION_STRING_POOL);
    if (sourceStringPool == ZR_NULL ||
        targetStringPool == ZR_NULL ||
        sourceStringPool->offset > sourceLength ||
        sourceStringPool->byteLength > (TZrUInt32)(sourceLength - sourceStringPool->offset) ||
        targetStringPool->offset > targetLength ||
        targetStringPool->byteLength > (TZrUInt32)(targetLength - targetStringPool->offset) ||
        extraStringPoolBytes > 0xFFFFFFFFu - sourceStringPool->byteLength ||
        sourceStringPool->byteLength + extraStringPoolBytes != targetStringPool->byteLength) {
        return ZR_FALSE;
    }

    if (sourceStringPool->byteLength > 0u) {
        memcpy(targetBlob + targetStringPool->offset,
               sourceBlob + sourceStringPool->offset,
               sourceStringPool->byteLength);
    }
    if (extraStringPoolBytes > 0u) {
        memcpy(targetBlob + targetStringPool->offset + sourceStringPool->byteLength,
               extraStringPool,
               extraStringPoolBytes);
    }

    return ZrCore_ZrpMetadata_WriteDefinitionTablePayload(targetBlob,
                                                          targetLength,
                                                          targetHeader,
                                                          ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                          manifestExportRows,
                                                          manifestExportCount,
                                                          (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow));
}

static TZrBool backend_aot_c_zrp_manifest_export_remap_tokens(
        SZrZrpMetadataManifestExportRow *row,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
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
        TZrUInt32 retainedMethodDefCount) {
    const TZrBool hasTypeToken =
            (TZrBool)((row->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN) != 0u);
    const TZrBool hasMemberToken =
            (TZrBool)((row->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN) != 0u);

    if (!hasTypeToken && !hasMemberToken) {
        return (TZrBool)((row->kind == ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE ||
                          row->kind == ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD ||
                          row->kind == ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD) &&
                         row->typeToken == 0u &&
                         row->memberToken == 0u);
    }

    if (row->kind == ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE) {
        return (TZrBool)(hasTypeToken &&
                         !hasMemberToken &&
                         row->memberToken == 0u &&
                         backend_aot_c_zrp_remap_type_def_token(&row->typeToken,
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
                                                                 retainedMethodDefCount));
    }

    if (row->kind == ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD ||
        row->kind == ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD) {
        return (TZrBool)(!hasTypeToken &&
                         hasMemberToken &&
                         row->typeToken == 0u &&
                         backend_aot_c_zrp_remap_retained_export_member_token(&row->memberToken,
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
                                                                               retainedMethodDefCount));
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_manifest_export_row_is_retained(
        const SZrZrpMetadataManifestExportRow *row,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
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
        TZrUInt32 retainedMethodDefCount) {
    SZrZrpMetadataManifestExportRow candidate;

    if (row == ZR_NULL) {
        return ZR_FALSE;
    }

    candidate = *row;
    return backend_aot_c_zrp_manifest_export_remap_tokens(&candidate,
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
                                                          retainedMethodDefCount);
}

TZrBool backend_aot_c_zrp_copy_manifest_exports(
        TZrByte *targetBlob,
        const SZrZrpMetadataHeader *targetHeader,
        const SZrZrpMetadataManifestExportRow *rows,
        TZrUInt32 count,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
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
        const SZrAotCZrpStringPoolRemap *stringRemap) {
    SZrZrpMetadataManifestExportRow *targetRows;

    if (targetHeader == ZR_NULL) {
        return ZR_FALSE;
    }
    if (targetHeader->manifestExports.byteLength == 0u) {
        return ZR_TRUE;
    }
    if (targetBlob == ZR_NULL ||
        rows == ZR_NULL ||
        targetHeader->manifestExports.count != count ||
        targetHeader->manifestExports.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow) ||
        targetHeader->manifestExports.byteLength != count * (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow)) {
        return ZR_FALSE;
    }

    targetRows = (SZrZrpMetadataManifestExportRow *)(void *)(targetBlob + targetHeader->manifestExports.offset);
    for (TZrUInt32 index = 0u; index < count; index++) {
        targetRows[index] = rows[index];
        if (!backend_aot_c_zrp_remap_string_offset(&targetRows[index].targetStringOffset, stringRemap) ||
            !backend_aot_c_zrp_manifest_export_remap_tokens(&targetRows[index],
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
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_publish_manifest_export_declarations(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrAotManifestExportDeclaration *declarations,
        TZrUInt32 declarationCount) {
    SZrZrpMetadataHeader sourceHeader;
    SZrZrpMetadataHeader targetHeader;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView manifestExportView;
    SZrZrpMetadataManifestExportRow *manifestExportRows = ZR_NULL;
    TZrByte *extraStringPool = ZR_NULL;
    TZrByte *targetBlob = ZR_NULL;
    TZrByte *oldOwnedBlob;
    TZrUInt32 publishableDeclarationCount = 0u;
    TZrUInt32 maxExtraStringPoolBytes = 0u;
    TZrUInt32 extraStringPoolBytes = 0u;
    TZrUInt32 manifestExportWriteIndex;
    TZrUInt32 targetStringPoolBytes;
    TZrUInt32 targetManifestExportCount;
    TZrSize targetLength = 0u;
    TZrBool success = ZR_FALSE;

    if (metadata == ZR_NULL) {
        return ZR_FALSE;
    }
    if (declarationCount == 0u || declarations == ZR_NULL || metadata->blob == ZR_NULL || metadata->length == 0u) {
        return ZR_TRUE;
    }
    if (!ZrCore_ZrpMetadata_ReadHeader(metadata->blob, metadata->length, &sourceHeader) ||
        !ZrCore_ZrpMetadata_GetSectionView(metadata->blob,
                                           metadata->length,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                           &stringPoolView) ||
        !ZrCore_ZrpMetadata_GetSectionView(metadata->blob,
                                           metadata->length,
                                           &sourceHeader,
                                           ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                           &manifestExportView) ||
        stringPoolView.byteLength > (TZrSize)0xFFFFFFFFu ||
        manifestExportView.count > 0xFFFFFFFFu - declarationCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < declarationCount; index++) {
        const SZrAotManifestExportDeclaration *declaration = &declarations[index];

        if (backend_aot_c_zrp_manifest_export_declaration_is_publishable(declaration)) {
            size_t targetBytes;

            if (declaration->target == ZR_NULL) {
                return ZR_FALSE;
            }
            targetBytes = strlen(declaration->target) + 1u;
            if (targetBytes > (size_t)0xFFFFFFFFu ||
                (TZrUInt32)targetBytes > 0xFFFFFFFFu - maxExtraStringPoolBytes) {
                return ZR_FALSE;
            }
            publishableDeclarationCount++;
            maxExtraStringPoolBytes += (TZrUInt32)targetBytes;
        }
    }

    if (publishableDeclarationCount == 0u) {
        return ZR_TRUE;
    }
    if (manifestExportView.count > 0xFFFFFFFFu - publishableDeclarationCount) {
        return ZR_FALSE;
    }

    targetManifestExportCount = manifestExportView.count + publishableDeclarationCount;
    if (targetManifestExportCount >
        0xFFFFFFFFu / (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow)) {
        return ZR_FALSE;
    }
    manifestExportRows = (SZrZrpMetadataManifestExportRow *)malloc(
            (size_t)targetManifestExportCount * sizeof(SZrZrpMetadataManifestExportRow));
    if (manifestExportRows == ZR_NULL) {
        return ZR_FALSE;
    }
    if (maxExtraStringPoolBytes > 0u) {
        extraStringPool = (TZrByte *)malloc((size_t)maxExtraStringPoolBytes);
        if (extraStringPool == ZR_NULL) {
            goto cleanup;
        }
    }

    if (manifestExportView.count > 0u) {
        memcpy(manifestExportRows,
               manifestExportView.data,
               (size_t)manifestExportView.count * sizeof(SZrZrpMetadataManifestExportRow));
    }
    manifestExportWriteIndex = manifestExportView.count;

    for (TZrUInt32 index = 0u; index < declarationCount; index++) {
        const SZrAotManifestExportDeclaration *declaration = &declarations[index];

        if (!backend_aot_c_zrp_manifest_export_declaration_is_publishable(declaration)) {
            continue;
        }

        if (!backend_aot_c_zrp_manifest_export_build_published_row(
                    metadata,
                    declaration,
                    &stringPoolView,
                    extraStringPool,
                    maxExtraStringPoolBytes,
                    &extraStringPoolBytes,
                    &manifestExportRows[manifestExportWriteIndex])) {
            goto cleanup;
        }
        manifestExportWriteIndex++;
    }

    if (manifestExportWriteIndex != targetManifestExportCount ||
        extraStringPoolBytes > 0xFFFFFFFFu - (TZrUInt32)stringPoolView.byteLength) {
        goto cleanup;
    }

    targetStringPoolBytes = (TZrUInt32)stringPoolView.byteLength + extraStringPoolBytes;
    if (!backend_aot_c_zrp_manifest_export_build_published_header(&sourceHeader,
                                                                  targetStringPoolBytes,
                                                                  targetManifestExportCount,
                                                                  &targetHeader,
                                                                  &targetLength)) {
        goto cleanup;
    }

    targetBlob = (TZrByte *)malloc((size_t)targetLength);
    if (targetBlob == ZR_NULL) {
        goto cleanup;
    }
    memset(targetBlob, 0, (size_t)targetLength);
    if (!ZrCore_ZrpMetadata_WriteHeader(targetBlob, targetLength, &targetHeader) ||
        !backend_aot_c_zrp_manifest_export_copy_published_sections(targetBlob,
                                                                   targetLength,
                                                                   &targetHeader,
                                                                   metadata->blob,
                                                                   metadata->length,
                                                                   &sourceHeader,
                                                                   extraStringPool,
                                                                   extraStringPoolBytes,
                                                                   manifestExportRows,
                                                                   targetManifestExportCount) ||
        !ZrCore_ZrpMetadata_ValidateDefinitionTables(targetBlob, targetLength, &targetHeader)) {
        goto cleanup;
    }

    oldOwnedBlob = metadata->ownedBlob;
    metadata->blob = targetBlob;
    metadata->length = targetLength;
    metadata->ownedBlob = targetBlob;
    targetBlob = ZR_NULL;
    if (oldOwnedBlob != ZR_NULL) {
        free(oldOwnedBlob);
    }
    success = ZR_TRUE;

cleanup:
    if (targetBlob != ZR_NULL) {
        free(targetBlob);
    }
    if (extraStringPool != ZR_NULL) {
        free(extraStringPool);
    }
    if (manifestExportRows != ZR_NULL) {
        free(manifestExportRows);
    }
    return success;
}
