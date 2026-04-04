#include "lsp_module_metadata.h"
#include "lsp_project_internal.h"

#include "zr_vm_parser/type_inference.h"

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

static const TZrChar *module_prototype_member_kind_text(const SZrTypeMemberInfo *member) {
    if (member == ZR_NULL) {
        return ZR_NULL;
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

static const SZrTypeMemberInfo *find_module_prototype_member(const SZrTypePrototypeInfo *modulePrototype,
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

static void append_module_prototype_completions(SZrState *state,
                                                const SZrTypePrototypeInfo *modulePrototype,
                                                SZrArray *result) {
    if (state == ZR_NULL || modulePrototype == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < modulePrototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&modulePrototype->members, index);
        const TZrChar *kind = module_prototype_member_kind_text(member);
        const TZrChar *detailType = "object";
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (member == ZR_NULL || member->name == ZR_NULL || member->isMetaMethod || kind == ZR_NULL) {
            continue;
        }

        if (strcmp(kind, "function") == 0) {
            detailType = project_feature_get_string_text(member->returnTypeName);
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     project_feature_get_string_text(member->name),
                     detailType != ZR_NULL ? detailType : "object");
        } else {
            detailType = project_feature_get_string_text(member->fieldTypeName);
            snprintf(detail,
                     sizeof(detail),
                     "%s %s",
                     kind,
                     detailType != ZR_NULL ? detailType : "object");
        }

        item = ZrLanguageServer_CompletionItem_New(state,
                                                   project_feature_get_string_text(member->name),
                                                   kind,
                                                   detail,
                                                   ZR_NULL,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static const TZrChar *binary_type_ref_text(const SZrIoFunctionTypedTypeRef *typeRef,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    const TZrChar *baseName = "object";

    if (buffer == ZR_NULL || bufferSize == 0) {
        return "object";
    }

    buffer[0] = '\0';
    if (typeRef == ZR_NULL) {
        return "object";
    }

    if (typeRef->isArray) {
        const TZrChar *elementName = typeRef->elementTypeName != ZR_NULL
                                         ? project_feature_get_string_text(typeRef->elementTypeName)
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
                    elementName = "object";
                    break;
            }
        }
        snprintf(buffer, bufferSize, "%s[]", elementName);
        return buffer;
    }

    if (typeRef->typeName != ZR_NULL) {
        const TZrChar *typeName = project_feature_get_string_text(typeRef->typeName);
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
            baseName = "object";
            break;
    }

    snprintf(buffer, bufferSize, "%s", baseName);
    return buffer;
}

static const TZrChar *compiled_type_ref_text(const SZrFunctionTypedTypeRef *typeRef,
                                             TZrChar *buffer,
                                             TZrSize bufferSize) {
    const TZrChar *baseName = "object";

    if (buffer == ZR_NULL || bufferSize == 0) {
        return "object";
    }

    buffer[0] = '\0';
    if (typeRef == ZR_NULL) {
        return "object";
    }

    if (typeRef->isArray) {
        const TZrChar *elementName = typeRef->elementTypeName != ZR_NULL
                                         ? project_feature_get_string_text(typeRef->elementTypeName)
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
                    elementName = "object";
                    break;
            }
        }
        snprintf(buffer, bufferSize, "%s[]", elementName);
        return buffer;
    }

    if (typeRef->typeName != ZR_NULL) {
        const TZrChar *typeName = project_feature_get_string_text(typeRef->typeName);
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
            baseName = "object";
            break;
    }

    snprintf(buffer, bufferSize, "%s", baseName);
    return buffer;
}

static TZrBool binary_export_symbol_is_callable(const SZrIoFunctionTypedExportSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return ZR_TRUE;
    }

    return symbol->valueType.baseType == ZR_VALUE_TYPE_FUNCTION ||
           symbol->valueType.baseType == ZR_VALUE_TYPE_CLOSURE;
}

