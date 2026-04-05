#include "lsp_project_internal.h"
#include "lsp_module_metadata.h"
#include "lsp_semantic_query.h"
#include "lsp_semantic_import_chain.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN ((TZrInt32)-1)
#define ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS (-1)
#define ZR_LSP_SEMANTIC_TOKEN_COMPARE_EQUAL 0
#define ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER 1

typedef enum EZrLspSemanticTokenType {
    ZR_LSP_SEMANTIC_TOKEN_NAMESPACE = 0,
    ZR_LSP_SEMANTIC_TOKEN_CLASS = 1,
    ZR_LSP_SEMANTIC_TOKEN_STRUCT = 2,
    ZR_LSP_SEMANTIC_TOKEN_INTERFACE = 3,
    ZR_LSP_SEMANTIC_TOKEN_ENUM = 4,
    ZR_LSP_SEMANTIC_TOKEN_FUNCTION = 5,
    ZR_LSP_SEMANTIC_TOKEN_METHOD = 6,
    ZR_LSP_SEMANTIC_TOKEN_PROPERTY = 7,
    ZR_LSP_SEMANTIC_TOKEN_VARIABLE = 8,
    ZR_LSP_SEMANTIC_TOKEN_PARAMETER = 9,
    ZR_LSP_SEMANTIC_TOKEN_KEYWORD = 10,
    ZR_LSP_SEMANTIC_TOKEN_DECORATOR = 11,
    ZR_LSP_SEMANTIC_TOKEN_META_METHOD = 12
} EZrLspSemanticTokenType;

typedef struct SZrLspSemanticTokenEntry {
    TZrUInt32 line;
    TZrUInt32 character;
    TZrUInt32 length;
    TZrUInt32 typeIndex;
} SZrLspSemanticTokenEntry;

static const TZrChar *const g_semanticTokenTypeNames[] = {
    "namespace",
    "class",
    "struct",
    "interface",
    "enum",
    "function",
    "method",
    "property",
    "variable",
    "parameter",
    "keyword",
    "decorator",
    "metaMethod"
};

static void semantic_token_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length);
static TZrBool semantic_token_string_equals_native(SZrString *value, const TZrChar *text, TZrSize length);
static TZrBool semantic_token_is_identifier_start(TZrChar value);
static TZrBool semantic_token_is_identifier_char(TZrChar value);
static TZrInt32 semantic_token_type_from_symbol(EZrSymbolType type);
static TZrInt32 semantic_token_type_from_value_kind(EZrLspResolvedValueKind kind);
static TZrInt32 semantic_token_type_from_metadata_member(const SZrLspResolvedMetadataMember *member);
static TZrInt32 semantic_token_type_from_semantic_query(const SZrLspSemanticQuery *query);
static TZrInt32 semantic_token_resolve_query_type(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  TZrUInt32 line,
                                                  TZrUInt32 character);
static TZrInt32 semantic_token_resolve_metadata_chain_member(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *moduleName,
                                                             const TZrChar *text,
                                                             TZrSize length,
                                                             SZrString **outNextModuleName);
static TZrUInt32 semantic_token_type_priority(TZrUInt32 typeIndex);
static void semantic_token_add(SZrState *state,
                               SZrArray *entries,
                               TZrUInt32 line,
                               TZrUInt32 character,
                               TZrUInt32 length,
                               TZrUInt32 typeIndex);
static void semantic_token_add_file_range(SZrState *state,
                                          SZrArray *entries,
                                          SZrFileRange range,
                                          TZrUInt32 typeIndex);
static void semantic_token_add_symbol_tokens(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrString *uri,
                                             SZrArray *entries);
static void semantic_token_scan_source(SZrState *state,
                                       SZrLspContext *context,
                                       SZrString *uri,
                                       const TZrChar *content,
                                       TZrSize contentLength,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrArray *bindings,
                                       SZrLspProjectIndex *projectIndex,
                                       SZrArray *entries);
static void semantic_token_append_encoded(SZrState *state, SZrArray *entries, SZrArray *result);

static void semantic_token_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
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

