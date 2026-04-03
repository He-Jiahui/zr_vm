#include "lsp_project_internal.h"

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_library/native_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum EZrLspNativeMemberKind {
    ZR_LSP_NATIVE_MEMBER_KIND_NONE = 0,
    ZR_LSP_NATIVE_MEMBER_KIND_MODULE = 1,
    ZR_LSP_NATIVE_MEMBER_KIND_FUNCTION = 2,
    ZR_LSP_NATIVE_MEMBER_KIND_TYPE = 3,
    ZR_LSP_NATIVE_MEMBER_KIND_HINT = 4
} EZrLspNativeMemberKind;

typedef struct SZrLspNativeMemberResolution {
    EZrLspNativeMemberKind kind;
    const ZrLibModuleLinkDescriptor *moduleLink;
    const ZrLibFunctionDescriptor *functionDescriptor;
    const ZrLibTypeDescriptor *typeDescriptor;
    const ZrLibTypeHintDescriptor *typeHintDescriptor;
} SZrLspNativeMemberResolution;

static void project_feature_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static const TZrChar *project_feature_get_string_text(SZrString *value) {
    TZrNativeString text;
    TZrSize length;

    project_feature_get_string_view(value, &text, &length);
    return text;
}

static const TZrChar *project_symbol_kind_text(EZrSymbolType type) {
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

static SZrSymbol *find_public_global_symbol(SZrSemanticAnalyzer *analyzer, SZrString *name) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || analyzer->symbolTable->globalScope == ZR_NULL ||
        name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->symbolTable->globalScope->symbols.length; index++) {
        SZrSymbol **symbolPtr =
            (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, index);
        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL) {
            continue;
        }

        if ((*symbolPtr)->accessModifier == ZR_ACCESS_PUBLIC &&
            ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->name, name)) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool create_hover_from_markdown(SZrState *state,
                                          const TZrChar *markdown,
                                          SZrFileRange range,
                                          SZrLspHover **result) {
    SZrLspHover *hover;
    SZrString *content;

    if (state == ZR_NULL || markdown == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    content = ZrCore_String_Create(state, (TZrNativeString)markdown, strlen(markdown));
    if (hover == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_LspRange_FromFileRange(range);
    *result = hover;
    return ZR_TRUE;
}

static TZrBool create_hover_for_symbol(SZrState *state,
                                       SZrLspContext *context,
                                       SZrSymbol *symbol,
                                       SZrFileRange currentRange,
                                       SZrLspHover **result) {
    SZrFileVersion *targetFileVersion;
    SZrString *content;
    const TZrChar *nativeContent = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || symbol == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    targetFileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, symbol->location.source);
    if (targetFileVersion != ZR_NULL) {
        nativeContent = targetFileVersion->content;
    }

    content = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(state,
                                                                    symbol,
                                                                    nativeContent != ZR_NULL ? nativeContent : "",
                                                                    nativeContent != ZR_NULL ? strlen(nativeContent) : 0);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    return create_hover_from_markdown(state, project_feature_get_string_text(content), currentRange, result);
}

static void append_project_symbol_completion(SZrState *state,
                                             SZrSymbol *symbol,
                                             SZrArray *result) {
    TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText = "object";
    SZrCompletionItem *item;

    if (state == ZR_NULL || symbol == ZR_NULL || result == ZR_NULL || symbol->name == ZR_NULL) {
        return;
    }

    if (symbol->typeInfo != ZR_NULL) {
        typeText = ZrParser_TypeNameString_Get(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer));
    }

    snprintf(detail,
             sizeof(detail),
             "%s %s",
             project_symbol_kind_text(symbol->type),
             typeText != ZR_NULL ? typeText : "object");
    item = ZrLanguageServer_CompletionItem_New(state,
                                               project_feature_get_string_text(symbol->name),
                                               project_symbol_kind_text(symbol->type),
                                               detail,
                                               ZR_NULL,
                                               symbol->typeInfo);
    if (item != ZR_NULL) {
        ZrCore_Array_Push(state, result, &item);
    }
}

