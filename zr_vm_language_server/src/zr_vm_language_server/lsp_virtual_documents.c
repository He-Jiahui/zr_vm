#include "lsp_virtual_documents.h"

#include "metadata/lsp_metadata_provider.h"
#include "metadata/lsp_native_declaration_projection.h"
#include "metadata/lsp_virtual_document_identity.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_library/native_registry.h"

#define ZR_LSP_VIRTUAL_URI_PREFIX "zr-decompiled:/"
#define ZR_LSP_VIRTUAL_RECORD_INITIAL_CAPACITY 32U

static const TZrChar *virtual_documents_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool virtual_documents_range_contains_file_position(SZrFileRange range, SZrFilePosition position) {
    TZrInt32 line = position.line;
    TZrInt32 column = position.column;

    return (range.start.line < line || (range.start.line == line && range.start.column <= column)) &&
           (line < range.end.line || (line == range.end.line && column <= range.end.column));
}

static TZrBool virtual_documents_range_contains_position(SZrFileRange range, SZrLspPosition position) {
    TZrInt32 line = position.line + 1;
    TZrInt32 column = position.character + 1;

    return (range.start.line < line || (range.start.line == line && range.start.column <= column)) &&
           (line < range.end.line || (line == range.end.line && column <= range.end.column));
}

static TZrBool virtual_documents_range_contains_lsp_position(SZrFileRange range,
                                                             SZrLspPosition position,
                                                             const TZrChar *content,
                                                             TZrSize contentLength) {
    SZrFilePosition filePosition;

    if (content == ZR_NULL) {
        return virtual_documents_range_contains_position(range, position);
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength);
    return virtual_documents_range_contains_file_position(range, filePosition);
}

static TZrBool virtual_documents_uri_is_virtual_declaration(SZrString *uri) {
    const TZrChar *text = virtual_documents_string_text(uri);
    TZrSize prefixLength = strlen(ZR_LSP_VIRTUAL_URI_PREFIX);

    return text != ZR_NULL && strncmp(text, ZR_LSP_VIRTUAL_URI_PREFIX, prefixLength) == 0;
}

static TZrBool virtual_documents_record_compact_type_members(SZrState *state,
                                                             const ZrLibModuleDescriptor *descriptor,
                                                             SZrString *uri,
                                                             SZrArray *outRecords) {
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL || outRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, outRecords, sizeof(SZrLspVirtualRecord), ZR_LSP_VIRTUAL_RECORD_INITIAL_CAPACITY);
    for (TZrSize typeIndex = 0; typeIndex < descriptor->typeCount; typeIndex++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[typeIndex];
        TZrInt32 fileLine = (TZrInt32)(typeIndex + 2);

        if (typeDescriptor == ZR_NULL || typeDescriptor->name == ZR_NULL) {
            continue;
        }

        for (TZrSize fieldIndex = 0; fieldIndex < typeDescriptor->fieldCount; fieldIndex++) {
            const ZrLibFieldDescriptor *fieldDescriptor = &typeDescriptor->fields[fieldIndex];
            SZrLspVirtualRecord record;
            TZrSize nameLength;
            TZrInt32 startColumn;
            TZrInt32 endColumn;

            if (fieldDescriptor == ZR_NULL || fieldDescriptor->name == ZR_NULL) {
                continue;
            }

            memset(&record, 0, sizeof(record));
            nameLength = strlen(fieldDescriptor->name);
            startColumn = (TZrInt32)(fieldIndex * 8 + 1);
            endColumn = startColumn + (TZrInt32)(nameLength > 0 ? nameLength : 1);
            record.kind = ZR_LSP_VIRTUAL_DECLARATION_FIELD;
            record.declarationIdentity = fieldDescriptor;
            record.ownerTypeDescriptor = typeDescriptor;
            record.ownerName = typeDescriptor->name;
            record.name = fieldDescriptor->name;
            record.range = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, fileLine, startColumn),
                                                     ZrParser_FilePosition_Create(0, fileLine, endColumn),
                                                     uri);
            ZrCore_Array_Push(state, outRecords, &record);
        }

        for (TZrSize methodIndex = 0; methodIndex < typeDescriptor->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[methodIndex];
            SZrLspVirtualRecord record;
            TZrSize nameLength;
            TZrInt32 startColumn;
            TZrInt32 endColumn;

            if (methodDescriptor == ZR_NULL || methodDescriptor->name == ZR_NULL) {
                continue;
            }

            memset(&record, 0, sizeof(record));
            nameLength = strlen(methodDescriptor->name);
            startColumn = (TZrInt32)(methodIndex * 8 + 129);
            endColumn = startColumn + (TZrInt32)(nameLength > 0 ? nameLength : 1);
            record.kind = ZR_LSP_VIRTUAL_DECLARATION_METHOD;
            record.declarationIdentity = methodDescriptor;
            record.ownerTypeDescriptor = typeDescriptor;
            record.ownerName = typeDescriptor->name;
            record.name = methodDescriptor->name;
            record.range = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, fileLine, startColumn),
                                                     ZrParser_FilePosition_Create(0, fileLine, endColumn),
                                                     uri);
            ZrCore_Array_Push(state, outRecords, &record);
        }
    }

    return ZR_TRUE;
}

