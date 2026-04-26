#include "metadata/lsp_metadata_provider.h"
#include "interface/lsp_interface_internal.h"
#include "lsp_virtual_documents.h"

#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/type_inference.h"

#include <ctype.h>
#include <string.h>

static const TZrChar *metadata_provider_string_text(SZrString *value);
static TZrBool metadata_provider_try_get_analyzer_for_uri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri,
                                                          SZrSemanticAnalyzer **outAnalyzer);

static const TZrChar *metadata_provider_exact_type_failure_text(void) {
    return "cannot infer exact type";
}

static TZrBool metadata_provider_type_text_is_specific(const TZrChar *typeText) {
    return typeText != ZR_NULL && typeText[0] != '\0' &&
           strcmp(typeText, metadata_provider_exact_type_failure_text()) != 0 &&
           strcmp(typeText, "object") != 0 &&
           strcmp(typeText, "unknown") != 0;
}

static SZrFileRange metadata_provider_module_entry_range(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

static const TZrChar *metadata_provider_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static const TZrChar *metadata_provider_hover_source_text(const SZrLspResolvedMetadataMember *resolvedMember,
                                                          const TZrChar *defaultText) {
    if (resolvedMember != ZR_NULL &&
        resolvedMember->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER) {
        return "ffi extern";
    }

    return defaultText;
}

static const TZrChar *metadata_provider_type_text_or_failure(const TZrChar *typeText) {
    return metadata_provider_type_text_is_specific(typeText)
               ? typeText
               : metadata_provider_exact_type_failure_text();
}

static void metadata_provider_set_type_text(SZrState *state,
                                            SZrLspResolvedMetadataMember *outResolved,
                                            const TZrChar *typeText) {
    const TZrChar *storedTypeText;

    if (state == ZR_NULL || outResolved == ZR_NULL) {
        return;
    }

    storedTypeText = metadata_provider_type_text_or_failure(typeText);
    outResolved->resolvedTypeText =
        ZrCore_String_Create(state, (TZrNativeString)storedTypeText, strlen(storedTypeText));
}

static void metadata_provider_set_type_text_from_inferred(SZrState *state,
                                                          SZrLspResolvedMetadataMember *outResolved,
                                                          SZrInferredType *typeInfo) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText;

    if (state == ZR_NULL || outResolved == ZR_NULL || typeInfo == ZR_NULL) {
        return;
    }

    typeText = ZrParser_TypeNameString_Get(state, typeInfo, typeBuffer, sizeof(typeBuffer));
    metadata_provider_set_type_text(state,
                                    outResolved,
                                    metadata_provider_type_text_is_specific(typeText) ? typeText : ZR_NULL);
}

static const TZrChar *metadata_provider_precise_inferred_type_text(SZrState *state,
                                                                   const SZrInferredType *typeInfo,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize) {
    const TZrChar *typeText = ZR_NULL;

    if (state != ZR_NULL &&
        typeInfo != ZR_NULL &&
        !(typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
          typeInfo->typeName == ZR_NULL &&
          (!typeInfo->elementTypes.isValid || typeInfo->elementTypes.length == 0))) {
        typeText = ZrParser_TypeNameString_Get(state, typeInfo, buffer, bufferSize);
        if (metadata_provider_type_text_is_specific(typeText)) {
            return typeText;
        }
    }

    return metadata_provider_exact_type_failure_text();
}

static SZrString *metadata_provider_create_markdown_text(SZrState *state, const TZrChar *text) {
    return state != ZR_NULL && text != ZR_NULL
               ? ZrCore_String_Create(state, (TZrNativeString)text, strlen(text))
               : ZR_NULL;
}

static SZrFilePosition metadata_provider_file_position_from_offset(const TZrChar *content,
                                                                   TZrSize contentLength,
                                                                   TZrSize offset) {
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    if (content == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, 1, 1);
    }

    if (offset > contentLength) {
        offset = contentLength;
    }

    for (TZrSize index = 0; index < offset; index++) {
        if (content[index] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }

    return ZrParser_FilePosition_Create(offset, line, column);
}

static TZrBool metadata_provider_identifier_boundary(const TZrChar *content, TZrSize contentLength, TZrSize offset) {
    if (content == ZR_NULL || offset >= contentLength) {
        return ZR_TRUE;
    }

    return !(isalnum((unsigned char)content[offset]) || content[offset] == '_');
}