static TZrBool semantic_token_string_equals_native(SZrString *value, const TZrChar *text, TZrSize length) {
    TZrNativeString currentText;
    TZrSize currentLength;

    if (value == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    semantic_token_get_string_view(value, &currentText, &currentLength);
    return currentText != ZR_NULL && currentLength == length && memcmp(currentText, text, length) == 0;
}

static TZrBool semantic_token_is_identifier_start(TZrChar value) {
    return isalpha((unsigned char)value) || value == '_';
}

static TZrBool semantic_token_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

static TZrInt32 semantic_token_type_from_symbol(EZrSymbolType type) {
    switch (type) {
        case ZR_SYMBOL_MODULE: return ZR_LSP_SEMANTIC_TOKEN_NAMESPACE;
        case ZR_SYMBOL_CLASS: return ZR_LSP_SEMANTIC_TOKEN_CLASS;
        case ZR_SYMBOL_STRUCT: return ZR_LSP_SEMANTIC_TOKEN_STRUCT;
        case ZR_SYMBOL_INTERFACE: return ZR_LSP_SEMANTIC_TOKEN_INTERFACE;
        case ZR_SYMBOL_ENUM:
        case ZR_SYMBOL_ENUM_MEMBER: return ZR_LSP_SEMANTIC_TOKEN_ENUM;
        case ZR_SYMBOL_FUNCTION: return ZR_LSP_SEMANTIC_TOKEN_FUNCTION;
        case ZR_SYMBOL_METHOD: return ZR_LSP_SEMANTIC_TOKEN_METHOD;
        case ZR_SYMBOL_PROPERTY:
        case ZR_SYMBOL_FIELD: return ZR_LSP_SEMANTIC_TOKEN_PROPERTY;
        case ZR_SYMBOL_PARAMETER: return ZR_LSP_SEMANTIC_TOKEN_PARAMETER;
        default: return ZR_LSP_SEMANTIC_TOKEN_VARIABLE;
    }
}

static TZrInt32 semantic_token_type_from_value_kind(EZrLspResolvedValueKind kind) {
    switch (kind) {
        case ZR_LSP_RESOLVED_VALUE_KIND_MODULE:
            return ZR_LSP_SEMANTIC_TOKEN_NAMESPACE;
        case ZR_LSP_RESOLVED_VALUE_KIND_CALLABLE:
            return ZR_LSP_SEMANTIC_TOKEN_METHOD;
        default:
            return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }
}

static TZrInt32 semantic_token_type_from_metadata_member(const SZrLspResolvedMetadataMember *member) {
    if (member == ZR_NULL) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    switch (member->memberKind) {
        case ZR_LSP_METADATA_MEMBER_MODULE:
            return ZR_LSP_SEMANTIC_TOKEN_NAMESPACE;
        case ZR_LSP_METADATA_MEMBER_CONSTANT:
        case ZR_LSP_METADATA_MEMBER_FIELD:
            return ZR_LSP_SEMANTIC_TOKEN_PROPERTY;
        case ZR_LSP_METADATA_MEMBER_FUNCTION:
        case ZR_LSP_METADATA_MEMBER_METHOD:
            return ZR_LSP_SEMANTIC_TOKEN_METHOD;
        case ZR_LSP_METADATA_MEMBER_TYPE:
            if (member->typeDescriptor != ZR_NULL &&
                member->typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                return ZR_LSP_SEMANTIC_TOKEN_CLASS;
            }
            if (member->typeDescriptor != ZR_NULL) {
                return ZR_LSP_SEMANTIC_TOKEN_STRUCT;
            }
            if (member->declarationSymbol != ZR_NULL) {
                return semantic_token_type_from_symbol(member->declarationSymbol->type);
            }
            return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
        default:
            return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }
}

static TZrInt32 semantic_token_type_from_semantic_query(const SZrLspSemanticQuery *query) {
    TZrInt32 tokenType;

    if (query == ZR_NULL) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    switch (query->kind) {
        case ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL:
            if (query->symbol != ZR_NULL) {
                tokenType = semantic_token_type_from_symbol(query->symbol->type);
                if (tokenType != ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
                    return tokenType;
                }
            }
            return semantic_token_type_from_value_kind(query->resolvedTypeInfo.valueKind);
        case ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER:
        case ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION:
        case ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER:
            tokenType = semantic_token_type_from_metadata_member(&query->resolvedMember);
            if (tokenType != ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
                return tokenType;
            }
            return semantic_token_type_from_value_kind(query->resolvedTypeInfo.valueKind);
        default:
            return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }
}

static TZrInt32 semantic_token_resolve_query_type(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  TZrUInt32 line,
                                                  TZrUInt32 character) {
    SZrLspSemanticQuery query;
    SZrLspPosition position;
    TZrInt32 tokenType = ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    position.line = (TZrInt32)line;
    position.character = (TZrInt32)character;
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &query)) {
        tokenType = semantic_token_type_from_semantic_query(&query);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return tokenType;
}