static TZrBool virtual_documents_collect_records(SZrState *state,
                                                 const ZrLibModuleDescriptor *descriptor,
                                                 SZrString *uri,
                                                 SZrArray *outRecords) {
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL || outRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!virtual_documents_uri_is_virtual_declaration(uri)) {
        return virtual_documents_record_compact_type_members(state, descriptor, uri, outRecords);
    }

    ZrCore_Array_Construct(outRecords);
    return ZrLanguageServer_LspNativeDeclarationProjection_Build(state, descriptor, uri, ZR_NULL, outRecords);
}

TZrBool ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(SZrString *uri) {
    return virtual_documents_uri_is_virtual_declaration(uri);
}

SZrString *ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(SZrState *state, const TZrChar *moduleName) {
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(buffer, sizeof(buffer), "%s%s.zr", ZR_LSP_VIRTUAL_URI_PREFIX, moduleName);
    return ZrCore_String_Create(state, buffer, strlen(buffer));
}

TZrBool ZrLanguageServer_LspVirtualDocuments_ParseDeclarationUri(SZrString *uri,
                                                                 TZrChar *moduleNameBuffer,
                                                                 TZrSize bufferSize) {
    const TZrChar *text = virtual_documents_string_text(uri);
    TZrSize prefixLength = strlen(ZR_LSP_VIRTUAL_URI_PREFIX);
    TZrSize length;

    if (moduleNameBuffer != ZR_NULL && bufferSize > 0) {
        moduleNameBuffer[0] = '\0';
    }
    if (text == ZR_NULL || moduleNameBuffer == ZR_NULL || bufferSize == 0 ||
        strncmp(text, ZR_LSP_VIRTUAL_URI_PREFIX, prefixLength) != 0) {
        return ZR_FALSE;
    }

    text += prefixLength;
    while (*text == '/') {
        text++;
    }

    length = strlen(text);
    if (length >= 3 && strcmp(text + length - 3, ".zr") == 0) {
        length -= 3;
    }
    if (length == 0 || length + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(moduleNameBuffer, text, length);
    moduleNameBuffer[length] = '\0';
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri,
                                                                     const ZrLibModuleDescriptor **outDescriptor,
                                                                     EZrLspImportedModuleSourceKind *outSourceKind,
                                                                     TZrChar *moduleNameBuffer,
                                                                     TZrSize bufferSize) {
    SZrString *moduleNameString;
    SZrLspResolvedImportedModule resolved;
    TZrBool parsedVirtualUri = ZR_FALSE;

    if (outDescriptor != ZR_NULL) {
        *outDescriptor = ZR_NULL;
    }
    if (outSourceKind != ZR_NULL) {
        *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    }
    if (state == ZR_NULL || uri == ZR_NULL || moduleNameBuffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(uri)) {
        SZrLspVirtualDocumentIdentity identity;
        SZrLspProjectIndex *owner = ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(context, uri);
        if (owner == ZR_NULL || (projectIndex != ZR_NULL && projectIndex != owner) ||
            !ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeDescriptor(
                    state, context, uri, &identity, ZR_NULL, outDescriptor) ||
            strlen(ZrCore_String_GetNativeString(identity.moduleName)) >= bufferSize) {
            if (outDescriptor != ZR_NULL) {
                *outDescriptor = ZR_NULL;
            }
            return ZR_FALSE;
        }
        strcpy(moduleNameBuffer, ZrCore_String_GetNativeString(identity.moduleName));
        if (outSourceKind != ZR_NULL) {
            *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
        }
        return ZR_TRUE;
    }

    parsedVirtualUri = ZrLanguageServer_LspVirtualDocuments_ParseDeclarationUri(uri, moduleNameBuffer, bufferSize);
    if (!parsedVirtualUri) {
        TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
        ZrLibRegisteredModuleInfo moduleInfo;

        memset(&moduleInfo, 0, sizeof(moduleInfo));
        if (state->global == ZR_NULL ||
            !ZrLanguageServer_Lsp_FileUriToNativePath(uri, nativePath, sizeof(nativePath)) ||
            !ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(state->global, nativePath, &moduleInfo) ||
            moduleInfo.moduleName == ZR_NULL ||
            moduleInfo.moduleName[0] == '\0' ||
            !(moduleInfo.registrationKind == ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN ||
              moduleInfo.isDescriptorPlugin)) {
            return ZR_FALSE;
        }

        if (strlen(moduleInfo.moduleName) + 1 > bufferSize) {
            return ZR_FALSE;
        }

        memcpy(moduleNameBuffer, moduleInfo.moduleName, strlen(moduleInfo.moduleName) + 1);
    }

    moduleNameString = ZrCore_String_Create(state, moduleNameBuffer, strlen(moduleNameBuffer));
    if (moduleNameString != ZR_NULL && projectIndex != ZR_NULL &&
        ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                 ZR_NULL,
                                                                 projectIndex,
                                                                 moduleNameString,
                                                                 &resolved) &&
        resolved.nativeDescriptor != ZR_NULL) {
        if (parsedVirtualUri && resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
            return ZR_FALSE;
        }
        if (outDescriptor != ZR_NULL) {
            *outDescriptor = resolved.nativeDescriptor;
        }
        if (outSourceKind != ZR_NULL) {
            *outSourceKind = resolved.sourceKind;
        }
        return ZR_TRUE;
    }

    if (outDescriptor != ZR_NULL) {
        EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
        *outDescriptor = ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state,
                                                                                          moduleNameBuffer,
                                                                                          &sourceKind);
        if (parsedVirtualUri && sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
            *outDescriptor = ZR_NULL;
            return ZR_FALSE;
        }
        if (outSourceKind != ZR_NULL) {
            *outSourceKind = sourceKind;
        }
    }
    return outDescriptor != ZR_NULL && *outDescriptor != ZR_NULL;
}