static TZrBool metadata_provider_try_member_name_range(SZrLspMetadataProvider *provider,
                                                       SZrString *uri,
                                                       SZrAstNode *declarationNode,
                                                       SZrString *memberName,
                                                       SZrFileRange *outRange) {
    SZrFileVersion *fileVersion;
    const TZrChar *content;
    TZrSize contentLength;
    const TZrChar *memberText;
    TZrSize memberLength;
    TZrSize searchStart;
    TZrSize searchEnd;

    if (provider == ZR_NULL || provider->context == ZR_NULL || uri == ZR_NULL || declarationNode == ZR_NULL ||
        memberName == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(provider->context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    content = fileVersion->content;
    contentLength = fileVersion->contentLength;
    memberText = metadata_provider_string_text(memberName);
    if (memberText == ZR_NULL || memberText[0] == '\0') {
        return ZR_FALSE;
    }

    memberLength = strlen(memberText);
    searchStart = declarationNode->location.start.offset <= contentLength
                      ? declarationNode->location.start.offset
                      : contentLength;
    searchEnd = declarationNode->location.end.offset > searchStart && declarationNode->location.end.offset <= contentLength
                    ? declarationNode->location.end.offset
                    : searchStart;

    while (searchStart > 0 && content[searchStart - 1] != '\n' && content[searchStart - 1] != '\r') {
        searchStart--;
    }
    while (searchEnd < contentLength && content[searchEnd] != '\n' && content[searchEnd] != '\r') {
        searchEnd++;
    }

    for (TZrSize index = searchStart; index + memberLength <= searchEnd; index++) {
        if (memcmp(content + index, memberText, memberLength) != 0) {
            continue;
        }

        if (!metadata_provider_identifier_boundary(content, contentLength, index == 0 ? contentLength : index - 1) ||
            !metadata_provider_identifier_boundary(content, contentLength, index + memberLength)) {
            continue;
        }

        *outRange = ZrParser_FileRange_Create(metadata_provider_file_position_from_offset(content, contentLength, index),
                                              metadata_provider_file_position_from_offset(content,
                                                                                         contentLength,
                                                                                         index + memberLength),
                                              uri);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrFileRange metadata_provider_type_member_declaration_range(SZrLspMetadataProvider *provider,
                                                                    SZrString *uri,
                                                                    SZrAstNode *declarationNode,
                                                                    SZrString *memberName) {
    if (declarationNode == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                         ZrParser_FilePosition_Create(0, 0, 0),
                                         ZR_NULL);
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_FIELD:
            return declarationNode->data.classField.nameLocation;
        case ZR_AST_STRUCT_FIELD:
            return declarationNode->location;
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.nameLocation;
        case ZR_AST_STRUCT_METHOD:
            return declarationNode->location;
        case ZR_AST_CLASS_PROPERTY:
            if (declarationNode->data.classProperty.modifier != ZR_NULL) {
                return metadata_provider_type_member_declaration_range(provider,
                                                                      uri,
                                                                      declarationNode->data.classProperty.modifier,
                                                                      memberName);
            }
            break;
        case ZR_AST_PROPERTY_GET:
            return declarationNode->data.propertyGet.nameLocation;
        case ZR_AST_PROPERTY_SET:
            return declarationNode->data.propertySet.nameLocation;
        case ZR_AST_ENUM_MEMBER: {
            SZrFileRange range;
            if (metadata_provider_try_member_name_range(provider, uri, declarationNode, memberName, &range)) {
                return range;
            }
            break;
        }
        default:
            break;
    }

    return declarationNode->location;
}

static TZrBool metadata_provider_node_declares_type(SZrAstNode *node, SZrString *typeName) {
    if (node == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            return node->data.classDeclaration.name != ZR_NULL &&
                   node->data.classDeclaration.name->name != ZR_NULL &&
                   ZrLanguageServer_Lsp_StringsEqual(node->data.classDeclaration.name->name, typeName);

        case ZR_AST_STRUCT_DECLARATION:
            return node->data.structDeclaration.name != ZR_NULL &&
                   node->data.structDeclaration.name->name != ZR_NULL &&
                   ZrLanguageServer_Lsp_StringsEqual(node->data.structDeclaration.name->name, typeName);

        case ZR_AST_ENUM_DECLARATION:
            return node->data.enumDeclaration.name != ZR_NULL &&
                   node->data.enumDeclaration.name->name != ZR_NULL &&
                   ZrLanguageServer_Lsp_StringsEqual(node->data.enumDeclaration.name->name, typeName);

        default:
            return ZR_FALSE;
    }
}

static SZrAstNode *metadata_provider_find_type_declaration_recursive(SZrAstNode *node, SZrString *typeName);

static SZrAstNode *metadata_provider_find_type_declaration_in_array(SZrAstNodeArray *nodes, SZrString *typeName) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        SZrAstNode *match = metadata_provider_find_type_declaration_recursive(nodes->nodes[index], typeName);
        if (match != ZR_NULL) {
            return match;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *metadata_provider_find_type_declaration_recursive(SZrAstNode *node, SZrString *typeName) {
    if (node == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (metadata_provider_node_declares_type(node, typeName)) {
        return node;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return metadata_provider_find_type_declaration_in_array(node->data.script.statements, typeName);

        case ZR_AST_BLOCK:
            return metadata_provider_find_type_declaration_in_array(node->data.block.body, typeName);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return metadata_provider_find_type_declaration_recursive(node->data.compileTimeDeclaration.declaration,
                                                                     typeName);

        case ZR_AST_EXTERN_BLOCK:
            return metadata_provider_find_type_declaration_in_array(node->data.externBlock.declarations, typeName);

        default:
            return ZR_NULL;
    }
}

static SZrAstNode *metadata_provider_find_type_declaration(SZrAstNode *ast, SZrString *typeName) {
    return metadata_provider_find_type_declaration_recursive(ast, typeName);
}

static SZrAstNode *metadata_provider_find_type_member_declaration(SZrAstNode *typeDeclaration,
                                                                  SZrString *memberName,
                                                                  EZrLspMetadataMemberKind *outKind) {
    SZrAstNodeArray *members = ZR_NULL;

    if (outKind != ZR_NULL) {
        *outKind = ZR_LSP_METADATA_MEMBER_NONE;
    }
    if (typeDeclaration == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclaration->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDeclaration->data.classDeclaration.members;
    } else if (typeDeclaration->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDeclaration->data.structDeclaration.members;
    } else if (typeDeclaration->type == ZR_AST_ENUM_DECLARATION) {
        members = typeDeclaration->data.enumDeclaration.members;
    } else {
        return ZR_NULL;
    }

    if (members == ZR_NULL || members->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        SZrString *name = ZR_NULL;
        EZrLspMetadataMemberKind kind = ZR_LSP_METADATA_MEMBER_NONE;

        if (member == ZR_NULL) {
            continue;
        }

        switch (member->type) {
            case ZR_AST_CLASS_FIELD:
                name = member->data.classField.name != ZR_NULL ? member->data.classField.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_FIELD;
                break;
            case ZR_AST_STRUCT_FIELD:
                name = member->data.structField.name != ZR_NULL ? member->data.structField.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_FIELD;
                break;
            case ZR_AST_CLASS_METHOD:
                name = member->data.classMethod.name != ZR_NULL ? member->data.classMethod.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_METHOD;
                break;
            case ZR_AST_STRUCT_METHOD:
                name = member->data.structMethod.name != ZR_NULL ? member->data.structMethod.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_METHOD;
                break;
            case ZR_AST_CLASS_PROPERTY:
                if (member->data.classProperty.modifier != ZR_NULL) {
                    if (member->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
                        member->data.classProperty.modifier->data.propertyGet.name != ZR_NULL) {
                        name = member->data.classProperty.modifier->data.propertyGet.name->name;
                        kind = ZR_LSP_METADATA_MEMBER_FIELD;
                    } else if (member->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
                               member->data.classProperty.modifier->data.propertySet.name != ZR_NULL) {
                        name = member->data.classProperty.modifier->data.propertySet.name->name;
                        kind = ZR_LSP_METADATA_MEMBER_FIELD;
                    }
                }
                break;
            case ZR_AST_ENUM_MEMBER:
                name = member->data.enumMember.name != ZR_NULL ? member->data.enumMember.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_CONSTANT;
                break;
            default:
                break;
        }

        if (name != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(name, memberName)) {
            if (outKind != ZR_NULL) {
                *outKind = kind;
            }
            return member;
        }
    }

    return ZR_NULL;
}

static SZrString *metadata_provider_append_markdown_section(SZrState *state, SZrString *base, SZrString *appendix) {
    TZrNativeString baseText;
    TZrNativeString appendixText;
    TZrSize baseLength;
    TZrSize appendixLength;
    TZrChar buffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize used = 0;

    if (state == ZR_NULL || base == ZR_NULL || appendix == ZR_NULL) {
        return base;
    }

    if (base->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        baseText = ZrCore_String_GetNativeStringShort(base);
        baseLength = base->shortStringLength;
    } else {
        baseText = ZrCore_String_GetNativeString(base);
        baseLength = base->longStringLength;
    }

    if (appendix->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        appendixText = ZrCore_String_GetNativeStringShort(appendix);
        appendixLength = appendix->shortStringLength;
    } else {
        appendixText = ZrCore_String_GetNativeString(appendix);
        appendixLength = appendix->longStringLength;
    }

    if (baseText == ZR_NULL || appendixText == ZR_NULL || appendixLength == 0 ||
        baseLength + appendixLength + 3 >= sizeof(buffer) || strstr(baseText, appendixText) != ZR_NULL) {
        return base;
    }

    memcpy(buffer + used, baseText, baseLength);
    used += baseLength;
    memcpy(buffer + used, "\n\n", 2);
    used += 2;
    memcpy(buffer + used, appendixText, appendixLength);
    used += appendixLength;
    buffer[used] = '\0';
    return ZrCore_String_Create(state, buffer, used);
}

static TZrBool metadata_provider_create_hover(SZrState *state,
                                              SZrString *content,
                                              SZrFileRange range,
                                              SZrLspHover **result) {
    SZrLspHover *hover;

    if (state == ZR_NULL || content == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_LspRange_FromFileRange(range);
    *result = hover;
    return ZR_TRUE;
}

static TZrBool metadata_provider_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLanguageServer_Lsp_FileUriToNativePath(uri, buffer, bufferSize);
}

static TZrBool metadata_provider_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL && position.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

static SZrFileRange metadata_provider_native_type_member_declaration_range(SZrString *uri,
                                                                           TZrSize typeIndex,
                                                                           TZrSize memberIndex,
                                                                           EZrLspMetadataMemberKind memberKind,
                                                                           const TZrChar *memberName) {
    TZrSize nameLength = memberName != ZR_NULL && memberName[0] != '\0' ? strlen(memberName) : 1;
    TZrInt32 startLine = (TZrInt32)(typeIndex + 2);
    TZrInt32 startColumn = (TZrInt32)((memberKind == ZR_LSP_METADATA_MEMBER_METHOD ? 129 : 1) + memberIndex * 8);
    TZrInt32 endColumn = startColumn + (TZrInt32)nameLength;
    SZrFilePosition start = ZrParser_FilePosition_Create(0, startLine, startColumn);
    SZrFilePosition end = ZrParser_FilePosition_Create(0, startLine, endColumn);

    return ZrParser_FileRange_Create(start, end, uri);
}

static TZrBool metadata_provider_try_get_native_type_index(const ZrLibModuleDescriptor *module,
                                                           const ZrLibTypeDescriptor *typeDescriptor,
                                                           TZrSize *outIndex) {
    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (module == ZR_NULL || typeDescriptor == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < module->typeCount; index++) {
        if (&module->types[index] == typeDescriptor) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_provider_try_get_native_field_index(const ZrLibTypeDescriptor *typeDescriptor,
                                                            const ZrLibFieldDescriptor *fieldDescriptor,
                                                            TZrSize *outIndex) {
    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (typeDescriptor == ZR_NULL || fieldDescriptor == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < typeDescriptor->fieldCount; index++) {
        if (&typeDescriptor->fields[index] == fieldDescriptor) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_provider_try_get_native_method_index(const ZrLibTypeDescriptor *typeDescriptor,
                                                             const ZrLibMethodDescriptor *methodDescriptor,
                                                             TZrSize *outIndex) {
    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (typeDescriptor == ZR_NULL || methodDescriptor == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < typeDescriptor->methodCount; index++) {
        if (&typeDescriptor->methods[index] == methodDescriptor) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_provider_try_get_analyzer_for_uri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri,
                                                          SZrSemanticAnalyzer **outAnalyzer) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    TZrNativeString sourceBuffer = ZR_NULL;
    TZrSize sourceLength = 0;
    TZrBool loadedFromDisk = ZR_FALSE;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        sourceBuffer = fileVersion->content;
        sourceLength = fileVersion->contentLength;
    } else if (state->global != ZR_NULL && metadata_provider_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        sourceBuffer = ZrLibrary_File_ReadAll(state->global, nativePath);
        sourceLength = sourceBuffer != ZR_NULL ? strlen(sourceBuffer) : 0;
        loadedFromDisk = sourceBuffer != ZR_NULL;
    }

    if (sourceBuffer == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state,
                                                 context,
                                                 uri,
                                                 sourceBuffer,
                                                 sourceLength,
                                                 fileVersion != ZR_NULL ? fileVersion->version : 0,
                                                 ZR_FALSE)) {
        if (loadedFromDisk) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceBuffer,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        }
        return ZR_FALSE;
    }

    if (loadedFromDisk) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
    }

    return ZR_TRUE;
}

static SZrSymbol *metadata_provider_find_public_global_symbol(SZrSemanticAnalyzer *analyzer,
                                                              SZrString *memberName) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || analyzer->symbolTable->globalScope == ZR_NULL ||
        memberName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->symbolTable->globalScope->symbols.length; index++) {
        SZrSymbol **symbolPtr =
            (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, index);
        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || (*symbolPtr)->name == ZR_NULL ||
            (*symbolPtr)->accessModifier != ZR_ACCESS_PUBLIC) {
            continue;
        }

        if (ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->name, memberName)) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static const TZrChar *metadata_provider_symbol_kind_text(EZrSymbolType type) {
    switch (type) {
        case ZR_SYMBOL_MODULE: return "module";
        case ZR_SYMBOL_CLASS: return "class";
        case ZR_SYMBOL_STRUCT: return "struct";
        case ZR_SYMBOL_INTERFACE: return "interface";
        case ZR_SYMBOL_ENUM: return "enum";
        case ZR_SYMBOL_METHOD: return "method";
        case ZR_SYMBOL_PROPERTY: return "property";
        case ZR_SYMBOL_FIELD: return "field";
        case ZR_SYMBOL_FUNCTION: return "function";
        case ZR_SYMBOL_PARAMETER: return "parameter";
        default: return "variable";
    }
}

static void metadata_provider_resolve_symbol_descriptor(SZrState *state,
                                                        SZrSymbol *symbol,
                                                        SZrLspResolvedMetadataMember *outResolved) {
    if (state == ZR_NULL || symbol == ZR_NULL || outResolved == ZR_NULL) {
        return;
    }

    outResolved->declarationSymbol = symbol;
    if (symbol->type == ZR_SYMBOL_FUNCTION || symbol->type == ZR_SYMBOL_METHOD) {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_FUNCTION;
    } else if (symbol->type == ZR_SYMBOL_CLASS || symbol->type == ZR_SYMBOL_STRUCT ||
               symbol->type == ZR_SYMBOL_INTERFACE || symbol->type == ZR_SYMBOL_ENUM) {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_TYPE;
    } else {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_CONSTANT;
    }

    metadata_provider_set_type_text_from_inferred(state, outResolved, symbol->typeInfo);
}

static void metadata_provider_attach_native_hint_metadata(const ZrLibModuleDescriptor *descriptor,
                                                          SZrString *memberName,
                                                          SZrLspResolvedMetadataMember *outResolved) {
    const TZrChar *memberText = metadata_provider_string_text(memberName);

    if (descriptor == ZR_NULL || memberText == ZR_NULL || outResolved == ZR_NULL ||
        outResolved->memberKind == ZR_LSP_METADATA_MEMBER_NONE) {
        return;
    }

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *typeHint = &descriptor->typeHints[index];
        if (typeHint->symbolName != ZR_NULL && strcmp(typeHint->symbolName, memberText) == 0) {
            outResolved->typeHintDescriptor = typeHint;
            return;
        }
    }
}

static void metadata_provider_find_native_member(SZrState *state,
                                                 const ZrLibModuleDescriptor *descriptor,
                                                 SZrString *memberName,
                                                 SZrLspResolvedMetadataMember *outResolved) {
    const TZrChar *memberText = metadata_provider_string_text(memberName);

    if (state == ZR_NULL || descriptor == ZR_NULL || memberText == ZR_NULL || outResolved == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *moduleLink = &descriptor->moduleLinks[index];
        if (moduleLink->name != ZR_NULL && strcmp(moduleLink->name, memberText) == 0) {
            outResolved->memberKind = ZR_LSP_METADATA_MEMBER_MODULE;
            outResolved->moduleLinkDescriptor = moduleLink;
            metadata_provider_set_type_text(state, outResolved, moduleLink->moduleName);
            return;
        }
    }

    for (TZrSize index = 0; index < descriptor->constantCount; index++) {
        const ZrLibConstantDescriptor *constantDescriptor = &descriptor->constants[index];
        if (constantDescriptor->name != ZR_NULL && strcmp(constantDescriptor->name, memberText) == 0) {
            outResolved->memberKind = ZR_LSP_METADATA_MEMBER_CONSTANT;
            outResolved->constantDescriptor = constantDescriptor;
            metadata_provider_set_type_text(state, outResolved, constantDescriptor->typeName);
            return;
        }
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        if (functionDescriptor->name != ZR_NULL && strcmp(functionDescriptor->name, memberText) == 0) {
            outResolved->memberKind = ZR_LSP_METADATA_MEMBER_FUNCTION;
            outResolved->functionDescriptor = functionDescriptor;
            metadata_provider_set_type_text(state, outResolved, functionDescriptor->returnTypeName);
            return;
        }
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, memberText) == 0) {
            outResolved->memberKind = ZR_LSP_METADATA_MEMBER_TYPE;
            outResolved->typeDescriptor = typeDescriptor;
            metadata_provider_set_type_text(state, outResolved, typeDescriptor->name);
            return;
        }
    }

    metadata_provider_attach_native_hint_metadata(descriptor, memberName, outResolved);
}