static TZrInt32 semantic_token_resolve_metadata_chain_member(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *moduleName,
                                                             const TZrChar *text,
                                                             TZrSize length,
                                                             SZrString **outNextModuleName) {
    SZrLspResolvedMetadataMember resolvedMember;
    SZrString *memberName;
    TZrInt32 tokenType;

    if (outNextModuleName != ZR_NULL) {
        *outNextModuleName = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || moduleName == ZR_NULL || text == ZR_NULL || length == 0) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    memberName = ZrCore_String_Create(state, (TZrNativeString)text, length);
    if (memberName == ZR_NULL) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    if (!ZrLanguageServer_LspSemanticImportChain_ResolveLinkedMember(state,
                                                                     context,
                                                                     analyzer,
                                                                     projectIndex,
                                                                     moduleName,
                                                                     memberName,
                                                                     &resolvedMember,
                                                                     outNextModuleName)) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    tokenType = semantic_token_type_from_metadata_member(&resolvedMember);
    if (tokenType == ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
        tokenType = semantic_token_type_from_value_kind(
            resolvedMember.memberKind == ZR_LSP_METADATA_MEMBER_MODULE
                ? ZR_LSP_RESOLVED_VALUE_KIND_MODULE
                : resolvedMember.memberKind == ZR_LSP_METADATA_MEMBER_FUNCTION ||
                          resolvedMember.memberKind == ZR_LSP_METADATA_MEMBER_METHOD
                      ? ZR_LSP_RESOLVED_VALUE_KIND_CALLABLE
                      : ZR_LSP_RESOLVED_VALUE_KIND_UNKNOWN);
    }

    return tokenType;
}

static TZrUInt32 semantic_token_type_priority(TZrUInt32 typeIndex) {
    switch (typeIndex) {
        case ZR_LSP_SEMANTIC_TOKEN_VARIABLE:
            return 0;
        case ZR_LSP_SEMANTIC_TOKEN_PARAMETER:
            return 1;
        case ZR_LSP_SEMANTIC_TOKEN_PROPERTY:
        case ZR_LSP_SEMANTIC_TOKEN_METHOD:
        case ZR_LSP_SEMANTIC_TOKEN_FUNCTION:
            return 2;
        case ZR_LSP_SEMANTIC_TOKEN_NAMESPACE:
        case ZR_LSP_SEMANTIC_TOKEN_CLASS:
        case ZR_LSP_SEMANTIC_TOKEN_STRUCT:
        case ZR_LSP_SEMANTIC_TOKEN_INTERFACE:
        case ZR_LSP_SEMANTIC_TOKEN_ENUM:
        case ZR_LSP_SEMANTIC_TOKEN_KEYWORD:
        case ZR_LSP_SEMANTIC_TOKEN_DECORATOR:
        case ZR_LSP_SEMANTIC_TOKEN_META_METHOD:
            return 3;
        default:
            return 0;
    }
}

