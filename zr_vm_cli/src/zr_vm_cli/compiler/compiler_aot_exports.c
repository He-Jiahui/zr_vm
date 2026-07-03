#include "compiler/compiler_aot_exports.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"

static TZrBool zr_cli_aot_export_read_u32_le(const TZrByte *bytes,
                                             TZrUInt32 length,
                                             TZrSize *cursor,
                                             TZrUInt32 *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (bytes == ZR_NULL || cursor == ZR_NULL || outValue == ZR_NULL ||
        *cursor > (TZrSize)length || (TZrSize)length - *cursor < sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt32)bytes[*cursor] |
                ((TZrUInt32)bytes[*cursor + 1u] << 8u) |
                ((TZrUInt32)bytes[*cursor + 2u] << 16u) |
                ((TZrUInt32)bytes[*cursor + 3u] << 24u);
    *cursor += sizeof(TZrUInt32);
    return ZR_TRUE;
}

void ZrCli_Compiler_AotExportDeclarations_Init(SZrCliAotPreserveRoots *roots) {
    if (roots == ZR_NULL) {
        return;
    }

    roots->exportDeclarations = ZR_NULL;
    roots->exportDeclarationCount = 0u;
    roots->exportDeclarationCapacity = 0u;
}

void ZrCli_Compiler_AotExportDeclarations_Free(SZrCliAotPreserveRoots *roots) {
    if (roots == ZR_NULL) {
        return;
    }

    free(roots->exportDeclarations);
    ZrCli_Compiler_AotExportDeclarations_Init(roots);
}