static const SZrTypePrototypeInfo *metadata_provider_find_type_prototype(SZrSemanticAnalyzer *analyzer,
                                                                         const TZrChar *typeName) {
    return ZrLanguageServer_LspModuleMetadata_FindTypePrototype(analyzer, typeName);
}

static const SZrTypeMemberInfo *metadata_provider_find_module_member(const SZrTypePrototypeInfo *modulePrototype,
                                                                     SZrString *memberName) {
    if (modulePrototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < modulePrototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&modulePrototype->members, index);
        if (member != ZR_NULL &&
            member->name != ZR_NULL &&
            !member->isMetaMethod &&
            ZrLanguageServer_Lsp_StringsEqual(member->name, memberName)) {
            return member;
        }
    }

    return ZR_NULL;
}

static const TZrChar *metadata_provider_module_member_kind_text(SZrSemanticAnalyzer *analyzer,
                                                                const SZrTypeMemberInfo *member) {
    const SZrTypePrototypeInfo *prototype;

    if (member == ZR_NULL) {
        return ZR_NULL;
    }

    if ((member->memberType == ZR_AST_CLASS_FIELD || member->memberType == ZR_AST_STRUCT_FIELD) &&
        member->fieldTypeName != ZR_NULL) {
        prototype = metadata_provider_find_type_prototype(analyzer, metadata_provider_string_text(member->fieldTypeName));
        if (prototype != ZR_NULL) {
            switch (prototype->type) {
                case ZR_OBJECT_PROTOTYPE_TYPE_CLASS: return "class";
                case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT: return "struct";
                case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE: return "interface";
                case ZR_OBJECT_PROTOTYPE_TYPE_MODULE: return "module";
                case ZR_OBJECT_PROTOTYPE_TYPE_ENUM: return "enum";
                default:
                    break;
            }
        }
    }

    switch (member->memberType) {
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
            return "function";
        case ZR_AST_CLASS_PROPERTY:
            return "property";
        case ZR_AST_CLASS_FIELD:
        case ZR_AST_STRUCT_FIELD:
            return "field";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *metadata_provider_binary_type_ref_text(const SZrIoFunctionTypedTypeRef *typeRef,
                                                             TZrChar *buffer,
                                                             TZrSize bufferSize) {
    const TZrChar *baseName = metadata_provider_exact_type_failure_text();

    if (buffer == ZR_NULL || bufferSize == 0) {
        return metadata_provider_exact_type_failure_text();
    }

    buffer[0] = '\0';
    if (typeRef == ZR_NULL) {
        return metadata_provider_exact_type_failure_text();
    }

    if (typeRef->isArray) {
        const TZrChar *elementName = typeRef->elementTypeName != ZR_NULL
                                         ? metadata_provider_string_text(typeRef->elementTypeName)
                                         : ZR_NULL;
        if (elementName == ZR_NULL || elementName[0] == '\0') {
            switch (typeRef->elementBaseType) {
                case ZR_VALUE_TYPE_BOOL: elementName = "bool"; break;
                case ZR_VALUE_TYPE_INT8: elementName = "i8"; break;
                case ZR_VALUE_TYPE_INT16: elementName = "i16"; break;
                case ZR_VALUE_TYPE_INT32: elementName = "i32"; break;
                case ZR_VALUE_TYPE_INT64: elementName = "int"; break;
                case ZR_VALUE_TYPE_UINT8: elementName = "u8"; break;
                case ZR_VALUE_TYPE_UINT16: elementName = "u16"; break;
                case ZR_VALUE_TYPE_UINT32: elementName = "u32"; break;
                case ZR_VALUE_TYPE_UINT64: elementName = "uint"; break;
                case ZR_VALUE_TYPE_FLOAT:
                case ZR_VALUE_TYPE_DOUBLE:
                    elementName = "float";
                    break;
                case ZR_VALUE_TYPE_STRING:
                    elementName = "string";
                    break;
                default:
                    elementName = metadata_provider_exact_type_failure_text();
                    break;
            }
        }
        snprintf(buffer, bufferSize, "%s[]", elementName);
        return buffer;
    }

    if (typeRef->typeName != ZR_NULL) {
        const TZrChar *typeName = metadata_provider_string_text(typeRef->typeName);
        if (typeName != ZR_NULL && typeName[0] != '\0') {
            return typeName;
        }
    }

    switch (typeRef->baseType) {
        case ZR_VALUE_TYPE_NULL: baseName = "null"; break;
        case ZR_VALUE_TYPE_BOOL: baseName = "bool"; break;
        case ZR_VALUE_TYPE_INT8: baseName = "i8"; break;
        case ZR_VALUE_TYPE_INT16: baseName = "i16"; break;
        case ZR_VALUE_TYPE_INT32: baseName = "i32"; break;
        case ZR_VALUE_TYPE_INT64: baseName = "int"; break;
        case ZR_VALUE_TYPE_UINT8: baseName = "u8"; break;
        case ZR_VALUE_TYPE_UINT16: baseName = "u16"; break;
        case ZR_VALUE_TYPE_UINT32: baseName = "u32"; break;
        case ZR_VALUE_TYPE_UINT64: baseName = "uint"; break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            baseName = "float";
            break;
        case ZR_VALUE_TYPE_STRING:
            baseName = "string";
            break;
        case ZR_VALUE_TYPE_ARRAY:
            baseName = "array";
            break;
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_FUNCTION:
            baseName = "function";
            break;
        default:
            baseName = metadata_provider_exact_type_failure_text();
            break;
    }

    snprintf(buffer, bufferSize, "%s", baseName);
    return buffer;
}

static TZrBool metadata_provider_binary_export_is_callable(const SZrIoFunctionTypedExportSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return ZR_TRUE;
    }

    return symbol->valueType.baseType == ZR_VALUE_TYPE_FUNCTION ||
           symbol->valueType.baseType == ZR_VALUE_TYPE_CLOSURE;
}

static void metadata_provider_resolve_binary_member(SZrState *state,
                                                    SZrIoSource *binarySource,
                                                    SZrString *memberName,
                                                    SZrLspResolvedMetadataMember *outResolved) {
    const SZrIoFunction *entryFunction;
    const TZrChar *memberText = metadata_provider_string_text(memberName);
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (state == ZR_NULL || binarySource == ZR_NULL || memberText == ZR_NULL || outResolved == ZR_NULL ||
        binarySource->modulesLength == 0 || binarySource->modules == ZR_NULL ||
        binarySource->modules[0].entryFunction == ZR_NULL) {
        return;
    }

    entryFunction = binarySource->modules[0].entryFunction;
    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        const TZrChar *symbolText = symbol->name != ZR_NULL ? metadata_provider_string_text(symbol->name) : ZR_NULL;

        if (symbolText == ZR_NULL || strcmp(symbolText, memberText) != 0) {
            continue;
        }

        outResolved->memberKind = metadata_provider_binary_export_is_callable(symbol)
                                      ? ZR_LSP_METADATA_MEMBER_FUNCTION
                                      : ZR_LSP_METADATA_MEMBER_CONSTANT;
        metadata_provider_set_type_text(state,
                                        outResolved,
                                        metadata_provider_binary_type_ref_text(&symbol->valueType,
                                                                               typeBuffer,
                                                                               sizeof(typeBuffer)));
        return;
    }
}