static void semantic_token_add(SZrState *state,
                               SZrArray *entries,
                               TZrUInt32 line,
                               TZrUInt32 character,
                               TZrUInt32 length,
                               TZrUInt32 typeIndex) {
    SZrLspSemanticTokenEntry entry;

    if (state == ZR_NULL || entries == ZR_NULL || length == 0) {
        return;
    }

    for (TZrSize index = 0; index < entries->length; index++) {
        SZrLspSemanticTokenEntry *current =
            (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(entries, index);
        if (current != ZR_NULL &&
            current->line == line &&
            current->character == character &&
            current->length == length) {
            if (current->typeIndex == typeIndex) {
                return;
            }
            if (semantic_token_type_priority(typeIndex) > semantic_token_type_priority(current->typeIndex)) {
                current->typeIndex = typeIndex;
            }
            return;
        }
    }

    entry.line = line;
    entry.character = character;
    entry.length = length;
    entry.typeIndex = typeIndex;
    ZrCore_Array_Push(state, entries, &entry);
}

static void semantic_token_add_file_range(SZrState *state,
                                          SZrArray *entries,
                                          SZrFileRange range,
                                          TZrUInt32 typeIndex) {
    SZrLspRange lspRange;
    TZrInt32 tokenLength;

    if (state == ZR_NULL || entries == ZR_NULL || range.source == ZR_NULL) {
        return;
    }

    lspRange = ZrLanguageServer_LspRange_FromFileRange(range);
    if (lspRange.start.line != lspRange.end.line) {
        return;
    }

    tokenLength = lspRange.end.character - lspRange.start.character;
    if (tokenLength <= 0) {
        return;
    }

    semantic_token_add(state,
                       entries,
                       (TZrUInt32)lspRange.start.line,
                       (TZrUInt32)lspRange.start.character,
                       (TZrUInt32)tokenLength,
                       typeIndex);
}

static int semantic_token_entry_compare(const void *leftPtr, const void *rightPtr) {
    const SZrLspSemanticTokenEntry *left = (const SZrLspSemanticTokenEntry *)leftPtr;
    const SZrLspSemanticTokenEntry *right = (const SZrLspSemanticTokenEntry *)rightPtr;

    if (left->line != right->line) {
        return left->line < right->line ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                        : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->character != right->character) {
        return left->character < right->character ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                                  : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->length != right->length) {
        return left->length < right->length ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                            : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->typeIndex != right->typeIndex) {
        return left->typeIndex < right->typeIndex ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                                  : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }

    return ZR_LSP_SEMANTIC_TOKEN_COMPARE_EQUAL;
}

static void semantic_token_append_encoded(SZrState *state, SZrArray *entries, SZrArray *result) {
    TZrUInt32 previousLine = 0;
    TZrUInt32 previousCharacter = 0;
    TZrBool isFirst = ZR_TRUE;

    if (state == ZR_NULL || entries == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (entries->length > 1) {
        qsort(entries->head, entries->length, sizeof(SZrLspSemanticTokenEntry), semantic_token_entry_compare);
    }

    for (TZrSize index = 0; index < entries->length; index++) {
        SZrLspSemanticTokenEntry *entry =
            (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(entries, index);
        TZrUInt32 deltaLine;
        TZrUInt32 deltaCharacter;
        TZrUInt32 modifiers = 0;

        if (entry == ZR_NULL) {
            continue;
        }

        if (isFirst) {
            deltaLine = entry->line;
            deltaCharacter = entry->character;
            isFirst = ZR_FALSE;
        } else {
            deltaLine = entry->line - previousLine;
            deltaCharacter = deltaLine == 0 ? entry->character - previousCharacter : entry->character;
        }

        ZrCore_Array_Push(state, result, &deltaLine);
        ZrCore_Array_Push(state, result, &deltaCharacter);
        ZrCore_Array_Push(state, result, &entry->length);
        ZrCore_Array_Push(state, result, &entry->typeIndex);
        ZrCore_Array_Push(state, result, &modifiers);

        previousLine = entry->line;
        previousCharacter = entry->character;
    }
}

static SZrLspImportBinding *semantic_token_find_import_binding(SZrArray *bindings,
                                                               const TZrChar *text,
                                                               TZrSize length) {
    for (TZrSize index = 0; bindings != ZR_NULL && index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL &&
            semantic_token_string_equals_native((*bindingPtr)->aliasName, text, length)) {
            return *bindingPtr;
        }
    }

    return ZR_NULL;
}

static void semantic_token_add_scope_symbols(SZrState *state,
                                             SZrArray *entries,
                                             SZrString *uri,
                                             SZrSymbolScope *scope) {
    if (state == ZR_NULL || entries == ZR_NULL || uri == ZR_NULL || scope == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < scope->symbols.length; index++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, index);
        TZrInt32 typeIndex;

        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->selectionRange.source, uri)) {
            continue;
        }

        typeIndex = semantic_token_type_from_symbol((*symbolPtr)->type);
        if (typeIndex != ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
            semantic_token_add_file_range(state,
                                          entries,
                                          (*symbolPtr)->selectionRange,
                                          (TZrUInt32)typeIndex);
        }
    }
}

