#include "backend_aot_c_zrp_metadata_member_token.h"

#include "backend_aot_c_zrp_metadata_remap.h"
#include "backend_aot_c_zrp_metadata_type_def.h"

#include <stdlib.h>
#include <string.h>

static TZrBool backend_aot_c_zrp_member_token_is_member_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool backend_aot_c_zrp_type_token_is_type_def(TZrMetadataToken token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool backend_aot_c_zrp_manifest_export_kind_is_valid(EZrAotManifestExportDeclarationKind kind) {
    return (TZrBool)(kind == ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE ||
                     kind == ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD ||
                     kind == ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD);
}

static TZrBool backend_aot_c_zrp_manifest_export_tokens_match_kind(
        const SZrAotManifestExportDeclaration *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (declaration->kind) {
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE:
            return (TZrBool)!declaration->hasMemberTokenBinding;
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD:
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD:
            return (TZrBool)!declaration->hasTypeTokenBinding;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_zrp_manifest_export_table_contains_kind_target(
        const SZrAotManifestExportEntry *entries,
        TZrUInt32 entryCount,
        const SZrAotManifestExportDeclaration *declaration) {
    if (entries == ZR_NULL || declaration == ZR_NULL || declaration->target == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < entryCount; index++) {
        if (entries[index].kind == (TZrUInt32)declaration->kind &&
            entries[index].target != ZR_NULL &&
            strcmp(entries[index].target, declaration->target) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void backend_aot_c_zrp_member_token_remap_destroy(SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL) {
        return;
    }

    if (metadata->ownedMemberTokenRemapEntries != ZR_NULL) {
        free(metadata->ownedMemberTokenRemapEntries);
    }

    metadata->memberTokenRemapEntries = ZR_NULL;
    metadata->memberTokenRemapCount = 0u;
    metadata->ownedMemberTokenRemapEntries = ZR_NULL;
}

void backend_aot_c_zrp_manifest_export_table_destroy(SZrAotCEmbeddedZrpMetadata *metadata) {
    if (metadata == ZR_NULL) {
        return;
    }

    if (metadata->ownedManifestExportEntries != ZR_NULL) {
        free(metadata->ownedManifestExportEntries);
    }

    metadata->manifestExportEntries = ZR_NULL;
    metadata->manifestExportCount = 0u;
    metadata->ownedManifestExportEntries = ZR_NULL;
}

static TZrBool backend_aot_c_zrp_member_token_remap_append(
        SZrAotCZrpMemberTokenRemapEntry *entries,
        TZrUInt32 capacity,
        TZrUInt32 *writeIndex,
        TZrMetadataToken sourceToken,
        TZrMetadataToken targetToken) {
    if (entries == ZR_NULL ||
        writeIndex == ZR_NULL ||
        *writeIndex >= capacity ||
        !backend_aot_c_zrp_member_token_is_member_def(sourceToken) ||
        !backend_aot_c_zrp_member_token_is_member_def(targetToken)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < *writeIndex; index++) {
        if (entries[index].sourceToken == sourceToken) {
            return ZR_FALSE;
        }
    }

    entries[*writeIndex].sourceToken = sourceToken;
    entries[*writeIndex].targetToken = targetToken;
    (*writeIndex)++;
    return ZR_TRUE;
}

TZrBool backend_aot_c_zrp_member_token_remap_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        TZrUInt32 retainedFieldDefCount) {
    SZrAotCZrpMemberTokenRemapEntry *entries;
    TZrUInt32 entryCapacity;
    TZrUInt32 actualRetainedMethodDefCount;
    TZrUInt32 actualRetainedFieldDefCount;
    TZrUInt32 writeIndex = 0u;

    if (metadata == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_c_zrp_member_token_remap_destroy(metadata);

    if ((methodCount > 0u && methodRows == ZR_NULL) ||
        (fieldCount > 0u && fieldRows == ZR_NULL)) {
        return ZR_FALSE;
    }

    actualRetainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodRows, methodCount, functionTable);
    if (actualRetainedMethodDefCount != retainedMethodDefCount) {
        return ZR_FALSE;
    }
    actualRetainedFieldDefCount =
            backend_aot_c_zrp_count_retained_field_defs(fieldRows,
                                                        fieldCount,
                                                        typeRows,
                                                        typeCount,
                                                        tokenRecords,
                                                        tokenRecordCount,
                                                        methodRows,
                                                        methodCount,
                                                        genericParamRows,
                                                        genericParamCount,
                                                        genericParamConstraintRows,
                                                        genericParamConstraintCount,
                                                        functionTable,
                                                        retainedMethodDefCount);
    if (actualRetainedFieldDefCount != retainedFieldDefCount) {
        return ZR_FALSE;
    }

    entryCapacity = retainedMethodDefCount + retainedFieldDefCount;
    if (entryCapacity < retainedMethodDefCount) {
        return ZR_FALSE;
    }
    if (entryCapacity == 0u) {
        return ZR_TRUE;
    }

    entries = (SZrAotCZrpMemberTokenRemapEntry *)malloc(
            sizeof(SZrAotCZrpMemberTokenRemapEntry) * (size_t)entryCapacity);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 methodIndex = 0u; methodIndex < methodCount; methodIndex++) {
        if (!backend_aot_c_zrp_method_def_row_is_retained(&methodRows[methodIndex], functionTable)) {
            continue;
        }
        if (!backend_aot_c_zrp_member_token_remap_append(
                    entries,
                    entryCapacity,
                    &writeIndex,
                    methodRows[methodIndex].token,
                    backend_aot_c_zrp_compacted_method_def_token(methodRows,
                                                                 methodCount,
                                                                 methodIndex,
                                                                 functionTable))) {
            free(entries);
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 fieldIndex = 0u; fieldIndex < fieldCount; fieldIndex++) {
        if (!backend_aot_c_zrp_field_def_row_is_retained(&fieldRows[fieldIndex],
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
        if (!backend_aot_c_zrp_member_token_remap_append(
                    entries,
                    entryCapacity,
                    &writeIndex,
                    fieldRows[fieldIndex].token,
                    backend_aot_c_zrp_compacted_retained_field_def_token(retainedMethodDefCount,
                                                                         fieldRows,
                                                                         fieldCount,
                                                                         fieldIndex,
                                                                         typeRows,
                                                                         typeCount,
                                                                         tokenRecords,
                                                                         tokenRecordCount,
                                                                         methodRows,
                                                                         methodCount,
                                                                         genericParamRows,
                                                                         genericParamCount,
                                                                         genericParamConstraintRows,
                                                                         genericParamConstraintCount,
                                                                         functionTable))) {
            free(entries);
            return ZR_FALSE;
        }
    }

    metadata->memberTokenRemapEntries = entries;
    metadata->memberTokenRemapCount = writeIndex;
    metadata->ownedMemberTokenRemapEntries = entries;
    return ZR_TRUE;
}

TZrBool backend_aot_c_embedded_zrp_metadata_remap_member_token(const SZrAotCEmbeddedZrpMetadata *metadata,
                                                               TZrMetadataToken *token) {
    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_member_token_is_member_def(*token)) {
        return ZR_TRUE;
    }
    if (metadata == ZR_NULL ||
        metadata->memberTokenRemapEntries == ZR_NULL ||
        metadata->memberTokenRemapCount == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < metadata->memberTokenRemapCount; index++) {
        const SZrAotCZrpMemberTokenRemapEntry *entry = &metadata->memberTokenRemapEntries[index];
        if (entry->sourceToken == *token) {
            *token = entry->targetToken;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_embedded_zrp_metadata_remap_type_def_token(const SZrAotCEmbeddedZrpMetadata *metadata,
                                                                 TZrMetadataToken *token) {
    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_zrp_type_token_is_type_def(*token)) {
        return ZR_TRUE;
    }
    if (metadata == ZR_NULL || !metadata->hasTypeDefTokenRemap) {
        return ZR_TRUE;
    }
    if (metadata->typeDefTokenRemapEntries == ZR_NULL || metadata->typeDefTokenRemapCount == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < metadata->typeDefTokenRemapCount; index++) {
        const SZrAotCZrpTypeDefTokenRemapEntry *entry = &metadata->typeDefTokenRemapEntries[index];
        if (entry->sourceToken == *token) {
            *token = entry->targetToken;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_zrp_manifest_export_table_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrAotManifestExportDeclaration *declarations,
        TZrUInt32 declarationCount) {
    SZrAotManifestExportEntry *entries;

    if (metadata == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_c_zrp_manifest_export_table_destroy(metadata);

    if (declarationCount == 0u) {
        return ZR_TRUE;
    }
    if (declarations == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = (SZrAotManifestExportEntry *)malloc(
            sizeof(SZrAotManifestExportEntry) * (size_t)declarationCount);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < declarationCount; index++) {
        const SZrAotManifestExportDeclaration *declaration = &declarations[index];
        SZrAotManifestExportEntry *entry = &entries[index];

        if (declaration->target == ZR_NULL ||
            !backend_aot_c_zrp_manifest_export_kind_is_valid(declaration->kind) ||
            !backend_aot_c_zrp_manifest_export_tokens_match_kind(declaration)) {
            free(entries);
            return ZR_FALSE;
        }
        if (backend_aot_c_zrp_manifest_export_table_contains_kind_target(entries,
                                                                         index,
                                                                         declaration)) {
            free(entries);
            return ZR_FALSE;
        }

        entry->kind = (TZrUInt32)declaration->kind;
        entry->flags = 0u;
        entry->target = declaration->target;
        entry->typeToken = 0u;
        entry->memberToken = 0u;

        if (declaration->hasTypeTokenBinding) {
            TZrMetadataToken typeToken = declaration->typeToken;
            if (!backend_aot_c_zrp_type_token_is_type_def(typeToken)) {
                free(entries);
                return ZR_FALSE;
            }
            if (!backend_aot_c_embedded_zrp_metadata_remap_type_def_token(metadata, &typeToken)) {
                free(entries);
                return ZR_FALSE;
            }
            if (!backend_aot_c_zrp_type_token_is_type_def(typeToken)) {
                free(entries);
                return ZR_FALSE;
            }
            entry->flags |= ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN;
            entry->typeToken = typeToken;
        }
        if (declaration->hasMemberTokenBinding) {
            TZrMetadataToken memberToken = declaration->memberToken;
            if (!backend_aot_c_zrp_member_token_is_member_def(memberToken)) {
                free(entries);
                return ZR_FALSE;
            }
            if (!backend_aot_c_embedded_zrp_metadata_remap_member_token(metadata, &memberToken)) {
                free(entries);
                return ZR_FALSE;
            }
            if (!backend_aot_c_zrp_member_token_is_member_def(memberToken)) {
                free(entries);
                return ZR_FALSE;
            }
            entry->flags |= ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN;
            entry->memberToken = memberToken;
        }
    }

    metadata->manifestExportEntries = entries;
    metadata->manifestExportCount = declarationCount;
    metadata->ownedManifestExportEntries = entries;
    return ZR_TRUE;
}