static void append_binary_module_completions(SZrState *state,
                                             const SZrIoFunction *entryFunction,
                                             SZrArray *result) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        const TZrChar *kind = binary_export_symbol_is_callable(symbol) ? "function" : "field";
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (symbol->name == ZR_NULL) {
            continue;
        }

        if (binary_export_symbol_is_callable(symbol)) {
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     project_feature_get_string_text(symbol->name),
                     binary_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)));
        } else {
            snprintf(detail,
                     sizeof(detail),
                     "field %s",
                     binary_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)));
        }

        item = ZrLanguageServer_CompletionItem_New(state,
                                                   project_feature_get_string_text(symbol->name),
                                                   kind,
                                                   detail,
                                                   ZR_NULL,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < entryFunction->classesLength; index++) {
        const SZrIoClass *classInfo = &entryFunction->classes[index];
        const TZrChar *name = classInfo->name != ZR_NULL ? project_feature_get_string_text(classInfo->name) : ZR_NULL;
        SZrCompletionItem *item;

        if (name == ZR_NULL || name[0] == '\0') {
            continue;
        }

        item = ZrLanguageServer_CompletionItem_New(state, name, "class", name, ZR_NULL, ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }

    for (TZrSize index = 0; index < entryFunction->structsLength; index++) {
        const SZrIoStruct *structInfo = &entryFunction->structs[index];
        const TZrChar *name = structInfo->name != ZR_NULL ? project_feature_get_string_text(structInfo->name) : ZR_NULL;
        SZrCompletionItem *item;

        if (name == ZR_NULL || name[0] == '\0') {
            continue;
        }

        item = ZrLanguageServer_CompletionItem_New(state, name, "struct", name, ZR_NULL, ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static TZrBool compiled_export_symbol_is_callable(const SZrFunctionTypedExportSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return ZR_TRUE;
    }

    return symbol->valueType.baseType == ZR_VALUE_TYPE_FUNCTION ||
           symbol->valueType.baseType == ZR_VALUE_TYPE_CLOSURE;
}

static void append_compiled_module_completions(SZrState *state,
                                               const SZrFunction *entryFunction,
                                               SZrArray *result) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        const TZrChar *kind = compiled_export_symbol_is_callable(symbol) ? "function" : "field";
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (symbol->name == ZR_NULL) {
            continue;
        }

        if (compiled_export_symbol_is_callable(symbol)) {
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     project_feature_get_string_text(symbol->name),
                     compiled_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)));
        } else {
            snprintf(detail,
                     sizeof(detail),
                     "field %s",
                     compiled_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)));
        }

        item = ZrLanguageServer_CompletionItem_New(state,
                                                   project_feature_get_string_text(symbol->name),
                                                   kind,
                                                   detail,
                                                   ZR_NULL,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static void append_intermediate_module_completions(SZrState *state,
                                                   const SZrLspIntermediateModuleMetadata *metadata,
                                                   SZrArray *result) {
    if (state == ZR_NULL || metadata == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < metadata->exportedSymbols.length; index++) {
        const SZrLspIntermediateExportSymbol *symbol =
            (const SZrLspIntermediateExportSymbol *)ZrCore_Array_Get((SZrArray *)&metadata->exportedSymbols, index);
        const TZrChar *kind = (symbol != ZR_NULL && symbol->isCallable) ? "function" : "field";
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];
        SZrCompletionItem *item;

        if (symbol == ZR_NULL || symbol->name == ZR_NULL || symbol->typeName == ZR_NULL) {
            continue;
        }

        if (symbol->isCallable) {
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     project_feature_get_string_text(symbol->name),
                     project_feature_get_string_text(symbol->typeName));
        } else {
            snprintf(detail,
                     sizeof(detail),
                     "field %s",
                     project_feature_get_string_text(symbol->typeName));
        }

        item = ZrLanguageServer_CompletionItem_New(state,
                                                   project_feature_get_string_text(symbol->name),
                                                   kind,
                                                   detail,
                                                   ZR_NULL,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

static TZrBool create_binary_module_member_hover(SZrState *state,
                                                 const SZrIoFunction *entryFunction,
                                                 SZrString *memberName,
                                                 const TZrChar *sourceKind,
                                                 SZrFileRange range,
                                                 SZrLspHover **result) {
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (state == ZR_NULL || entryFunction == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        const TZrChar *name = symbol->name != ZR_NULL ? project_feature_get_string_text(symbol->name) : ZR_NULL;
        const TZrChar *kindText = binary_export_symbol_is_callable(symbol) ? "function" : "field";

        if (name == ZR_NULL || strcmp(name, project_feature_get_string_text(memberName)) != 0) {
            continue;
        }

        snprintf(buffer,
                 sizeof(buffer),
                 "**%s** `%s`\n\nType: %s%s%s",
                 kindText,
                 name,
                 binary_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)),
                 sourceKind != ZR_NULL ? "\n\nSource: " : "",
                 sourceKind != ZR_NULL ? sourceKind : "");
        return create_hover_from_markdown(state, buffer, range, result);
    }

    return ZR_FALSE;
}