static void semantic_token_add_symbol_tokens(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrString *uri,
                                             SZrArray *entries) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        uri == ZR_NULL || entries == ZR_NULL) {
        return;
    }

    semantic_token_add_scope_symbols(state, entries, uri, analyzer->symbolTable->globalScope);
    for (TZrSize index = 0; index < analyzer->symbolTable->allScopes.length; index++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&analyzer->symbolTable->allScopes, index);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL && *scopePtr != analyzer->symbolTable->globalScope) {
            semantic_token_add_scope_symbols(state, entries, uri, *scopePtr);
        }
    }
}

static TZrBool semantic_token_is_keyword_directive(const TZrChar *text, TZrSize length) {
    return ZrLanguageServer_Lsp_IsKnownDirectiveToken(text, length);
}

static TZrBool semantic_token_is_meta_method(const TZrChar *text, TZrSize length) {
    return ZrLanguageServer_Lsp_IsKnownMetaMethodToken(text, length);
}

static TZrBool semantic_token_is_keyword_word(const TZrChar *text, TZrSize length) {
    return length == 5 && memcmp(text, "using", 5) == 0;
}

static TZrInt32 semantic_token_guess_member_type(const TZrChar *content,
                                                 TZrSize contentLength,
                                                 TZrSize segmentEnd,
                                                 TZrBool hasFollowingDot) {
    TZrSize lookahead = segmentEnd;

    if (hasFollowingDot) {
        return ZR_LSP_SEMANTIC_TOKEN_NAMESPACE;
    }

    while (lookahead < contentLength && isspace((unsigned char)content[lookahead])) {
        if (content[lookahead] == '\n' || content[lookahead] == '\r') {
            break;
        }
        lookahead++;
    }

    return lookahead < contentLength && content[lookahead] == '('
               ? ZR_LSP_SEMANTIC_TOKEN_METHOD
               : ZR_LSP_SEMANTIC_TOKEN_PROPERTY;
}

