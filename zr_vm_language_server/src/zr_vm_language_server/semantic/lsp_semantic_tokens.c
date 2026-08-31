#include "project/lsp_project_internal.h"
#include "module/lsp_module_metadata.h"
#include "semantic/lsp_semantic_token_canonical.h"
#include "semantic/lsp_semantic_token_entries.h"
#include "semantic/lsp_semantic_query.h"
#include "semantic/lsp_semantic_import_chain.h"

#include <ctype.h>
#include <string.h>

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
static TZrInt32 semantic_token_resolve_metadata_chain_member(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *moduleName,
                                                             const TZrChar *text,
                                                             TZrSize length,
                                                             SZrString **outNextModuleName);
static TZrBool semantic_token_add_utf16_span(SZrState *state,
                                             SZrArray *entries,
                                             const TZrChar *content,
                                             TZrSize contentLength,
                                             TZrSize startOffset,
                                             TZrSize endOffset,
                                             TZrUInt32 typeIndex);
static TZrBool semantic_token_add_utf16_span_with_modifiers(SZrState *state,
                                                            SZrArray *entries,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            TZrSize startOffset,
                                                            TZrSize endOffset,
                                                            TZrUInt32 typeIndex,
                                                            TZrUInt32 modifiers);
static void semantic_token_add_file_range_with_modifiers(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrArray *entries,
                                                         SZrString *uri,
                                                         SZrFileRange range,
                                                         TZrUInt32 typeIndex,
                                                         TZrUInt32 modifiers);