static TZrBool create_compiled_module_member_hover(SZrState *state,
                                                   const SZrFunction *entryFunction,
                                                   SZrString *memberName,
                                                   const TZrChar *sourceKind,
                                                   SZrFileRange range,
                                                   SZrLspHover **result) {
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (state == ZR_NULL || entryFunction == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        const TZrChar *name = symbol->name != ZR_NULL ? project_feature_get_string_text(symbol->name) : ZR_NULL;
        const TZrChar *kindText = compiled_export_symbol_is_callable(symbol) ? "function" : "field";

        if (name == ZR_NULL || strcmp(name, project_feature_get_string_text(memberName)) != 0) {
            continue;
        }

        snprintf(buffer,
                 sizeof(buffer),
                 "**%s** `%s`\n\nType: %s%s%s",
                 kindText,
                 name,
                 compiled_type_ref_text(&symbol->valueType, typeBuffer, sizeof(typeBuffer)),
                 sourceKind != ZR_NULL ? "\n\nSource: " : "",
                 sourceKind != ZR_NULL ? sourceKind : "");
        return create_hover_from_markdown(state, buffer, range, result);
    }

    return ZR_FALSE;
}

static TZrBool create_intermediate_module_member_hover(SZrState *state,
                                                       const SZrLspIntermediateModuleMetadata *metadata,
                                                       SZrString *memberName,
                                                       const TZrChar *sourceKind,
                                                       SZrFileRange range,
                                                       SZrLspHover **result) {
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || metadata == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < metadata->exportedSymbols.length; index++) {
        const SZrLspIntermediateExportSymbol *symbol =
            (const SZrLspIntermediateExportSymbol *)ZrCore_Array_Get((SZrArray *)&metadata->exportedSymbols, index);
        const TZrChar *name = symbol != ZR_NULL && symbol->name != ZR_NULL
                                  ? project_feature_get_string_text(symbol->name)
                                  : ZR_NULL;
        const TZrChar *kindText = symbol != ZR_NULL && symbol->isCallable ? "function" : "field";

        if (symbol == ZR_NULL || name == ZR_NULL || symbol->typeName == ZR_NULL ||
            strcmp(name, project_feature_get_string_text(memberName)) != 0) {
            continue;
        }

        snprintf(buffer,
                 sizeof(buffer),
                 "**%s** `%s`\n\nType: %s%s%s",
                 kindText,
                 name,
                 project_feature_get_string_text(symbol->typeName),
                 sourceKind != ZR_NULL ? "\n\nSource: " : "",
                 sourceKind != ZR_NULL ? sourceKind : "");
        return create_hover_from_markdown(state, buffer, range, result);
    }

    return ZR_FALSE;
}