static void semantic_token_scan_source(SZrState *state,
                                       SZrLspContext *context,
                                       SZrString *uri,
                                       const TZrChar *content,
                                       TZrSize contentLength,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrArray *bindings,
                                       SZrLspProjectIndex *projectIndex,
                                       SZrArray *entries) {
    TZrSize offset = 0;
    TZrUInt32 line = 0;
    TZrUInt32 character = 0;

    ZR_UNUSED_PARAMETER(uri);

    while (offset < contentLength) {
        TZrChar current = content[offset];

        if (current == '\r') {
            offset++;
            continue;
        }
        if (current == '\n') {
            line++;
            character = 0;
            offset++;
            continue;
        }
        if (current == '/' && offset + 1 < contentLength && content[offset + 1] == '/') {
            offset += 2;
            character += 2;
            while (offset < contentLength && content[offset] != '\n') {
                offset++;
                character++;
            }
            continue;
        }
        if (current == '/' && offset + 1 < contentLength && content[offset + 1] == '*') {
            offset += 2;
            character += 2;
            while (offset + 1 < contentLength) {
                if (content[offset] == '\n') {
                    line++;
                    character = 0;
                    offset++;
                    continue;
                }
                if (content[offset] == '*' && content[offset + 1] == '/') {
                    offset += 2;
                    character += 2;
                    break;
                }
                offset++;
                character++;
            }
            continue;
        }
        if (current == '"' || current == '\'') {
            TZrChar quote = current;
            offset++;
            character++;
            while (offset < contentLength) {
                TZrChar stringChar = content[offset];
                if (stringChar == '\n') {
                    line++;
                    character = 0;
                    offset++;
                    continue;
                }
                if (stringChar == '\\' && offset + 1 < contentLength) {
                    offset += 2;
                    character += 2;
                    continue;
                }
                offset++;
                character++;
                if (stringChar == quote) {
                    break;
                }
            }
            continue;
        }
        if (current == '%') {
            TZrSize start = offset;
            TZrUInt32 startCharacter = character;

            offset++;
            character++;
            while (offset < contentLength && semantic_token_is_identifier_char(content[offset])) {
                offset++;
                character++;
            }
            if (semantic_token_is_keyword_directive(content + start, offset - start)) {
                semantic_token_add(state,
                                   entries,
                                   line,
                                   startCharacter,
                                   (TZrUInt32)(offset - start),
                                   ZR_LSP_SEMANTIC_TOKEN_KEYWORD);
            }
            continue;
        }
        if (current == '#') {
            TZrSize start = offset;
            TZrUInt32 startCharacter = character;

            offset++;
            character++;
            while (offset < contentLength &&
                   content[offset] != '\n' &&
                   content[offset] != '\r' &&
                   content[offset] != '#') {
                offset++;
                character++;
            }
            if (offset < contentLength && content[offset] == '#') {
                offset++;
                character++;
                semantic_token_add(state,
                                   entries,
                                   line,
                                   startCharacter,
                                   (TZrUInt32)(offset - start),
                                   ZR_LSP_SEMANTIC_TOKEN_DECORATOR);
            }
            continue;
        }
        if (current == '@') {
            TZrSize start = offset;
            TZrUInt32 startCharacter = character;

            offset++;
            character++;
            while (offset < contentLength && semantic_token_is_identifier_char(content[offset])) {
                offset++;
                character++;
            }
            if (semantic_token_is_meta_method(content + start, offset - start)) {
                semantic_token_add(state,
                                   entries,
                                   line,
                                   startCharacter,
                                   (TZrUInt32)(offset - start),
                                   ZR_LSP_SEMANTIC_TOKEN_META_METHOD);
            }
            continue;
        }
        if (!semantic_token_is_identifier_start(current)) {
            offset++;
            character++;
            continue;
        }

        {
            TZrSize start = offset;
            TZrUInt32 startLine = line;
            TZrUInt32 startCharacter = character;
            TZrSize length;
            TZrSize previous = start;
            SZrLspImportBinding *binding;

            while (offset < contentLength && semantic_token_is_identifier_char(content[offset])) {
                offset++;
                character++;
            }
            length = offset - start;

            if (semantic_token_is_keyword_word(content + start, length)) {
                semantic_token_add(state,
                                   entries,
                                   startLine,
                                   startCharacter,
                                   (TZrUInt32)length,
                                   ZR_LSP_SEMANTIC_TOKEN_KEYWORD);
            }

            while (previous > 0 && isspace((unsigned char)content[previous - 1])) {
                previous--;
            }
            if (previous > 0 && content[previous - 1] == '.') {
                TZrInt32 tokenType =
                    semantic_token_resolve_query_type(state, context, uri, startLine, startCharacter);
                if (tokenType < 0) {
                    tokenType = semantic_token_guess_member_type(content,
                                                                 contentLength,
                                                                 offset,
                                                                 ZR_FALSE);
                }
                if (tokenType >= 0) {
                    semantic_token_add(state,
                                       entries,
                                       startLine,
                                       startCharacter,
                                       (TZrUInt32)length,
                                       (TZrUInt32)tokenType);
                }
                continue;
            }

            binding = semantic_token_find_import_binding(bindings, content + start, length);
            if (binding == ZR_NULL) {
                continue;
            }

            semantic_token_add(state,
                               entries,
                               startLine,
                               startCharacter,
                               (TZrUInt32)length,
                               ZR_LSP_SEMANTIC_TOKEN_NAMESPACE);

            {
                SZrString *currentModuleName = binding->moduleName;
                TZrSize chainOffset = offset;
                TZrUInt32 chainCharacter = character;

                while (chainOffset < contentLength) {
                    TZrUInt32 segmentCharacter;
                    TZrSize segmentStart;
                    TZrSize segmentLength;
                    TZrSize lookahead;
                    TZrBool hasFollowingDot;
                    TZrInt32 tokenType;
                    TZrInt32 fallbackTokenType = ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
                    SZrString *nextModuleName = ZR_NULL;

                    while (chainOffset < contentLength &&
                           isspace((unsigned char)content[chainOffset]) &&
                           content[chainOffset] != '\n' &&
                           content[chainOffset] != '\r') {
                        chainOffset++;
                        chainCharacter++;
                    }
                    if (chainOffset >= contentLength || content[chainOffset] != '.') {
                        break;
                    }

                    chainOffset++;
                    chainCharacter++;
                    while (chainOffset < contentLength &&
                           isspace((unsigned char)content[chainOffset]) &&
                           content[chainOffset] != '\n' &&
                           content[chainOffset] != '\r') {
                        chainOffset++;
                        chainCharacter++;
                    }
                    if (chainOffset >= contentLength || !semantic_token_is_identifier_start(content[chainOffset])) {
                        break;
                    }

                    segmentStart = chainOffset;
                    segmentCharacter = chainCharacter;
                    while (chainOffset < contentLength && semantic_token_is_identifier_char(content[chainOffset])) {
                        chainOffset++;
                        chainCharacter++;
                    }
                    segmentLength = chainOffset - segmentStart;

                    lookahead = chainOffset;
                    while (lookahead < contentLength &&
                           isspace((unsigned char)content[lookahead]) &&
                           content[lookahead] != '\n' &&
                           content[lookahead] != '\r') {
                        lookahead++;
                    }
                    hasFollowingDot = lookahead < contentLength && content[lookahead] == '.';

                    tokenType = semantic_token_resolve_query_type(state,
                                                                  context,
                                                                  uri,
                                                                  startLine,
                                                                  segmentCharacter);
                    fallbackTokenType = semantic_token_resolve_metadata_chain_member(state,
                                                                                     context,
                                                                                     analyzer,
                                                                                     projectIndex,
                                                                                     currentModuleName,
                                                                                     content + segmentStart,
                                                                                     segmentLength,
                                                                                     &nextModuleName);
                    if (nextModuleName != ZR_NULL) {
                        currentModuleName = nextModuleName;
                    }
                    if (tokenType < 0) {
                        tokenType = fallbackTokenType;
                    }
                    if (tokenType < 0) {
                        tokenType = semantic_token_guess_member_type(content,
                                                                     contentLength,
                                                                     chainOffset,
                                                                     hasFollowingDot);
                    }

                    semantic_token_add(state,
                                       entries,
                                       startLine,
                                       segmentCharacter,
                                       (TZrUInt32)segmentLength,
                                       (TZrUInt32)tokenType);
                }

                offset = chainOffset;
                character = chainCharacter;
            }
        }
    }
}