static void semantic_token_add_symbol_tokens(SZrState *state,
                                             SZrLspContext *context,
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
        case ZR_LSP_METADATA_MEMBER_PROPERTY:
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

static TZrBool semantic_token_is_member_like(TZrInt32 tokenType) {
    return tokenType == ZR_LSP_SEMANTIC_TOKEN_FUNCTION || tokenType == ZR_LSP_SEMANTIC_TOKEN_METHOD ||
           tokenType == ZR_LSP_SEMANTIC_TOKEN_PROPERTY;
}

static TZrBool semantic_token_should_prefer_chain_fallback(TZrInt32 tokenType, TZrInt32 fallbackTokenType) {
    if (fallbackTokenType == ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
        return ZR_FALSE;
    }
    if (tokenType == ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
        return ZR_TRUE;
    }
    if (tokenType != fallbackTokenType &&
        (fallbackTokenType == ZR_LSP_SEMANTIC_TOKEN_NAMESPACE ||
         fallbackTokenType == ZR_LSP_SEMANTIC_TOKEN_CLASS ||
         fallbackTokenType == ZR_LSP_SEMANTIC_TOKEN_STRUCT ||
         fallbackTokenType == ZR_LSP_SEMANTIC_TOKEN_INTERFACE ||
         fallbackTokenType == ZR_LSP_SEMANTIC_TOKEN_ENUM)) {
        return ZR_TRUE;
    }

    return semantic_token_is_member_like(fallbackTokenType) && !semantic_token_is_member_like(tokenType);
}

static TZrBool semantic_token_add_utf16_span(SZrState *state,
                                             SZrArray *entries,
                                             const TZrChar *content,
                                             TZrSize contentLength,
                                             TZrSize startOffset,
                                             TZrSize endOffset,
                                             TZrUInt32 typeIndex) {
    return ZrLanguageServer_LspSemanticTokenEntries_AddUtf16Span(
        state, entries, content, contentLength, startOffset, endOffset, typeIndex, 0U);
}

static TZrBool semantic_token_add_utf16_span_with_modifiers(SZrState *state,
                                                            SZrArray *entries,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            TZrSize startOffset,
                                                            TZrSize endOffset,
                                                            TZrUInt32 typeIndex,
                                                            TZrUInt32 modifiers) {
    return ZrLanguageServer_LspSemanticTokenEntries_AddUtf16Span(state,
                                                                  entries,
                                                                  content,
                                                                  contentLength,
                                                                  startOffset,
                                                                  endOffset,
                                                                  typeIndex,
                                                                  modifiers);
}

static void semantic_token_add_file_range_with_modifiers(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrArray *entries,
                                                         SZrString *uri,
                                                         SZrFileRange range,
                                                         TZrUInt32 typeIndex,
                                                         TZrUInt32 modifiers) {
    ZrLanguageServer_LspSemanticTokenEntries_AddFileRange(
        state, context, entries, uri, range, typeIndex, modifiers);
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

static TZrBool semantic_token_identifier_is_import_alias_declaration(const TZrChar *content,
                                                                     TZrSize contentLength,
                                                                     TZrSize identifierEnd) {
    TZrSize cursor = identifierEnd;
    static const TZrChar importToken[] = "import";

    while (cursor < contentLength &&
           isspace((unsigned char)content[cursor]) &&
           content[cursor] != '\n' &&
           content[cursor] != '\r') {
        cursor++;
    }
    if (cursor >= contentLength || content[cursor] != '=') {
        return ZR_FALSE;
    }

    cursor++;
    while (cursor < contentLength &&
           isspace((unsigned char)content[cursor]) &&
           content[cursor] != '\n' &&
           content[cursor] != '\r') {
        cursor++;
    }

    if (cursor < contentLength && content[cursor] == '%') {
        cursor++;
    }

    return cursor + sizeof(importToken) - 1 <= contentLength &&
           memcmp(content + cursor, importToken, sizeof(importToken) - 1) == 0 &&
           (cursor + sizeof(importToken) - 1 == contentLength ||
            content[cursor + sizeof(importToken) - 1] == '(' ||
            isspace((unsigned char)content[cursor + sizeof(importToken) - 1]));
}

static void semantic_token_append_import_alias_binding(SZrState *state,
                                                       SZrArray *bindings,
                                                       SZrString *uri,
                                                       const TZrChar *content,
                                                       TZrSize contentLength,
                                                       TZrSize aliasStart,
                                                       TZrSize aliasLength,
                                                       TZrUInt32 aliasLine,
                                                       TZrUInt32 aliasCharacter) {
    SZrLspImportBinding *binding;

    if (state == ZR_NULL || bindings == ZR_NULL || uri == ZR_NULL || content == ZR_NULL ||
        aliasLength == 0 || aliasStart + aliasLength > contentLength ||
        semantic_token_find_import_binding(bindings, content + aliasStart, aliasLength) != ZR_NULL ||
        !semantic_token_identifier_is_import_alias_declaration(content, contentLength, aliasStart + aliasLength)) {
        return;
    }

    binding = (SZrLspImportBinding *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspImportBinding));
    if (binding == ZR_NULL) {
        return;
    }

    memset(binding, 0, sizeof(*binding));
    binding->aliasName = ZrCore_String_Create(state, (TZrNativeString)(content + aliasStart), aliasLength);
    binding->aliasLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(aliasStart, aliasLine, aliasCharacter),
        ZrParser_FilePosition_Create(aliasStart + aliasLength, aliasLine, aliasCharacter + (TZrUInt32)aliasLength),
        uri);
    binding->modulePathLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 0, 0),
        ZrParser_FilePosition_Create(0, 0, 0),
        uri);

    if (binding->aliasName == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, binding, sizeof(SZrLspImportBinding));
        return;
    }
    ZrCore_Array_Push(state, bindings, &binding);
}

static void semantic_token_add_symbol_tokens(SZrState *state,
                                             SZrLspContext *context,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrString *uri,
                                             SZrArray *entries) {
    SZrParserSemanticQueryScope scope;
    SZrArray declarations;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->ast == ZR_NULL || uri == ZR_NULL || entries == ZR_NULL) {
        return;
    }

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE;
    scope.root = analyzer->ast;
    ZrCore_Array_Construct(&declarations);
    if (!ZrParser_SemanticQuery_DeclaredSymbols(
                analyzer->semanticContext, &scope, &declarations)) {
        ZrCore_Array_Free(state, &declarations);
        return;
    }

    for (TZrSize index = 0U; index < declarations.length; index++) {
        const SZrParserSemanticSymbolQuery *symbol =
            (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(&declarations, index);
        TZrInt32 typeIndex;
        TZrUInt32 modifiers = 0U;

        if (symbol == ZR_NULL || symbol->declarationRange.source == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual(symbol->declarationRange.source, uri)) {
            continue;
        }

        typeIndex = ZrLanguageServer_LspSemanticToken_TypeFromCanonicalSymbol(symbol);
        if (typeIndex != ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN) {
            if (symbol->role == ZR_SEMANTIC_REFERENCE_DECLARATION) {
                modifiers = ZR_LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION;
            }
            semantic_token_add_file_range_with_modifiers(state,
                                                         context,
                                                         entries,
                                                         uri,
                                                         symbol->declarationRange,
                                                         (TZrUInt32)typeIndex,
                                                         modifiers);
        }
    }

    ZrCore_Array_Free(state, &declarations);
}