static TZrBool create_module_prototype_member_hover(SZrState *state,
                                                    const SZrTypePrototypeInfo *modulePrototype,
                                                    SZrString *memberName,
                                                    const TZrChar *sourceKind,
                                                    SZrFileRange range,
                                                    SZrLspHover **result) {
    const SZrTypeMemberInfo *member;
    const TZrChar *kindText;
    const TZrChar *detailText = "object";
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || modulePrototype == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    member = find_module_prototype_member(modulePrototype, memberName);
    kindText = module_prototype_member_kind_text(member);
    if (member == ZR_NULL || kindText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(kindText, "function") == 0) {
        detailText = project_feature_get_string_text(member->returnTypeName);
    } else {
        detailText = project_feature_get_string_text(member->fieldTypeName);
    }

    snprintf(buffer,
             sizeof(buffer),
             "**%s** `%s`\n\nType: %s%s%s",
             kindText,
             project_feature_get_string_text(memberName),
             detailText != ZR_NULL ? detailText : "object",
             sourceKind != ZR_NULL ? "\n\nSource: " : "",
             sourceKind != ZR_NULL ? sourceKind : "");
    return create_hover_from_markdown(state, buffer, range, result);
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
                                          const TZrChar *sourceKind,
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
                     "module <%s>%s%s",
                     resolution.moduleLink->moduleName != ZR_NULL ? resolution.moduleLink->moduleName : "",
                     sourceKind != ZR_NULL ? "\n\nSource: " : "",
                     sourceKind != ZR_NULL ? sourceKind : "");
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
             "**%s** `%s`\n\nType: %s%s%s%s%s",
             kindText,
             project_feature_get_string_text(memberName),
             detailText,
             sourceKind != ZR_NULL ? "\n\nSource: " : "",
             sourceKind != ZR_NULL ? sourceKind : "",
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
        SZrLspResolvedImportedModule resolved;

        ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                 analyzer,
                                                                 projectIndex,
                                                                 binding->moduleName,
                                                                 &resolved);
        if (resolved.sourceRecord != ZR_NULL) {
            SZrSemanticAnalyzer *targetAnalyzer =
                ZrLanguageServer_Lsp_FindAnalyzer(state, context, resolved.sourceRecord->uri);
            if (targetAnalyzer == ZR_NULL) {
                targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, resolved.sourceRecord->uri);
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
        } else if (resolved.modulePrototype != ZR_NULL) {
            append_module_prototype_completions(state, resolved.modulePrototype, result);
        } else if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
            SZrIoSource *binarySource = ZR_NULL;
            SZrLspIntermediateModuleMetadata intermediateMetadata;
            SZrFunction *intermediateFunction = ZR_NULL;
            ZrCore_Array_Construct(&intermediateMetadata.exportedSymbols);

            if (ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(state,
                                                                          projectIndex,
                                                                          binding->moduleName,
                                                                          &binarySource) &&
                binarySource != ZR_NULL &&
                binarySource->modulesLength > 0 &&
                binarySource->modules != ZR_NULL &&
                binarySource->modules[0].entryFunction != ZR_NULL) {
                append_binary_module_completions(state, binarySource->modules[0].entryFunction, result);
            } else if (ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleMetadata(state,
                                                                                         projectIndex,
                                                                                         binding->moduleName,
                                                                                         &intermediateMetadata)) {
                append_intermediate_module_completions(state, &intermediateMetadata, result);
            } else if (ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleFunction(state,
                                                                                         projectIndex,
                                                                                         binding->moduleName,
                                                                                         &intermediateFunction) &&
                       intermediateFunction != ZR_NULL) {
                append_compiled_module_completions(state, intermediateFunction, result);
            }
            if (binarySource != ZR_NULL) {
                ZrCore_Io_ReadSourceFree(state->global, binarySource);
            }
            ZrLanguageServer_LspModuleMetadata_FreeIntermediateModuleMetadata(state, &intermediateMetadata);
            if (intermediateFunction != ZR_NULL) {
                ZrCore_Function_Free(state, intermediateFunction);
            }
        } else {
            append_native_module_completions(state, resolved.nativeDescriptor, result);
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
        SZrLspResolvedImportedModule resolved;
        const TZrChar *sourceKind;

        ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                 analyzer,
                                                                 projectIndex,
                                                                 memberHit.moduleName,
                                                                 &resolved);
        sourceKind = ZrLanguageServer_LspModuleMetadata_SourceKindLabel(resolved.sourceKind);
        if (resolved.sourceRecord != ZR_NULL) {
            SZrSemanticAnalyzer *targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, resolved.sourceRecord->uri);
            if (targetAnalyzer == ZR_NULL) {
                targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, resolved.sourceRecord->uri);
            }
            SZrSymbol *targetSymbol = find_public_global_symbol(targetAnalyzer, memberHit.memberName);
            if (targetSymbol != ZR_NULL) {
                ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
                return create_hover_for_symbol(state, context, targetSymbol, memberHit.location, result);
            }
        }

        if (resolved.modulePrototype != ZR_NULL) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return create_module_prototype_member_hover(state,
                                                        resolved.modulePrototype,
                                                        memberHit.memberName,
                                                        sourceKind,
                                                        memberHit.location,
                                                        result);
        }

        if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
            SZrIoSource *binarySource = ZR_NULL;
            SZrLspIntermediateModuleMetadata intermediateMetadata;
            SZrFunction *intermediateFunction = ZR_NULL;
            TZrBool hoverCreated = ZR_FALSE;
            ZrCore_Array_Construct(&intermediateMetadata.exportedSymbols);

            if (ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(state,
                                                                          projectIndex,
                                                                          memberHit.moduleName,
                                                                          &binarySource) &&
                binarySource != ZR_NULL &&
                binarySource->modulesLength > 0 &&
                binarySource->modules != ZR_NULL &&
                binarySource->modules[0].entryFunction != ZR_NULL) {
                hoverCreated = create_binary_module_member_hover(state,
                                                                 binarySource->modules[0].entryFunction,
                                                                 memberHit.memberName,
                                                                 sourceKind,
                                                                 memberHit.location,
                                                                 result);
            } else if (ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleMetadata(state,
                                                                                         projectIndex,
                                                                                         memberHit.moduleName,
                                                                                         &intermediateMetadata)) {
                hoverCreated = create_intermediate_module_member_hover(state,
                                                                       &intermediateMetadata,
                                                                       memberHit.memberName,
                                                                       sourceKind,
                                                                       memberHit.location,
                                                                       result);
            } else if (ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleFunction(state,
                                                                                         projectIndex,
                                                                                         memberHit.moduleName,
                                                                                         &intermediateFunction) &&
                       intermediateFunction != ZR_NULL) {
                hoverCreated = create_compiled_module_member_hover(state,
                                                                   intermediateFunction,
                                                                   memberHit.memberName,
                                                                   sourceKind,
                                                                   memberHit.location,
                                                                   result);
            }
            if (binarySource != ZR_NULL) {
                ZrCore_Io_ReadSourceFree(state->global, binarySource);
            }
            ZrLanguageServer_LspModuleMetadata_FreeIntermediateModuleMetadata(state, &intermediateMetadata);
            if (intermediateFunction != ZR_NULL) {
                ZrCore_Function_Free(state, intermediateFunction);
            }
            if (hoverCreated) {
                ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
                return ZR_TRUE;
            }
        }

        if (resolved.nativeDescriptor != ZR_NULL) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return create_native_member_hover(state,
                                              resolved.nativeDescriptor,
                                              memberHit.memberName,
                                              sourceKind,
                                              memberHit.location,
                                              result);
        }
    }

    if (ZrLanguageServer_LspProject_FindImportBindingHit(analyzer->ast, &bindings, fileRange, &binding, &bindingRange) &&
        binding != ZR_NULL) {
        SZrLspResolvedImportedModule resolved;
        TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];

        ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                 analyzer,
                                                                 projectIndex,
                                                                 binding->moduleName,
                                                                 &resolved);
        snprintf(buffer,
                 sizeof(buffer),
                 "module <%s>\n\nSource: %s",
                 project_feature_get_string_text(binding->moduleName) != ZR_NULL
                     ? project_feature_get_string_text(binding->moduleName)
                     : "",
                 ZrLanguageServer_LspModuleMetadata_SourceKindLabel(resolved.sourceKind));
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return create_hover_from_markdown(state, buffer, bindingRange, result);
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return ZR_FALSE;
}