TZrBool ZrLanguageServer_Lsp_GetSemanticTokens(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *uri,
                                               SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;
    SZrLspProjectIndex *projectIndex;
    SZrArray entries;
    SZrArray bindings;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    } else {
        ZrCore_Array_Empty(result);
    }

    ZrCore_Array_Init(state,
                      &entries,
                      sizeof(SZrLspSemanticTokenEntry),
                      ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
        semantic_token_add_symbol_tokens(state, analyzer, uri, &entries);
    }

    semantic_token_scan_source(state,
                               context,
                               uri,
                               fileVersion->content,
                               fileVersion->contentLength,
                               analyzer,
                               &bindings,
                               projectIndex,
                               &entries);
    semantic_token_append_encoded(state, &entries, result);

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    ZrCore_Array_Free(state, &entries);
    return ZR_TRUE;
}

TZrSize ZrLanguageServer_Lsp_SemanticTokenTypeCount(void) {
    return sizeof(g_semanticTokenTypeNames) / sizeof(g_semanticTokenTypeNames[0]);
}

const TZrChar *ZrLanguageServer_Lsp_SemanticTokenTypeName(TZrSize index) {
    return index < ZrLanguageServer_Lsp_SemanticTokenTypeCount() ? g_semanticTokenTypeNames[index] : ZR_NULL;
}