static const ZrLibModuleDescriptor *resolve_native_module_descriptor(SZrState *state, SZrString *moduleName) {
    const TZrChar *moduleText;

    if (state == ZR_NULL || moduleName == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    moduleText = project_feature_get_string_text(moduleName);
    return moduleText != ZR_NULL ? ZrLibrary_NativeRegistry_FindModule(state->global, moduleText) : ZR_NULL;
}

static void append_native_module_completions(SZrState *state,
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
                 functionDescriptor->returnTypeName != ZR_NULL ? functionDescriptor->returnTypeName : "object");
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

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *hint = &descriptor->typeHints[index];
        SZrCompletionItem *item;

        if (hint->symbolName == ZR_NULL) {
            continue;
        }

        item = ZrLanguageServer_CompletionItem_New(state,
                                                   hint->symbolName,
                                                   hint->symbolKind != ZR_NULL ? hint->symbolKind : "symbol",
                                                   hint->signature,
                                                   hint->documentation,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static TZrBool resolve_native_member_descriptor(const ZrLibModuleDescriptor *descriptor,
                                                SZrString *memberName,
                                                SZrLspNativeMemberResolution *outResolution) {
    const TZrChar *memberText = project_feature_get_string_text(memberName);

    if (descriptor == ZR_NULL || memberText == ZR_NULL || outResolution == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outResolution, 0, sizeof(*outResolution));
    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *link = &descriptor->moduleLinks[index];
        if (link->name != ZR_NULL && strcmp(link->name, memberText) == 0) {
            outResolution->kind = ZR_LSP_NATIVE_MEMBER_KIND_MODULE;
            outResolution->moduleLink = link;
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        if (functionDescriptor->name != ZR_NULL && strcmp(functionDescriptor->name, memberText) == 0) {
            outResolution->kind = ZR_LSP_NATIVE_MEMBER_KIND_FUNCTION;
            outResolution->functionDescriptor = functionDescriptor;
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, memberText) == 0) {
            outResolution->kind = ZR_LSP_NATIVE_MEMBER_KIND_TYPE;
            outResolution->typeDescriptor = typeDescriptor;
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *typeHint = &descriptor->typeHints[index];
        if (typeHint->symbolName != ZR_NULL && strcmp(typeHint->symbolName, memberText) == 0) {
            outResolution->kind = ZR_LSP_NATIVE_MEMBER_KIND_HINT;
            outResolution->typeHintDescriptor = typeHint;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool create_native_member_hover(SZrState *state,
                                          const ZrLibModuleDescriptor *descriptor,
                                          SZrString *memberName,
                                          SZrFileRange range,
                                          SZrLspHover **result) {
    SZrLspNativeMemberResolution resolution;
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    const TZrChar *kindText = "symbol";
    const TZrChar *detailText = "object";
    const TZrChar *documentation = ZR_NULL;

    if (state == ZR_NULL || descriptor == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL ||
        !resolve_native_member_descriptor(descriptor, memberName, &resolution)) {
        return ZR_FALSE;
    }

    switch (resolution.kind) {
        case ZR_LSP_NATIVE_MEMBER_KIND_MODULE:
            snprintf(buffer,
                     sizeof(buffer),
                     "module <%s>",
                     resolution.moduleLink->moduleName != ZR_NULL ? resolution.moduleLink->moduleName : "");
            return create_hover_from_markdown(state, buffer, range, result);

        case ZR_LSP_NATIVE_MEMBER_KIND_FUNCTION:
            kindText = "function";
            detailText = resolution.functionDescriptor->returnTypeName != ZR_NULL
                         ? resolution.functionDescriptor->returnTypeName
                         : "object";
            documentation = resolution.functionDescriptor->documentation;
            break;

        case ZR_LSP_NATIVE_MEMBER_KIND_TYPE:
            kindText = resolution.typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS ? "class" : "struct";
            detailText = resolution.typeDescriptor->name != ZR_NULL ? resolution.typeDescriptor->name : "object";
            documentation = resolution.typeDescriptor->documentation;
            break;

        case ZR_LSP_NATIVE_MEMBER_KIND_HINT:
            kindText = resolution.typeHintDescriptor->symbolKind != ZR_NULL
                       ? resolution.typeHintDescriptor->symbolKind
                       : "symbol";
            detailText = resolution.typeHintDescriptor->signature != ZR_NULL
                         ? resolution.typeHintDescriptor->signature
                         : "object";
            documentation = resolution.typeHintDescriptor->documentation;
            break;

        default:
            return ZR_FALSE;
    }

    snprintf(buffer,
             sizeof(buffer),
             "**%s** `%s`\n\nType: %s%s%s",
             kindText,
             project_feature_get_string_text(memberName),
             detailText,
             documentation != ZR_NULL ? "\n\n" : "",
             documentation != ZR_NULL ? documentation : "");
    return create_hover_from_markdown(state, buffer, range, result);
}

static TZrBool extract_receiver_alias_before_dot(SZrState *state,
                                                 const TZrChar *content,
                                                 TZrSize contentLength,
                                                 TZrSize cursorOffset,
                                                 SZrString **outAlias) {
    TZrSize receiverEnd;
    TZrSize receiverStart;

    if (state == ZR_NULL || content == ZR_NULL || outAlias == ZR_NULL ||
        cursorOffset == 0 || cursorOffset > contentLength || content[cursorOffset - 1] != '.') {
        return ZR_FALSE;
    }

    receiverEnd = cursorOffset - 1;
    receiverStart = receiverEnd;
    while (receiverStart > 0) {
        TZrChar ch = content[receiverStart - 1];
        if (!isalnum((unsigned char)ch) && ch != '_') {
            break;
        }
        receiverStart--;
    }

    if (receiverStart == receiverEnd) {
        return ZR_FALSE;
    }

    *outAlias = ZrCore_String_Create(state,
                                     (TZrNativeString)(content + receiverStart),
                                     receiverEnd - receiverStart);
    return *outAlias != ZR_NULL;
}

static TZrBool project_feature_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source)) {
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

static SZrLspImportBinding *find_import_binding_for_completion_position(SZrAstNode *node,
                                                                        SZrArray *bindings,
                                                                        SZrFileRange cursorRange) {
    if (node == ZR_NULL || bindings == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    SZrLspImportBinding *binding =
                        find_import_binding_for_completion_position(node->data.script.statements->nodes[index],
                                                                    bindings,
                                                                    cursorRange);
                    if (binding != ZR_NULL) {
                        return binding;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    SZrLspImportBinding *binding =
                        find_import_binding_for_completion_position(node->data.block.body->nodes[index],
                                                                    bindings,
                                                                    cursorRange);
                    if (binding != ZR_NULL) {
                        return binding;
                    }
                }
            }
            break;

        case ZR_AST_VARIABLE_DECLARATION:
            if (node->data.variableDeclaration.pattern != ZR_NULL &&
                node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
                node->data.variableDeclaration.value != ZR_NULL &&
                node->data.variableDeclaration.value->type == ZR_AST_IMPORT_EXPRESSION &&
                (project_feature_file_range_contains_position(node->location, cursorRange) ||
                 project_feature_file_range_contains_position(node->data.variableDeclaration.value->location,
                                                              cursorRange) ||
                 node->data.variableDeclaration.pattern->location.start.line == cursorRange.start.line)) {
                return ZrLanguageServer_LspProject_FindImportBindingByAlias(
                    bindings,
                    node->data.variableDeclaration.pattern->data.identifier.name);
            }
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            return find_import_binding_for_completion_position(node->data.functionDeclaration.body,
                                                               bindings,
                                                               cursorRange);

        case ZR_AST_TEST_DECLARATION:
            return find_import_binding_for_completion_position(node->data.testDeclaration.body,
                                                               bindings,
                                                               cursorRange);

        default:
            break;
    }

    return ZR_NULL;
}

TZrBool ZrLanguageServer_Lsp_ProjectTryCollectImportCompletions(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                const TZrChar *content,
                                                                TZrSize contentLength,
                                                                TZrSize cursorOffset,
                                                                SZrFileRange cursorRange,
                                                                SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrArray bindings;
    SZrString *aliasName = ZR_NULL;
    SZrLspImportBinding *binding;
    SZrLspProjectIndex *projectIndex;
    TZrSize startLength;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    startLength = result->length;
    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (extract_receiver_alias_before_dot(state, content, contentLength, cursorOffset, &aliasName)) {
        binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(&bindings, aliasName);
    } else {
        binding = find_import_binding_for_completion_position(analyzer->ast, &bindings, cursorRange);
    }
    if (binding != ZR_NULL) {
        SZrLspProjectFileRecord *record =
            projectIndex != ZR_NULL ? ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, binding->moduleName)
                                    : ZR_NULL;
        if (record != ZR_NULL) {
            SZrSemanticAnalyzer *targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
            if (targetAnalyzer == ZR_NULL) {
                targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, record->uri);
            }
            if (targetAnalyzer != ZR_NULL &&
                targetAnalyzer->symbolTable != ZR_NULL &&
                targetAnalyzer->symbolTable->globalScope != ZR_NULL) {
                for (TZrSize index = 0; index < targetAnalyzer->symbolTable->globalScope->symbols.length; index++) {
                    SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&targetAnalyzer->symbolTable->globalScope->symbols,
                                                                           index);
                    if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                        (*symbolPtr)->accessModifier == ZR_ACCESS_PUBLIC) {
                        append_project_symbol_completion(state, *symbolPtr, result);
                    }
                }
            }
        } else {
            append_native_module_completions(state, resolve_native_module_descriptor(state, binding->moduleName), result);
        }
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return result->length > startLength;
}