TZrBool ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(SZrState *state,
                                                                   const ZrLibModuleDescriptor *descriptor,
                                                                   SZrString *uri,
                                                                   SZrString **outText) {
    return ZrLanguageServer_LspNativeDeclarationProjection_Build(state, descriptor, uri, outText, ZR_NULL);
}

SZrFileRange ZrLanguageServer_LspVirtualDocuments_ModuleEntryRange(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

TZrBool ZrLanguageServer_LspVirtualDocuments_FindTypeMemberDeclaration(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       const void *declarationIdentity,
                                                                       TZrInt32 memberKind,
                                                                       SZrFileRange *outRange) {
    SZrArray records;
    TZrBool found = ZR_FALSE;
    const SZrLspVirtualRecord *match = ZR_NULL;
    EZrLspVirtualDeclarationKind kind;

    if (outRange != ZR_NULL) {
        memset(outRange, 0, sizeof(*outRange));
    }
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL ||
        declarationIdentity == ZR_NULL || outRange == ZR_NULL ||
        (memberKind != ZR_LSP_METADATA_MEMBER_FIELD && memberKind != ZR_LSP_METADATA_MEMBER_METHOD)) {
        return ZR_FALSE;
    }
    kind = memberKind == ZR_LSP_METADATA_MEMBER_FIELD
            ? ZR_LSP_VIRTUAL_DECLARATION_FIELD : ZR_LSP_VIRTUAL_DECLARATION_METHOD;
    if (virtual_documents_uri_is_virtual_declaration(uri)) {
        return ZrLanguageServer_LspNativeDeclarationProjection_Find(
                state, descriptor, uri, kind, declarationIdentity, outRange);
    }

    if (!virtual_documents_collect_records(state, descriptor, uri, &records)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < records.length; index++) {
        SZrLspVirtualRecord *record = (SZrLspVirtualRecord *)ZrCore_Array_Get(&records, index);
        if (record == ZR_NULL || record->kind != kind ||
            record->declarationIdentity != declarationIdentity) {
            continue;
        }
        if (match != ZR_NULL) {
            goto cleanup;
        }
        match = record;
    }
    if (match != ZR_NULL) {
        *outRange = match->range;
        found = ZR_TRUE;
    }

cleanup:
    ZrCore_Array_Free(state, &records);
    return found;
}

TZrBool ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       SZrLspPosition position,
                                                                       SZrLspVirtualDeclarationMatch *outMatch) {
    SZrArray records;
    SZrString *renderedText = ZR_NULL;
    const TZrChar *content = ZR_NULL;
    TZrSize contentLength = 0;

    if (outMatch != ZR_NULL) {
        memset(outMatch, 0, sizeof(*outMatch));
    }
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL || outMatch == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&records);
    if (virtual_documents_uri_is_virtual_declaration(uri)) {
        if (!ZrLanguageServer_LspNativeDeclarationProjection_Build(state, descriptor, uri, &renderedText, &records)) {
            return ZR_FALSE;
        }
        content = virtual_documents_string_text(renderedText);
        contentLength = content != ZR_NULL ? strlen(content) : 0;
    } else if (!virtual_documents_collect_records(state, descriptor, uri, &records)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < records.length; index++) {
        SZrLspVirtualRecord *record = (SZrLspVirtualRecord *)ZrCore_Array_Get(&records, index);

        if (record != ZR_NULL &&
            virtual_documents_range_contains_lsp_position(record->range, position, content, contentLength)) {
            outMatch->kind = record->kind;
            outMatch->descriptor = descriptor;
            outMatch->declarationIdentity = record->declarationIdentity;
            outMatch->ownerTypeDescriptor = record->ownerTypeDescriptor;
            outMatch->moduleName = descriptor->moduleName;
            outMatch->ownerName = record->ownerName;
            outMatch->name = record->name;
            outMatch->targetModuleName = record->targetModuleName;
            outMatch->range = record->range;
            ZrCore_Array_Free(state, &records);
            return ZR_TRUE;
        }
    }

    ZrCore_Array_Free(state, &records);
    return ZR_FALSE;
}