static void metadata_provider_resolve_module_prototype_member(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              const SZrTypePrototypeInfo *modulePrototype,
                                                              SZrString *memberName,
                                                              SZrLspResolvedMetadataMember *outResolved) {
    const SZrTypeMemberInfo *member = metadata_provider_find_module_member(modulePrototype, memberName);
    const TZrChar *kindText = metadata_provider_module_member_kind_text(analyzer, member);

    if (state == ZR_NULL || member == ZR_NULL || kindText == ZR_NULL || outResolved == ZR_NULL) {
        return;
    }

    outResolved->memberKind = strcmp(kindText, "function") == 0 ? ZR_LSP_METADATA_MEMBER_FUNCTION
                                                                 : ZR_LSP_METADATA_MEMBER_CONSTANT;
    metadata_provider_set_type_text(state,
                                    outResolved,
                                    strcmp(kindText, "function") == 0
                                        ? metadata_provider_string_text(member->returnTypeName)
                                        : metadata_provider_string_text(member->fieldTypeName));
}

static void metadata_provider_append_project_symbol_completion(SZrState *state,
                                                               SZrSymbol *symbol,
                                                               SZrArray *result) {
    TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText = metadata_provider_exact_type_failure_text();
    SZrCompletionItem *item;

    if (state == ZR_NULL || symbol == ZR_NULL || result == ZR_NULL || symbol->name == ZR_NULL) {
        return;
    }

    typeText = metadata_provider_precise_inferred_type_text(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer));

    snprintf(detail,
             sizeof(detail),
             "%s %s",
             metadata_provider_symbol_kind_text(symbol->type),
             metadata_provider_type_text_or_failure(typeText));
    item = ZrLanguageServer_CompletionItem_New(state,
                                               metadata_provider_string_text(symbol->name),
                                               metadata_provider_symbol_kind_text(symbol->type),
                                               detail,
                                               ZR_NULL,
                                               symbol->typeInfo);
    if (item != ZR_NULL) {
        ZrCore_Array_Push(state, result, &item);
    }
}