static TZrBool zr_cli_aot_export_member_token_is_valid(TZrMetadataToken token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool zr_cli_aot_export_type_token_is_valid(TZrMetadataToken token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool zr_cli_aot_export_target_matches(const TZrChar *symbolName,
                                                const TZrChar *target,
                                                const TZrChar *moduleName) {
    TZrSize moduleNameLength;

    if (symbolName == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(symbolName, target) == 0) {
        return ZR_TRUE;
    }
    if (moduleName == ZR_NULL || moduleName[0] == '\0') {
        return ZR_FALSE;
    }

    moduleNameLength = strlen(moduleName);
    return (TZrBool)(strncmp(target, moduleName, moduleNameLength) == 0 &&
                     target[moduleNameLength] == '.' &&
                      strcmp(symbolName, target + moduleNameLength + 1u) == 0);
}

static TZrBool zr_cli_aot_export_declaration_member_symbol_kind(
        EZrAotManifestExportDeclarationKind kind,
        TZrUInt8 *outSymbolKind) {
    if (outSymbolKind == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (kind) {
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD:
            *outSymbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
            return ZR_TRUE;
        case ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD:
            *outSymbolKind = ZR_FUNCTION_TYPED_SYMBOL_VARIABLE;
            return ZR_TRUE;
        default:
            *outSymbolKind = 0u;
            return ZR_FALSE;
    }
}

static TZrBool zr_cli_aot_export_type_def_record_matches_target(const SZrFunction *function,
                                                                const SZrMetadataTokenRecord *record,
                                                                const TZrChar *target,
                                                                const TZrChar *moduleName) {
    const TZrByte *blob;
    TZrSize cursor = 0u;
    TZrUInt32 encodedValueType;
    TZrUInt32 encodedStringIndex;

    if (function == ZR_NULL ||
        record == ZR_NULL ||
        target == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset >= function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    blob = function->signatureBlobHeap + record->signatureBlobOffset;
    if (blob[cursor++] != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF ||
        !zr_cli_aot_export_read_u32_le(blob, record->signatureBlobLength, &cursor, &encodedValueType) ||
        !zr_cli_aot_export_read_u32_le(blob, record->signatureBlobLength, &cursor, &encodedStringIndex) ||
        cursor != record->signatureBlobLength) {
        return ZR_FALSE;
    }
    (void)encodedValueType;

    for (TZrUInt32 index = 0u; index < function->metadataStringHeapLength; index++) {
        const SZrMetadataStringHeapEntry *entry = &function->metadataStringHeap[index];
        const TZrChar *actualText = entry->value != ZR_NULL
                                            ? ZrCore_String_GetNativeString(entry->value)
                                            : ZR_NULL;

        if (entry->stringIndex == encodedStringIndex &&
            zr_cli_aot_export_target_matches(actualText, target, moduleName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void zr_cli_aot_export_declaration_bind_type_token(const SZrFunction *function,
                                                          const TZrChar *target,
                                                          const TZrChar *moduleName,
                                                          SZrAotManifestExportDeclaration *destination) {
    if (function == ZR_NULL || target == ZR_NULL ||
        destination->kind != ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE ||
        function->metadataTokenRecords == ZR_NULL) {
        return;
    }

    for (TZrUInt32 recordIndex = 0u; recordIndex < function->metadataTokenRecordLength; recordIndex++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[recordIndex];

        if (!zr_cli_aot_export_type_token_is_valid(record->token) ||
            !zr_cli_aot_export_type_def_record_matches_target(function, record, target, moduleName)) {
            continue;
        }

        destination->hasTypeTokenBinding = ZR_TRUE;
        destination->typeToken = record->token;
        return;
    }
}

static void zr_cli_aot_export_declaration_bind_member_token(const SZrFunction *function,
                                                            const TZrChar *target,
                                                            const TZrChar *moduleName,
                                                            SZrAotManifestExportDeclaration *destination) {
    TZrUInt8 expectedSymbolKind = 0u;

    if (function == ZR_NULL || target == ZR_NULL ||
        !zr_cli_aot_export_declaration_member_symbol_kind(destination->kind, &expectedSymbolKind) ||
        function->typedExportedSymbols == ZR_NULL) {
        return;
    }

    for (TZrUInt32 symbolIndex = 0u; symbolIndex < function->typedExportedSymbolLength; symbolIndex++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[symbolIndex];
        const TZrChar *symbolName = symbol->name != ZR_NULL
                                            ? ZrCore_String_GetNativeString(symbol->name)
                                            : ZR_NULL;

        if (symbol->symbolKind != expectedSymbolKind ||
            !zr_cli_aot_export_target_matches(symbolName, target, moduleName) ||
            !zr_cli_aot_export_member_token_is_valid(symbol->metadataToken)) {
            continue;
        }

        destination->hasMemberTokenBinding = ZR_TRUE;
        destination->memberToken = symbol->metadataToken;
        return;
    }
}

static void zr_cli_aot_export_declaration_bind_metadata_tokens(const SZrFunction *function,
                                                               const TZrChar *target,
                                                               const TZrChar *moduleName,
                                                               SZrAotManifestExportDeclaration *destination) {
    destination->hasTypeTokenBinding = ZR_FALSE;
    destination->typeToken = 0u;
    destination->hasMemberTokenBinding = ZR_FALSE;
    destination->memberToken = 0u;

    zr_cli_aot_export_declaration_bind_type_token(function, target, moduleName, destination);
    zr_cli_aot_export_declaration_bind_member_token(function, target, moduleName, destination);
}

static TZrBool zr_cli_aot_export_declaration_kind_from_project(
        EZrLibrary_ProjectExportDeclarationKind kind,
        EZrAotManifestExportDeclarationKind *outKind) {
    if (outKind != ZR_NULL) {
        *outKind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE;
    }
    if (outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (kind) {
        case ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_TYPE:
            *outKind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE;
            return ZR_TRUE;
        case ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_METHOD:
            *outKind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
            return ZR_TRUE;
        case ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_FIELD:
            *outKind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_cli_aot_export_declaration_exists(const SZrAotManifestExportDeclaration *declarations,
                                                    TZrSize declarationCount,
                                                    EZrAotManifestExportDeclarationKind kind,
                                                    const TZrChar *target) {
    if (declarations == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < declarationCount; index++) {
        if (declarations[index].kind == kind &&
            declarations[index].target != ZR_NULL &&
            strcmp(declarations[index].target, target) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrCli_Compiler_ApplyProjectAotExportDeclarations(const SZrCliProjectContext *project,
                                                         const SZrFunction *function,
                                                         SZrAotWriterOptions *options,
                                                         SZrCliAotPreserveRoots *roots) {
    const SZrLibrary_Project *libraryProject;
    TZrSize declarationCount;

    if (project == ZR_NULL || project->libraryProject == ZR_NULL || options == ZR_NULL || roots == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCli_Compiler_AotExportDeclarations_Free(roots);
    options->manifestExportDeclarations = ZR_NULL;
    options->manifestExportDeclarationCount = 0u;

    libraryProject = project->libraryProject;
    declarationCount = libraryProject->exportDeclarationCount;
    if (declarationCount == 0u) {
        return ZR_TRUE;
    }
    if (declarationCount > (TZrSize)UINT32_MAX) {
        return ZR_FALSE;
    }

    roots->exportDeclarations = (SZrAotManifestExportDeclaration *)malloc(
            sizeof(*roots->exportDeclarations) * declarationCount);
    if (roots->exportDeclarations == ZR_NULL) {
        return ZR_FALSE;
    }
    roots->exportDeclarationCapacity = (TZrUInt32)declarationCount;

    for (TZrSize declarationIndex = 0u; declarationIndex < declarationCount; declarationIndex++) {
        const SZrLibrary_ProjectExportDeclaration *source =
                &libraryProject->exportDeclarations[declarationIndex];
        SZrAotManifestExportDeclaration *destination =
                &roots->exportDeclarations[declarationIndex];
        const TZrChar *target = source->target != ZR_NULL
                                        ? ZrCore_String_GetNativeString(source->target)
                                        : ZR_NULL;

        if (target == ZR_NULL || target[0] == '\0' ||
            !zr_cli_aot_export_declaration_kind_from_project(source->kind, &destination->kind) ||
            zr_cli_aot_export_declaration_exists(roots->exportDeclarations,
                                                 declarationIndex,
                                                 destination->kind,
                                                 target)) {
            ZrCli_Compiler_AotExportDeclarations_Free(roots);
            return ZR_FALSE;
        }

        destination->target = target;
        zr_cli_aot_export_declaration_bind_metadata_tokens(function, target, options->moduleName, destination);
    }

    roots->exportDeclarationCount = (TZrUInt32)declarationCount;
    options->manifestExportDeclarations = roots->exportDeclarations;
    options->manifestExportDeclarationCount = roots->exportDeclarationCount;
    return ZR_TRUE;
}