static TZrBool semantic_token_is_meta_method(const TZrChar *text, TZrSize length) {
    return ZrLanguageServer_Lsp_IsKnownMetaMethodToken(text, length);
}

static TZrBool semantic_token_is_keyword_word(const TZrChar *text, TZrSize length) {
    static const TZrChar *const keywordWords[] = {
        "let",       "var",       "fn",       "ref",       "in",       "out",
        "scoped",    "readonly",  "resource", "property", "init",     "own",
        "move",      "share",     "degrade",  "wake",     "intoGc",   "drop",
        "native",    "extern",    "async",    "await",
        "comptime",  "yield",     "import",   "typeid",   "typeof",   "loadModule",
        "loadPlugin", "pub",      "pri",      "pro",      "static",   "new",
        "class",
        "struct",    "interface", "enum",     "union",    "module",
        "if",        "else",      "switch",   "case",     "default",  "while",
        "for",       "break",     "continue", "return",   "try",      "catch",
        "finally",   "throw",     "super",    "get",      "set",
        "where"
    };

    for (TZrSize index = 0; index < sizeof(keywordWords) / sizeof(keywordWords[0]); index++) {
        TZrSize keywordLength = strlen(keywordWords[index]);

        if (length == keywordLength && memcmp(text, keywordWords[index], length) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
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
        if (current == '"' || current == '\'' || current == '`') {
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
        if ((current == '%' || current == '$') &&
            offset + 1 < contentLength && semantic_token_is_identifier_start(content[offset + 1])) {
            /* Do not classify a current identifier embedded in invalid removed-prefix syntax. */
            offset += 2;
            character += 2;
            while (offset < contentLength && semantic_token_is_identifier_char(content[offset])) {
                offset++;
                character++;
            }
            continue;
        }
        if (current == '#') {
            TZrSize start = offset;

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
                semantic_token_add_utf16_span(state,
                                              entries,
                                              content,
                                              contentLength,
                                              start,
                                              offset,
                                              ZR_LSP_SEMANTIC_TOKEN_DECORATOR);
            }
            continue;
        }
        if (current == '@') {
            TZrSize start = offset;

            offset++;
            character++;
            while (offset < contentLength && semantic_token_is_identifier_char(content[offset])) {
                offset++;
                character++;
            }
            if (semantic_token_is_meta_method(content + start, offset - start)) {
                semantic_token_add_utf16_span(state,
                                              entries,
                                              content,
                                              contentLength,
                                              start,
                                              offset,
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
            SZrLspPosition startPosition;
            TZrSize length;
            TZrSize previous = start;
            SZrLspImportBinding *binding;
            TZrInt32 canonicalType;
            TZrUInt32 canonicalModifiers = 0U;

            while (offset < contentLength && semantic_token_is_identifier_char(content[offset])) {
                offset++;
                character++;
            }
            length = offset - start;
            startPosition = ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(content,
                                                                                        contentLength,
                                                                                        start);
            startLine = (TZrUInt32)startPosition.line;
            startCharacter = (TZrUInt32)startPosition.character;

            if (semantic_token_is_keyword_word(content + start, length)) {
                semantic_token_add_utf16_span(state,
                                              entries,
                                              content,
                                              contentLength,
                                              start,
                                              offset,
                                              ZR_LSP_SEMANTIC_TOKEN_KEYWORD);
            } else {
                canonicalType = ZrLanguageServer_LspSemanticToken_ResolveCanonical(
                    analyzer,
                    uri,
                    start,
                    length,
                    startLine,
                    startCharacter,
                    &canonicalModifiers);
                if (canonicalType >= 0) {
                    semantic_token_add_utf16_span_with_modifiers(state,
                                                                 entries,
                                                                 content,
                                                                 contentLength,
                                                                 start,
                                                                 offset,
                                                                 (TZrUInt32)canonicalType,
                                                                 canonicalModifiers);
                }
            }
            semantic_token_append_import_alias_binding(state,
                                                       bindings,
                                                       uri,
                                                       content,
                                                       contentLength,
                                                       start,
                                                       length,
                                                       startLine,
                                                       startCharacter);

            while (previous > 0 && isspace((unsigned char)content[previous - 1])) {
                previous--;
            }
            if (previous > 0 && content[previous - 1] == '.') {
                TZrUInt32 tokenModifiers = 0U;
                TZrInt32 tokenType = ZrLanguageServer_LspSemanticToken_ResolveCanonical(
                    analyzer,
                    uri,
                    start,
                    length,
                    startLine,
                    startCharacter,
                    &tokenModifiers);
                if (tokenType >= 0) {
                    semantic_token_add_utf16_span_with_modifiers(state,
                                                                 entries,
                                                                 content,
                                                                 contentLength,
                                                                 start,
                                                                 offset,
                                                                 (TZrUInt32)tokenType,
                                                                 tokenModifiers);
                }
                continue;
            }

            binding = semantic_token_find_import_binding(bindings, content + start, length);
            if (binding == ZR_NULL) {
                TZrUInt32 tokenModifiers = 0U;
                TZrInt32 tokenType = ZrLanguageServer_LspSemanticToken_ResolveCanonical(
                    analyzer,
                    uri,
                    start,
                    length,
                    startLine,
                    startCharacter,
                    &tokenModifiers);
                if (tokenType >= 0) {
                    semantic_token_add_utf16_span_with_modifiers(state,
                                                                 entries,
                                                                 content,
                                                                 contentLength,
                                                                 start,
                                                                 offset,
                                                                 (TZrUInt32)tokenType,
                                                                 tokenModifiers);
                }
                continue;
            }

            semantic_token_add_utf16_span(state,
                                          entries,
                                          content,
                                          contentLength,
                                          start,
                                          offset,
                                          ZR_LSP_SEMANTIC_TOKEN_NAMESPACE);

            {
                SZrString *currentModuleName = binding->moduleName;
                TZrSize chainOffset = offset;
                TZrUInt32 chainCharacter = character;

                while (chainOffset < contentLength) {
                    TZrUInt32 segmentLine;
                    TZrUInt32 segmentCharacter;
                    TZrSize segmentStart;
                    TZrSize segmentLength;
                    TZrInt32 tokenType;
                    TZrUInt32 tokenModifiers = 0U;
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
                    {
                        SZrLspPosition segmentPosition =
                            ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(content,
                                                                                        contentLength,
                                                                                        segmentStart);
                        segmentLine = (TZrUInt32)segmentPosition.line;
                        segmentCharacter = (TZrUInt32)segmentPosition.character;
                    }

                    tokenType = ZrLanguageServer_LspSemanticToken_ResolveCanonical(
                        analyzer,
                        uri,
                        segmentStart,
                        segmentLength,
                        segmentLine,
                        segmentCharacter,
                        &tokenModifiers);
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
                    if (semantic_token_should_prefer_chain_fallback(tokenType, fallbackTokenType)) {
                        tokenType = fallbackTokenType;
                    }
                    if (tokenType >= 0) {
                        semantic_token_add_utf16_span_with_modifiers(state,
                                                                     entries,
                                                                     content,
                                                                     contentLength,
                                                                     segmentStart,
                                                                     segmentStart + segmentLength,
                                                                     (TZrUInt32)tokenType,
                                                                     tokenModifiers);
                    }
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
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrSemanticAnalyzer *analyzer;
    SZrLspProjectIndex *projectIndex;
    SZrArray entries;
    SZrArray bindings;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
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
        semantic_token_add_symbol_tokens(state, context, analyzer, uri, &entries);
    }

    semantic_token_scan_source(state,
                               context,
                               uri,
                               snapshot.content,
                               snapshot.contentLength,
                               analyzer,
                               &bindings,
                               projectIndex,
                               &entries);
    ZrLanguageServer_LspSemanticTokenEntries_AppendEncoded(state, &entries, result);

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    ZrCore_Array_Free(state, &entries);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

TZrSize ZrLanguageServer_Lsp_SemanticTokenTypeCount(void) {
    return sizeof(g_semanticTokenTypeNames) / sizeof(g_semanticTokenTypeNames[0]);
}

const TZrChar *ZrLanguageServer_Lsp_SemanticTokenTypeName(TZrSize index) {
    return index < ZrLanguageServer_Lsp_SemanticTokenTypeCount() ? g_semanticTokenTypeNames[index] : ZR_NULL;
}