static TZrBool metadata_provider_completion_items_contain_label(SZrArray *items, const TZrChar *label) {
    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(items, index);
        const TZrChar *itemLabel;

        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        itemLabel = metadata_provider_string_text((*itemPtr)->label);
        if (itemLabel != ZR_NULL && strcmp(itemLabel, label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void metadata_provider_append_module_prototype_completions(SZrState *state,
                                                                  SZrSemanticAnalyzer *analyzer,
                                                                  const SZrTypePrototypeInfo *modulePrototype,
                                                                  SZrArray *result,
                                                                  TZrBool missingOnly) {
    if (state == ZR_NULL || modulePrototype == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < modulePrototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&modulePrototype->members, index);
        const TZrChar *kind = metadata_provider_module_member_kind_text(analyzer, member);
        const TZrChar *detailType = metadata_provider_exact_type_failure_text();
        const TZrChar *label;
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (member == ZR_NULL || member->name == ZR_NULL || member->isMetaMethod || kind == ZR_NULL) {
            continue;
        }

        label = metadata_provider_string_text(member->name);
        if (label == ZR_NULL || (missingOnly && metadata_provider_completion_items_contain_label(result, label))) {
            continue;
        }

        if (strcmp(kind, "function") == 0) {
            detailType = metadata_provider_string_text(member->returnTypeName);
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     label,
                     metadata_provider_type_text_or_failure(detailType));
        } else {
            detailType = metadata_provider_string_text(member->fieldTypeName);
            snprintf(detail,
                     sizeof(detail),
                     "%s %s",
                     kind,
                     metadata_provider_type_text_or_failure(detailType));
        }

        item = ZrLanguageServer_CompletionItem_New(state, label, kind, detail, ZR_NULL, ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static void metadata_provider_append_binary_module_completions(SZrState *state,
                                                               const SZrIoFunction *entryFunction,
                                                               SZrArray *result) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        const TZrChar *kind = metadata_provider_binary_export_is_callable(symbol) ? "function" : "field";
        const TZrChar *name = symbol->name != ZR_NULL ? metadata_provider_string_text(symbol->name) : ZR_NULL;
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (name == ZR_NULL) {
            continue;
        }

        if (metadata_provider_binary_export_is_callable(symbol)) {
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     name,
                     metadata_provider_binary_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)));
        } else {
            snprintf(detail,
                     sizeof(detail),
                     "field %s",
                     metadata_provider_binary_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)));
        }

        item = ZrLanguageServer_CompletionItem_New(state, name, kind, detail, ZR_NULL, ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < entryFunction->classesLength; index++) {
        const SZrIoClass *classInfo = &entryFunction->classes[index];
        const TZrChar *name = classInfo->name != ZR_NULL ? metadata_provider_string_text(classInfo->name) : ZR_NULL;
        SZrCompletionItem *item;

        if (name == ZR_NULL || name[0] == '\0' || metadata_provider_completion_items_contain_label(result, name)) {
            continue;
        }

        item = ZrLanguageServer_CompletionItem_New(state, name, "class", name, ZR_NULL, ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < entryFunction->structsLength; index++) {
        const SZrIoStruct *structInfo = &entryFunction->structs[index];
        const TZrChar *name = structInfo->name != ZR_NULL ? metadata_provider_string_text(structInfo->name) : ZR_NULL;
        SZrCompletionItem *item;

        if (name == ZR_NULL || name[0] == '\0' || metadata_provider_completion_items_contain_label(result, name)) {
            continue;
        }

        item = ZrLanguageServer_CompletionItem_New(state, name, "struct", name, ZR_NULL, ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static void metadata_provider_append_native_module_completions(SZrState *state,
                                                               const ZrLibModuleDescriptor *descriptor,
                                                               SZrArray *result) {
    if (state == ZR_NULL || descriptor == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *link = &descriptor->moduleLinks[index];
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (link->name == ZR_NULL || link->moduleName == ZR_NULL) {
            continue;
        }

        snprintf(detail, sizeof(detail), "module <%s>", link->moduleName);
        item = ZrLanguageServer_CompletionItem_New(state,
                                                   link->name,
                                                   "module",
                                                   detail,
                                                   link->documentation,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < descriptor->constantCount; index++) {
        const ZrLibConstantDescriptor *constantDescriptor = &descriptor->constants[index];
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (constantDescriptor->name == ZR_NULL) {
            continue;
        }

        snprintf(detail,
                 sizeof(detail),
                 "constant %s",
                 metadata_provider_type_text_or_failure(constantDescriptor->typeName));
        item = ZrLanguageServer_CompletionItem_New(state,
                                                   constantDescriptor->name,
                                                   "constant",
                                                   detail,
                                                   constantDescriptor->documentation,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (functionDescriptor->name == ZR_NULL) {
            continue;
        }

        snprintf(detail,
                 sizeof(detail),
                 "%s(...): %s",
                 functionDescriptor->name,
                 metadata_provider_type_text_or_failure(functionDescriptor->returnTypeName));
        item = ZrLanguageServer_CompletionItem_New(state,
                                                   functionDescriptor->name,
                                                   "function",
                                                   detail,
                                                   functionDescriptor->documentation,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        const TZrChar *kind;
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (typeDescriptor->name == ZR_NULL) {
            continue;
        }

        kind = typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS ? "class" : "struct";
        snprintf(detail, sizeof(detail), "%s %s", kind, typeDescriptor->name);
        item = ZrLanguageServer_CompletionItem_New(state,
                                                   typeDescriptor->name,
                                                   kind,
                                                   detail,
                                                   typeDescriptor->documentation,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

}

void ZrLanguageServer_LspMetadataProvider_Init(SZrLspMetadataProvider *provider,
                                               SZrState *state,
                                               SZrLspContext *context) {
    if (provider == ZR_NULL) {
        return;
    }

    memset(provider, 0, sizeof(*provider));
    provider->state = state;
    provider->context = context;
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(SZrLspMetadataProvider *provider,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrLspProjectIndex *projectIndex,
                                                                   SZrString *moduleName,
                                                                   SZrLspResolvedImportedModule *outResolved) {
    if (provider == ZR_NULL) {
        return ZR_FALSE;
    }

    if (provider->state != ZR_NULL &&
        provider->context != ZR_NULL &&
        projectIndex != ZR_NULL &&
        moduleName != ZR_NULL &&
        ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, moduleName) == ZR_NULL) {
        ZrLanguageServer_LspProject_EnsureModuleLoadedByName(provider->state,
                                                             provider->context,
                                                             projectIndex,
                                                             moduleName);
    }

    return ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(provider->state,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    moduleName,
                                                                    outResolved);
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(SZrLspMetadataProvider *provider,
                                                                        SZrSemanticAnalyzer *analyzer,
                                                                        SZrLspProjectIndex *projectIndex,
                                                                        SZrString *moduleName,
                                                                        SZrLspResolvedImportedModuleEntry *outResolved) {
    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (provider == ZR_NULL || outResolved == ZR_NULL ||
        !ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    moduleName,
                                                                    &outResolved->module)) {
        return ZR_FALSE;
    }

    if (outResolved->module.sourceRecord != ZR_NULL && outResolved->module.sourceRecord->uri != ZR_NULL) {
        outResolved->declarationUri = outResolved->module.sourceRecord->uri;
        outResolved->declarationRange = metadata_provider_module_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    } else if (outResolved->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
               ZrLanguageServer_LspMetadataProvider_ResolveBinaryModuleUri(provider,
                                                                           projectIndex,
                                                                           moduleName,
                                                                           &outResolved->declarationUri) &&
               outResolved->declarationUri != ZR_NULL) {
        outResolved->declarationRange = metadata_provider_module_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    } else if (ZrLanguageServer_LspMetadataProvider_ResolveNativeModuleUri(provider,
                                                                           projectIndex,
                                                                           moduleName,
                                                                           &outResolved->declarationUri) &&
               outResolved->declarationUri != ZR_NULL) {
        outResolved->declarationRange = metadata_provider_module_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspMetadataProvider_LoadBinaryModuleSource(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrIoSource **outSource) {
    if (provider == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(provider->state,
                                                                     projectIndex,
                                                                     moduleName,
                                                                     outSource);
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryModuleUri(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrString **outUri) {
    if (provider == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(provider->state,
                                                                     projectIndex,
                                                                     moduleName,
                                                                     outUri);
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryExportDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrString **outUri,
    SZrFileRange *outRange) {
    if (provider == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_LspModuleMetadata_ResolveBinaryExportDeclaration(provider->state,
                                                                             projectIndex,
                                                                             moduleName,
                                                                             memberName,
                                                                             outUri,
                                                                             outRange);
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveNativeModuleUri(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrString **outUri) {
    if (provider == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(provider->state,
                                                                     projectIndex,
                                                                     moduleName,
                                                                     outUri);
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrLspResolvedMetadataMember *resolvedMember) {
    const TZrChar *ownerTypeText;
    const TZrChar *memberText;

    if (provider == ZR_NULL || provider->state == ZR_NULL || resolvedMember == ZR_NULL ||
        resolvedMember->module.moduleName == ZR_NULL || resolvedMember->ownerTypeDescriptor == ZR_NULL ||
        resolvedMember->module.nativeDescriptor == ZR_NULL ||
        (resolvedMember->memberKind != ZR_LSP_METADATA_MEMBER_FIELD &&
         resolvedMember->memberKind != ZR_LSP_METADATA_MEMBER_METHOD)) {
        return ZR_FALSE;
    }

    ownerTypeText = resolvedMember->ownerTypeDescriptor->name;
    memberText = metadata_provider_string_text(resolvedMember->memberName);
    if (!ZrLanguageServer_LspMetadataProvider_ResolveNativeModuleUri(provider,
                                                                     projectIndex,
                                                                     resolvedMember->module.moduleName,
                                                                     &resolvedMember->declarationUri) ||
        resolvedMember->declarationUri == ZR_NULL ||
        ownerTypeText == ZR_NULL || memberText == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocuments_FindTypeMemberDeclaration(provider->state,
                                                                        resolvedMember->module.nativeDescriptor,
                                                                        resolvedMember->declarationUri,
                                                                        ownerTypeText,
                                                                        memberText,
                                                                        resolvedMember->memberKind,
                                                                        &resolvedMember->declarationRange)) {
        return ZR_FALSE;
    }

    resolvedMember->hasDeclaration = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveProjectTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *ownerTypeName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolved) {
    if (provider == ZR_NULL || provider->state == ZR_NULL || provider->context == ZR_NULL ||
        projectIndex == ZR_NULL || ownerTypeName == ZR_NULL || memberName == ZR_NULL ||
        outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        SZrLspProjectFileRecord *record;
        SZrSemanticAnalyzer *targetAnalyzer = ZR_NULL;
        SZrAstNode *typeDeclaration;
        SZrAstNode *memberDeclaration;
        EZrLspMetadataMemberKind memberKind = ZR_LSP_METADATA_MEMBER_NONE;

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL) {
            continue;
        }

        record = *recordPtr;
        if (record->uri == ZR_NULL ||
            !metadata_provider_try_get_analyzer_for_uri(provider->state,
                                                        provider->context,
                                                        record->uri,
                                                        &targetAnalyzer) ||
            targetAnalyzer == ZR_NULL ||
            targetAnalyzer->ast == ZR_NULL) {
            continue;
        }

        typeDeclaration = metadata_provider_find_type_declaration(targetAnalyzer->ast, ownerTypeName);
        if (typeDeclaration == ZR_NULL) {
            continue;
        }

        memberDeclaration = metadata_provider_find_type_member_declaration(typeDeclaration, memberName, &memberKind);
        if (memberDeclaration == ZR_NULL || memberKind == ZR_LSP_METADATA_MEMBER_NONE) {
            continue;
        }

        outResolved->module.projectIndex = projectIndex;
        outResolved->module.sourceRecord = record;
        outResolved->module.moduleName = record->moduleName;
        outResolved->module.sourceKind = record->isFfiWrapperSource
                                             ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
                                             : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
        outResolved->memberName = memberName;
        outResolved->memberKind = memberKind;
        outResolved->ownerTypeName = ownerTypeName;
        outResolved->declarationAnalyzer = targetAnalyzer;
        outResolved->declarationUri = record->uri;
        outResolved->declarationRange =
            metadata_provider_type_member_declaration_range(provider, record->uri, memberDeclaration, memberName);
        outResolved->hasDeclaration = outResolved->declarationUri != ZR_NULL;
        outResolved->declarationSymbol =
            ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(targetAnalyzer, outResolved->declarationRange);
        if (outResolved->resolvedTypeText == ZR_NULL && outResolved->declarationSymbol != ZR_NULL) {
            metadata_provider_set_type_text_from_inferred(provider->state,
                                                          outResolved,
                                                          outResolved->declarationSymbol->typeInfo);
        }
        return outResolved->hasDeclaration;
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspMetadataProvider_FindNativeTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspResolvedMetadataMember *outResolved) {
    const ZrLibModuleDescriptor *descriptor = ZR_NULL;
    EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *moduleName;
    SZrLspVirtualDeclarationMatch match;
    const ZrLibTypeDescriptor *ownerTypeDescriptor = ZR_NULL;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (provider == ZR_NULL || provider->state == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(provider->state,
                                                                      projectIndex,
                                                                      uri,
                                                                      &descriptor,
                                                                      &sourceKind,
                                                                      moduleNameBuffer,
                                                                      sizeof(moduleNameBuffer))) {
        return ZR_FALSE;
    }

    memset(&match, 0, sizeof(match));
    if (!ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(provider->state,
                                                                        descriptor,
                                                                        uri,
                                                                        position,
                                                                        &match) ||
        (match.kind != ZR_LSP_VIRTUAL_DECLARATION_FIELD &&
         match.kind != ZR_LSP_VIRTUAL_DECLARATION_METHOD)) {
        return ZR_FALSE;
    }

    moduleName = ZrCore_String_Create(provider->state,
                                      (TZrNativeString)moduleNameBuffer,
                                      strlen(moduleNameBuffer));
    if (moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(provider,
                                                                    ZR_NULL,
                                                                    projectIndex,
                                                                    moduleName,
                                                                    &outResolved->module)) {
        memset(&outResolved->module, 0, sizeof(outResolved->module));
        outResolved->module.projectIndex = projectIndex;
        outResolved->module.moduleName = moduleName;
        outResolved->module.nativeDescriptor = descriptor;
        outResolved->module.sourceKind = sourceKind;
    }
    if (outResolved->module.nativeDescriptor == ZR_NULL) {
        outResolved->module.nativeDescriptor = descriptor;
        outResolved->module.moduleName = moduleName;
        outResolved->module.sourceKind = sourceKind;
    }

    for (TZrSize typeIndex = 0; typeIndex < descriptor->typeCount; typeIndex++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[typeIndex];

        if (typeDescriptor != ZR_NULL &&
            typeDescriptor->name != ZR_NULL &&
            match.ownerName != ZR_NULL &&
            strcmp(typeDescriptor->name, match.ownerName) == 0) {
            ownerTypeDescriptor = typeDescriptor;
            break;
        }
    }

    if (ownerTypeDescriptor == ZR_NULL || match.name == ZR_NULL) {
        return ZR_FALSE;
    }

    outResolved->module.moduleName = moduleName;
    outResolved->memberName = ZrCore_String_Create(provider->state,
                                                   (TZrNativeString)match.name,
                                                   strlen(match.name));
    outResolved->ownerTypeDescriptor = ownerTypeDescriptor;
    outResolved->typeDescriptor = ownerTypeDescriptor;
    outResolved->ownerTypeName = ZrCore_String_Create(provider->state,
                                                      (TZrNativeString)ownerTypeDescriptor->name,
                                                      strlen(ownerTypeDescriptor->name));
    outResolved->declarationUri = uri;
    outResolved->declarationRange = match.range;
    outResolved->hasDeclaration = ZR_TRUE;

    if (match.kind == ZR_LSP_VIRTUAL_DECLARATION_FIELD) {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_FIELD;
        for (TZrSize fieldIndex = 0; fieldIndex < ownerTypeDescriptor->fieldCount; fieldIndex++) {
            const ZrLibFieldDescriptor *fieldDescriptor = &ownerTypeDescriptor->fields[fieldIndex];
            if (fieldDescriptor->name != ZR_NULL && strcmp(fieldDescriptor->name, match.name) == 0) {
                outResolved->fieldDescriptor = fieldDescriptor;
                metadata_provider_set_type_text(provider->state, outResolved, fieldDescriptor->typeName);
                return ZR_TRUE;
            }
        }
    } else {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_METHOD;
        for (TZrSize methodIndex = 0; methodIndex < ownerTypeDescriptor->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *methodDescriptor = &ownerTypeDescriptor->methods[methodIndex];
            if (methodDescriptor->name != ZR_NULL && strcmp(methodDescriptor->name, match.name) == 0) {
                outResolved->methodDescriptor = methodDescriptor;
                metadata_provider_set_type_text(provider->state, outResolved, methodDescriptor->returnTypeName);
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

const TZrChar *ZrLanguageServer_LspMetadataProvider_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind) {
    return ZrLanguageServer_LspModuleMetadata_SourceKindLabel(sourceKind);
}

TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(SZrLspMetadataProvider *provider,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrLspProjectIndex *projectIndex,
                                                                   SZrString *moduleName,
                                                                   SZrString *memberName,
                                                                   SZrLspResolvedMetadataMember *outResolved) {
    SZrIoSource *binarySource = ZR_NULL;

    if (outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outResolved, 0, sizeof(*outResolved));
    outResolved->memberName = memberName;
    outResolved->declarationRange = metadata_provider_module_entry_range(ZR_NULL);
    if (provider == ZR_NULL || provider->state == ZR_NULL || moduleName == ZR_NULL || memberName == ZR_NULL ||
        !ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    moduleName,
                                                                    &outResolved->module)) {
        return ZR_FALSE;
    }

    if (outResolved->module.nativeDescriptor != ZR_NULL) {
        metadata_provider_find_native_member(provider->state,
                                             outResolved->module.nativeDescriptor,
                                             memberName,
                                             outResolved);
    }

    if (outResolved->module.sourceRecord != ZR_NULL && outResolved->module.sourceRecord->uri != ZR_NULL &&
        provider->context != ZR_NULL) {
        metadata_provider_try_get_analyzer_for_uri(provider->state,
                                                   provider->context,
                                                   outResolved->module.sourceRecord->uri,
                                                   &outResolved->declarationAnalyzer);
        if (outResolved->declarationAnalyzer != ZR_NULL) {
            outResolved->declarationSymbol =
                metadata_provider_find_public_global_symbol(outResolved->declarationAnalyzer, memberName);
        }
        if (outResolved->declarationSymbol != ZR_NULL) {
            metadata_provider_resolve_symbol_descriptor(provider->state,
                                                        outResolved->declarationSymbol,
                                                        outResolved);
            outResolved->declarationUri = outResolved->module.sourceRecord->uri;
            outResolved->declarationRange = ZrLanguageServer_Lsp_GetSymbolLookupRange(outResolved->declarationSymbol);
            outResolved->hasDeclaration = ZR_TRUE;
            return ZR_TRUE;
        }
    }

    if (outResolved->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
        ZrLanguageServer_LspMetadataProvider_LoadBinaryModuleSource(provider,
                                                                    projectIndex,
                                                                    moduleName,
                                                                    &binarySource) &&
        binarySource != ZR_NULL) {
        metadata_provider_resolve_binary_member(provider->state, binarySource, memberName, outResolved);
    }

    if (outResolved->resolvedTypeText == ZR_NULL && outResolved->module.modulePrototype != ZR_NULL) {
        metadata_provider_resolve_module_prototype_member(provider->state,
                                                          analyzer,
                                                          outResolved->module.modulePrototype,
                                                          memberName,
                                                          outResolved);
    }

    if (binarySource != ZR_NULL) {
        ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(provider->state->global, binarySource);
    }

    if (outResolved->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
        ZrLanguageServer_LspMetadataProvider_ResolveBinaryExportDeclaration(provider,
                                                                            projectIndex,
                                                                            moduleName,
                                                                            memberName,
                                                                            &outResolved->declarationUri,
                                                                            &outResolved->declarationRange)) {
        outResolved->hasDeclaration = ZR_TRUE;
    } else if (outResolved->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
               ZrLanguageServer_LspMetadataProvider_ResolveBinaryModuleUri(provider,
                                                                           projectIndex,
                                                                           moduleName,
                                                                           &outResolved->declarationUri) &&
               outResolved->declarationUri != ZR_NULL) {
        outResolved->declarationRange = metadata_provider_module_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    } else if (ZrLanguageServer_LspMetadataProvider_ResolveNativeModuleUri(provider,
                                                                           projectIndex,
                                                                           moduleName,
                                                                           &outResolved->declarationUri) &&
               outResolved->declarationUri != ZR_NULL) {
        outResolved->declarationRange = metadata_provider_module_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspMetadataProvider_CreateImportedModuleHover(SZrLspMetadataProvider *provider,
                                                                       const SZrLspResolvedImportedModule *resolvedModule,
                                                                       SZrFileRange range,
                                                                       SZrLspHover **result) {
    const TZrChar *moduleText;
    const TZrChar *sourceKind;
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (provider == ZR_NULL || resolvedModule == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleText = metadata_provider_string_text(resolvedModule->moduleName);
    sourceKind = ZrLanguageServer_LspMetadataProvider_SourceKindLabel(resolvedModule->sourceKind);
    snprintf(buffer,
             sizeof(buffer),
             "module <%s>\n\nSource: %s",
             moduleText != ZR_NULL ? moduleText : "",
             sourceKind != ZR_NULL ? sourceKind : "unresolved");
    return metadata_provider_create_hover(provider->state,
                                          metadata_provider_create_markdown_text(provider->state, buffer),
                                          range,
                                          result);
}

TZrBool ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(SZrLspMetadataProvider *provider,
                                                                       SZrSemanticAnalyzer *analyzer,
                                                                       const SZrLspResolvedMetadataMember *resolvedMember,
                                                                       SZrFileRange range,
                                                                       SZrLspHover **result) {
    const TZrChar *sourceKind;
    const TZrChar *hoverSourceText;
    const TZrChar *memberText;
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    SZrString *content = ZR_NULL;
    SZrString *sourceSection;
    SZrString *hoverUri = ZR_NULL;
    SZrFileVersion *fileVersion = ZR_NULL;
    TZrChar sourceBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    SZrHoverInfo *hoverInfo = ZR_NULL;
    TZrBool contentHasSource = ZR_FALSE;
    SZrSemanticAnalyzer *targetAnalyzer = analyzer;

    if (provider == ZR_NULL || resolvedMember == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memberText = metadata_provider_string_text(resolvedMember->memberName);
    sourceKind = ZrLanguageServer_LspMetadataProvider_SourceKindLabel(resolvedMember->module.sourceKind);
    hoverSourceText = metadata_provider_hover_source_text(resolvedMember,
                                                          sourceKind != ZR_NULL ? sourceKind : "project source");
    if (resolvedMember->declarationUri != ZR_NULL) {
        hoverUri = resolvedMember->declarationUri;
    } else if (resolvedMember->module.sourceRecord != ZR_NULL) {
        hoverUri = resolvedMember->module.sourceRecord->uri;
    }
    if (resolvedMember->declarationAnalyzer != ZR_NULL) {
        targetAnalyzer = resolvedMember->declarationAnalyzer;
    } else if (hoverUri != ZR_NULL && provider->context != ZR_NULL) {
        metadata_provider_try_get_analyzer_for_uri(provider->state, provider->context, hoverUri, &targetAnalyzer);
    }
    if (resolvedMember->declarationSymbol != ZR_NULL &&
        hoverUri != ZR_NULL &&
        provider->context != ZR_NULL) {
        fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(provider->context, hoverUri);
        if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
            metadata_provider_try_get_analyzer_for_uri(provider->state, provider->context, hoverUri, &targetAnalyzer);
            fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(provider->context, hoverUri);
        }

        if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
            if (targetAnalyzer != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(
                    provider->state,
                    targetAnalyzer,
                    ZrLanguageServer_Lsp_GetSymbolLookupRange(resolvedMember->declarationSymbol),
                    &hoverInfo) &&
                hoverInfo != ZR_NULL &&
                hoverInfo->contents != ZR_NULL) {
                content = hoverInfo->contents;
                contentHasSource = ZR_TRUE;
            }

            if (content == ZR_NULL) {
                content = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(provider->state,
                                                                                resolvedMember->declarationSymbol,
                                                                                fileVersion->content,
                                                                                fileVersion->contentLength);
            }

            content = ZrLanguageServer_Lsp_AppendSymbolFfiMetadataMarkdown(provider->state,
                                                                           content,
                                                                           resolvedMember->declarationSymbol);

            if (!contentHasSource) {
                snprintf(sourceBuffer,
                         sizeof(sourceBuffer),
                         "Source: %s",
                         hoverSourceText != ZR_NULL ? hoverSourceText : "project source");
                sourceSection = metadata_provider_create_markdown_text(provider->state, sourceBuffer);
                content = metadata_provider_append_markdown_section(provider->state, content, sourceSection);
            }
            content = metadata_provider_append_markdown_section(provider->state,
                                                                content,
                                                                ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(
                                                                    provider->state,
                                                                    resolvedMember->declarationSymbol,
                                                                    fileVersion->content,
                                                                    fileVersion->contentLength));
        }
    }
    if (hoverInfo != ZR_NULL) {
        ZrLanguageServer_HoverInfo_Free(provider->state, hoverInfo);
    }

    if (content == ZR_NULL) {
        switch (resolvedMember->memberKind) {
            case ZR_LSP_METADATA_MEMBER_MODULE:
                snprintf(buffer,
                         sizeof(buffer),
                         "module <%s>\n\nSource: %s",
                         resolvedMember->moduleLinkDescriptor != ZR_NULL &&
                                  resolvedMember->moduleLinkDescriptor->moduleName != ZR_NULL
                              ? resolvedMember->moduleLinkDescriptor->moduleName
                              : "",
                         hoverSourceText != ZR_NULL ? hoverSourceText : "unresolved");
                break;
            case ZR_LSP_METADATA_MEMBER_CONSTANT:
                snprintf(buffer,
                         sizeof(buffer),
                         "**constant** `%s`\n\nType: %s\n\nSource: %s%s%s",
                         memberText != ZR_NULL ? memberText : "",
                         resolvedMember->resolvedTypeText != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->resolvedTypeText)
                             : metadata_provider_exact_type_failure_text(),
                         hoverSourceText != ZR_NULL ? hoverSourceText : "unresolved",
                         resolvedMember->constantDescriptor != ZR_NULL && resolvedMember->constantDescriptor->documentation != ZR_NULL
                             ? "\n\n"
                             : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                         resolvedMember->typeHintDescriptor->documentation != ZR_NULL
                                     ? "\n\n"
                                    : ""),
                         resolvedMember->constantDescriptor != ZR_NULL && resolvedMember->constantDescriptor->documentation != ZR_NULL
                             ? resolvedMember->constantDescriptor->documentation
                             : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                        resolvedMember->typeHintDescriptor->documentation != ZR_NULL
                                    ? resolvedMember->typeHintDescriptor->documentation
                                    : ""));
                break;
            case ZR_LSP_METADATA_MEMBER_FUNCTION:
                snprintf(buffer,
                         sizeof(buffer),
                         "**function** `%s`\n\nType: %s\n\nSource: %s%s%s",
                         memberText != ZR_NULL ? memberText : "",
                         resolvedMember->resolvedTypeText != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->resolvedTypeText)
                             : metadata_provider_exact_type_failure_text(),
                         hoverSourceText != ZR_NULL ? hoverSourceText : "unresolved",
                         resolvedMember->functionDescriptor != ZR_NULL && resolvedMember->functionDescriptor->documentation != ZR_NULL
                             ? "\n\n"
                             : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                         resolvedMember->typeHintDescriptor->documentation != ZR_NULL
                                     ? "\n\n"
                                    : ""),
                         resolvedMember->functionDescriptor != ZR_NULL && resolvedMember->functionDescriptor->documentation != ZR_NULL
                             ? resolvedMember->functionDescriptor->documentation
                             : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                        resolvedMember->typeHintDescriptor->documentation != ZR_NULL
                                    ? resolvedMember->typeHintDescriptor->documentation
                                    : ""));
                break;
            case ZR_LSP_METADATA_MEMBER_TYPE:
                snprintf(buffer,
                         sizeof(buffer),
                         "**%s** `%s`\n\nType: %s\n\nSource: %s%s%s",
                         resolvedMember->typeDescriptor != ZR_NULL &&
                                 resolvedMember->typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS
                             ? "class"
                             : (resolvedMember->typeDescriptor != ZR_NULL ? "struct"
                                                                           : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                                                                      resolvedMember->typeHintDescriptor->symbolKind != ZR_NULL
                                                                                  ? resolvedMember->typeHintDescriptor->symbolKind
                                                                                  : "type")),
                         memberText != ZR_NULL ? memberText : "",
                         resolvedMember->resolvedTypeText != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->resolvedTypeText)
                             : metadata_provider_exact_type_failure_text(),
                         hoverSourceText != ZR_NULL ? hoverSourceText : "unresolved",
                         resolvedMember->typeDescriptor != ZR_NULL && resolvedMember->typeDescriptor->documentation != ZR_NULL
                             ? "\n\n"
                             : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                         resolvedMember->typeHintDescriptor->documentation != ZR_NULL
                                     ? "\n\n"
                                    : ""),
                         resolvedMember->typeDescriptor != ZR_NULL && resolvedMember->typeDescriptor->documentation != ZR_NULL
                             ? resolvedMember->typeDescriptor->documentation
                             : (resolvedMember->typeHintDescriptor != ZR_NULL &&
                                        resolvedMember->typeHintDescriptor->documentation != ZR_NULL
                                    ? resolvedMember->typeHintDescriptor->documentation
                                    : ""));
                break;
            case ZR_LSP_METADATA_MEMBER_FIELD:
                snprintf(buffer,
                         sizeof(buffer),
                         "**field** `%s`\n\nType: %s\n\nReceiver: %s\n\nSource: %s%s%s",
                         memberText != ZR_NULL ? memberText : "",
                         resolvedMember->resolvedTypeText != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->resolvedTypeText)
                             : metadata_provider_exact_type_failure_text(),
                         resolvedMember->ownerTypeName != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->ownerTypeName)
                             : metadata_provider_exact_type_failure_text(),
                         hoverSourceText != ZR_NULL ? hoverSourceText : "unresolved",
                         resolvedMember->fieldDescriptor != ZR_NULL && resolvedMember->fieldDescriptor->documentation != ZR_NULL
                             ? "\n\n"
                             : "",
                         resolvedMember->fieldDescriptor != ZR_NULL && resolvedMember->fieldDescriptor->documentation != ZR_NULL
                             ? resolvedMember->fieldDescriptor->documentation
                             : "");
                break;
            case ZR_LSP_METADATA_MEMBER_METHOD: {
                TZrChar parameterBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
                TZrSize parameterUsed = 0;
                const TZrChar *documentation = "";
                parameterBuffer[0] = '\0';

                if (resolvedMember->methodDescriptor != ZR_NULL) {
                    for (TZrSize index = 0; index < resolvedMember->methodDescriptor->parameterCount; index++) {
                        const ZrLibParameterDescriptor *parameter = &resolvedMember->methodDescriptor->parameters[index];
                        TZrInt32 written;

                        written = snprintf(parameterBuffer + parameterUsed,
                                           sizeof(parameterBuffer) - parameterUsed,
                                           "%s%s%s%s",
                                           index > 0 ? ", " : "",
                                           parameter->name != ZR_NULL ? parameter->name : "",
                                           parameter->name != ZR_NULL ? ": " : "",
                                           metadata_provider_type_text_or_failure(parameter->typeName));
                        if (written <= 0 || (TZrSize)written >= sizeof(parameterBuffer) - parameterUsed) {
                            break;
                        }
                        parameterUsed += (TZrSize)written;
                    }
                    documentation = resolvedMember->methodDescriptor->documentation != ZR_NULL
                                        ? resolvedMember->methodDescriptor->documentation
                                        : "";
                }

                snprintf(buffer,
                         sizeof(buffer),
                         "**method** `%s`\n\nSignature: %s(%s): %s\n\nReceiver: %s\n\nSource: %s%s%s",
                         memberText != ZR_NULL ? memberText : "",
                         resolvedMember->methodDescriptor != ZR_NULL && resolvedMember->methodDescriptor->name != ZR_NULL
                             ? resolvedMember->methodDescriptor->name
                             : (memberText != ZR_NULL ? memberText : ""),
                         parameterBuffer,
                         resolvedMember->resolvedTypeText != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->resolvedTypeText)
                             : metadata_provider_exact_type_failure_text(),
                         resolvedMember->ownerTypeName != ZR_NULL
                             ? metadata_provider_string_text(resolvedMember->ownerTypeName)
                             : metadata_provider_exact_type_failure_text(),
                         hoverSourceText != ZR_NULL ? hoverSourceText : "unresolved",
                         documentation[0] != '\0' ? "\n\n" : "",
                         documentation);
                break;
            }
            default:
                return ZR_FALSE;
        }

        content = metadata_provider_create_markdown_text(provider->state, buffer);
    }

    return metadata_provider_create_hover(provider->state,
                                          content,
                                          resolvedMember->hasDeclaration ? resolvedMember->declarationRange : range,
                                          result);
}

TZrBool ZrLanguageServer_LspMetadataProvider_AppendImportedModuleCompletions(
    SZrLspMetadataProvider *provider,
    SZrSemanticAnalyzer *analyzer,
    const SZrLspResolvedImportedModule *resolvedModule,
    SZrArray *result) {
    SZrIoSource *binarySource = ZR_NULL;

    if (provider == ZR_NULL || resolvedModule == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (resolvedModule->sourceRecord != ZR_NULL && resolvedModule->sourceRecord->uri != ZR_NULL &&
        provider->context != ZR_NULL) {
        SZrSemanticAnalyzer *targetAnalyzer = ZR_NULL;

        metadata_provider_try_get_analyzer_for_uri(provider->state,
                                                   provider->context,
                                                   resolvedModule->sourceRecord->uri,
                                                   &targetAnalyzer);
        if (targetAnalyzer != ZR_NULL &&
            targetAnalyzer->symbolTable != ZR_NULL &&
            targetAnalyzer->symbolTable->globalScope != ZR_NULL) {
            for (TZrSize index = 0; index < targetAnalyzer->symbolTable->globalScope->symbols.length; index++) {
                SZrSymbol **symbolPtr =
                    (SZrSymbol **)ZrCore_Array_Get(&targetAnalyzer->symbolTable->globalScope->symbols, index);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                    (*symbolPtr)->accessModifier == ZR_ACCESS_PUBLIC) {
                    metadata_provider_append_project_symbol_completion(provider->state, *symbolPtr, result);
                }
            }
        }
        return ZR_TRUE;
    }

    if (resolvedModule->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
        ZrLanguageServer_LspMetadataProvider_LoadBinaryModuleSource(provider,
                                                                    resolvedModule->projectIndex,
                                                                    resolvedModule->moduleName,
                                                                    &binarySource) &&
        binarySource != ZR_NULL &&
        binarySource->modulesLength > 0 &&
        binarySource->modules != ZR_NULL &&
        binarySource->modules[0].entryFunction != ZR_NULL) {
        metadata_provider_append_binary_module_completions(provider->state, binarySource->modules[0].entryFunction, result);
    }

    if (resolvedModule->nativeDescriptor != ZR_NULL) {
        metadata_provider_append_native_module_completions(provider->state, resolvedModule->nativeDescriptor, result);
    }

    if (resolvedModule->modulePrototype != ZR_NULL) {
        metadata_provider_append_module_prototype_completions(provider->state,
                                                              analyzer,
                                                              resolvedModule->modulePrototype,
                                                              result,
                                                              result->length > 0);
    }

    if (binarySource != ZR_NULL) {
        ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(provider->state->global, binarySource);
    }
    return result->length > 0;
}