TZrBool ZrLanguageServer_Lsp_ProjectTryGetHover(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrLspPosition position,
                                                SZrLspHover **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrArray bindings;
    SZrLspImportedMemberHit memberHit;
    SZrLspImportBinding *binding = ZR_NULL;
    SZrFileRange bindingRange;
    SZrLspProjectIndex *projectIndex;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);

    if (ZrLanguageServer_LspProject_FindImportedMemberHit(analyzer->ast, &bindings, fileRange, &memberHit)) {
        SZrLspProjectFileRecord *record =
            projectIndex != ZR_NULL ? ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, memberHit.moduleName)
                                    : ZR_NULL;
        if (record != ZR_NULL) {
            SZrSemanticAnalyzer *targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
            if (targetAnalyzer == ZR_NULL) {
                targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, record->uri);
            }
            SZrSymbol *targetSymbol = find_public_global_symbol(targetAnalyzer, memberHit.memberName);
            if (targetSymbol != ZR_NULL) {
                ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
                return create_hover_for_symbol(state, context, targetSymbol, memberHit.location, result);
            }
        } else {
            const ZrLibModuleDescriptor *descriptor = resolve_native_module_descriptor(state, memberHit.moduleName);
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return create_native_member_hover(state, descriptor, memberHit.memberName, memberHit.location, result);
        }
    }

    if (ZrLanguageServer_LspProject_FindImportBindingHit(analyzer->ast, &bindings, fileRange, &binding, &bindingRange) &&
        binding != ZR_NULL) {
        TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
        snprintf(buffer,
                 sizeof(buffer),
                 "module <%s>",
                 project_feature_get_string_text(binding->moduleName) != ZR_NULL
                     ? project_feature_get_string_text(binding->moduleName)
                     : "");
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return create_hover_from_markdown(state, buffer, bindingRange, result);
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return ZR_FALSE;
}
