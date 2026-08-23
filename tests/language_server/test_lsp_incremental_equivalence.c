// Differential contract for declaration-scoped incremental parsing.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_language_server/lsp_interface.h"
#include "interface/lsp_interface_internal.h"
#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_language_server/symbol_table.h"

#define TEST_JSON_CAPACITY 16384U

static int g_failures = 0;

typedef struct SZrTestJsonBuffer {
    TZrChar content[TEST_JSON_CAPACITY];
    TZrSize length;
    TZrBool isValid;
} SZrTestJsonBuffer;

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0U) {
        free(pointer);
        return ZR_NULL;
    }
    return pointer == ZR_NULL ? malloc(newSize) : realloc(pointer, newSize);
}

static void check(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        g_failures++;
    } else {
        printf("PASS: %s\n", message);
    }
}

static SZrString *test_string(SZrState *state, const TZrChar *text) {
    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static TZrBool strings_equal(const SZrString *left, const SZrString *right) {
    const TZrChar *leftText = left == ZR_NULL ? ZR_NULL : ZrCore_String_GetNativeString(left);
    const TZrChar *rightText = right == ZR_NULL ? ZR_NULL : ZrCore_String_GetNativeString(right);

    if (leftText == ZR_NULL || rightText == ZR_NULL) {
        return leftText == rightText;
    }
    return strcmp(leftText, rightText) == 0;
}

static TZrBool positions_equal(SZrFilePosition left, SZrFilePosition right) {
    return left.offset == right.offset && left.line == right.line && left.column == right.column;
}

static TZrBool ranges_equal(SZrFileRange left, SZrFileRange right) {
    return positions_equal(left.start, right.start) && positions_equal(left.end, right.end) &&
           strings_equal(left.source, right.source);
}

static TZrBool identifiers_equal(const SZrIdentifier *left, const SZrIdentifier *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    return left->isMoveBinding == right->isMoveBinding && strings_equal(left->name, right->name);
}

static TZrBool ast_nodes_equal(const SZrAstNode *left, const SZrAstNode *right,
                               TZrSize depth);

static TZrBool types_equal(const SZrType *left, const SZrType *right, TZrSize depth) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    if (depth > 16U || left->dimensions != right->dimensions ||
        left->ownershipQualifier != right->ownershipQualifier ||
        left->referenceAccess != right->referenceAccess ||
        left->isScopedReference != right->isScopedReference ||
        left->isReadonlyView != right->isReadonlyView ||
        left->isDecoratorPseudoType != right->isDecoratorPseudoType ||
        left->isImplicitBuiltinType != right->isImplicitBuiltinType ||
        left->arrayFixedSize != right->arrayFixedSize ||
        left->arrayMinSize != right->arrayMinSize || left->arrayMaxSize != right->arrayMaxSize ||
        left->hasArraySizeConstraint != right->hasArraySizeConstraint) {
        return ZR_FALSE;
    }
    return ast_nodes_equal(left->name, right->name, depth + 1U) &&
           types_equal(left->subType, right->subType, depth + 1U) &&
           ast_nodes_equal(left->arraySizeExpression, right->arraySizeExpression, depth + 1U);
}

static TZrBool ast_node_arrays_equal(const SZrAstNodeArray *left,
                                     const SZrAstNodeArray *right,
                                     TZrSize depth) {
    TZrSize index;

    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    if (left->count != right->count) {
        return ZR_FALSE;
    }
    for (index = 0U; index < left->count; ++index) {
        if (!ast_nodes_equal(left->nodes[index], right->nodes[index], depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool ast_nodes_equal(const SZrAstNode *left, const SZrAstNode *right,
                               TZrSize depth) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    if (depth > 32U || left->type != right->type || !ranges_equal(left->location, right->location)) {
        return ZR_FALSE;
    }

    switch (left->type) {
        case ZR_AST_SCRIPT:
            return ast_nodes_equal(left->data.script.moduleName, right->data.script.moduleName,
                                   depth + 1U) &&
                   ast_node_arrays_equal(left->data.script.statements,
                                         right->data.script.statements,
                                         depth + 1U);
        case ZR_AST_VARIABLE_DECLARATION:
            return left->data.variableDeclaration.accessModifier ==
                           right->data.variableDeclaration.accessModifier &&
                   left->data.variableDeclaration.isConst == right->data.variableDeclaration.isConst &&
                   ast_nodes_equal(left->data.variableDeclaration.pattern,
                                   right->data.variableDeclaration.pattern,
                                   depth + 1U) &&
                   ast_nodes_equal(left->data.variableDeclaration.value,
                                   right->data.variableDeclaration.value,
                                   depth + 1U) &&
                   types_equal(left->data.variableDeclaration.typeInfo,
                               right->data.variableDeclaration.typeInfo,
                               depth + 1U);
        case ZR_AST_FUNCTION_DECLARATION:
            return identifiers_equal(left->data.functionDeclaration.name,
                                     right->data.functionDeclaration.name) &&
                   ranges_equal(left->data.functionDeclaration.nameLocation,
                                right->data.functionDeclaration.nameLocation) &&
                   left->data.functionDeclaration.generic == right->data.functionDeclaration.generic &&
                   ast_node_arrays_equal(left->data.functionDeclaration.params,
                                         right->data.functionDeclaration.params,
                                         depth + 1U) &&
                   left->data.functionDeclaration.args == right->data.functionDeclaration.args &&
                   types_equal(left->data.functionDeclaration.returnType,
                               right->data.functionDeclaration.returnType,
                               depth + 1U) &&
                   ast_nodes_equal(left->data.functionDeclaration.body,
                                   right->data.functionDeclaration.body,
                                   depth + 1U) &&
                   ast_node_arrays_equal(left->data.functionDeclaration.decorators,
                                         right->data.functionDeclaration.decorators,
                                         depth + 1U) &&
                   ranges_equal(left->data.functionDeclaration.fnKeywordLocation,
                                right->data.functionDeclaration.fnKeywordLocation) &&
                   ranges_equal(left->data.functionDeclaration.returnDelimiterLocation,
                                right->data.functionDeclaration.returnDelimiterLocation) &&
                   left->data.functionDeclaration.accessModifier ==
                           right->data.functionDeclaration.accessModifier &&
                   left->data.functionDeclaration.isAsync == right->data.functionDeclaration.isAsync;
        case ZR_AST_BLOCK:
            return left->data.block.isStatement == right->data.block.isStatement &&
                   ast_node_arrays_equal(left->data.block.body, right->data.block.body, depth + 1U);
        case ZR_AST_RETURN_STATEMENT:
            return left->data.returnStatement.isReferenceReturn ==
                           right->data.returnStatement.isReferenceReturn &&
                   ranges_equal(left->data.returnStatement.referenceLocation,
                                right->data.returnStatement.referenceLocation) &&
                   ast_nodes_equal(left->data.returnStatement.expr,
                                   right->data.returnStatement.expr,
                                   depth + 1U);
        case ZR_AST_IDENTIFIER_LITERAL:
            return left->data.identifier.isMoveBinding == right->data.identifier.isMoveBinding &&
                   strings_equal(left->data.identifier.name, right->data.identifier.name);
        case ZR_AST_INTEGER_LITERAL:
            return left->data.integerLiteral.value == right->data.integerLiteral.value &&
                   strings_equal(left->data.integerLiteral.literal, right->data.integerLiteral.literal);
        case ZR_AST_BINARY_EXPRESSION:
            return left->data.binaryExpression.op.op != ZR_NULL &&
                   right->data.binaryExpression.op.op != ZR_NULL &&
                   strcmp(left->data.binaryExpression.op.op,
                          right->data.binaryExpression.op.op) == 0 &&
                   ast_nodes_equal(left->data.binaryExpression.left,
                                   right->data.binaryExpression.left,
                                   depth + 1U) &&
                   ast_nodes_equal(left->data.binaryExpression.right,
                                   right->data.binaryExpression.right,
                                   depth + 1U);
        default:
            return ZR_FALSE;
    }
}

static void json_buffer_init(SZrTestJsonBuffer *buffer) {
    memset(buffer, 0, sizeof(*buffer));
    buffer->isValid = ZR_TRUE;
}

static void json_append_format(SZrTestJsonBuffer *buffer, const TZrChar *format, ...) {
    va_list args;
    int written;

    if (!buffer->isValid || buffer->length >= TEST_JSON_CAPACITY) {
        buffer->isValid = ZR_FALSE;
        return;
    }
    va_start(args, format);
    written = vsnprintf(buffer->content + buffer->length,
                        TEST_JSON_CAPACITY - buffer->length,
                        format,
                        args);
    va_end(args);
    if (written < 0 || (TZrSize)written >= TEST_JSON_CAPACITY - buffer->length) {
        buffer->isValid = ZR_FALSE;
        return;
    }
    buffer->length += (TZrSize)written;
}

static void json_append_string(SZrTestJsonBuffer *buffer, const SZrString *value) {
    const TZrChar *text = value == ZR_NULL ? ZR_NULL : ZrCore_String_GetNativeString(value);
    const unsigned char *cursor = (const unsigned char *)(text == ZR_NULL ? "" : text);

    json_append_format(buffer, "\"");
    while (buffer->isValid && *cursor != '\0') {
        switch (*cursor) {
            case '\"':
                json_append_format(buffer, "\\\"");
                break;
            case '\\':
                json_append_format(buffer, "\\\\");
                break;
            case '\n':
                json_append_format(buffer, "\\n");
                break;
            case '\r':
                json_append_format(buffer, "\\r");
                break;
            case '\t':
                json_append_format(buffer, "\\t");
                break;
            default:
                if (*cursor < 0x20U) {
                    json_append_format(buffer, "\\u%04x", (unsigned int)*cursor);
                } else {
                    json_append_format(buffer, "%c", (TZrChar)*cursor);
                }
                break;
        }
        ++cursor;
    }
    json_append_format(buffer, "\"");
}

static void json_append_range(SZrTestJsonBuffer *buffer, SZrLspRange range) {
    json_append_format(buffer,
                       "[%d,%d,%d,%d]",
                       range.start.line,
                       range.start.character,
                       range.end.line,
                       range.end.character);
}

static TZrBool capture_lsp_json(SZrState *state,
                                SZrLspContext *context,
                                SZrString *uri,
                                SZrTestJsonBuffer *buffer) {
    SZrArray diagnostics = {0};
    SZrArray symbols = {0};
    SZrArray tokens = {0};
    TZrSize index;
    TZrBool success;

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4U);
    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 4U);
    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 16U);
    success = ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) &&
              ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols) &&
              ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &tokens);
    if (!success) {
        ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
        ZrCore_Array_Free(state, &symbols);
        ZrCore_Array_Free(state, &tokens);
        return ZR_FALSE;
    }

    json_buffer_init(buffer);
    json_append_format(buffer, "{\"diagnostics\":[");
    for (index = 0U; index < diagnostics.length; ++index) {
        SZrLspDiagnostic *diagnostic =
                *(SZrLspDiagnostic **)ZrCore_Array_Get(&diagnostics, index);
        if (index != 0U) {
            json_append_format(buffer, ",");
        }
        json_append_format(buffer, "{\"range\":");
        json_append_range(buffer, diagnostic->range);
        json_append_format(buffer, ",\"severity\":%d,\"code\":", diagnostic->severity);
        json_append_string(buffer, diagnostic->code);
        json_append_format(buffer, ",\"message\":");
        json_append_string(buffer, diagnostic->message);
        json_append_format(buffer, "}");
    }
    json_append_format(buffer, "],\"symbols\":[");
    for (index = 0U; index < symbols.length; ++index) {
        SZrLspSymbolInformation *symbol =
                *(SZrLspSymbolInformation **)ZrCore_Array_Get(&symbols, index);
        if (index != 0U) {
            json_append_format(buffer, ",");
        }
        json_append_format(buffer, "{\"name\":");
        json_append_string(buffer, symbol->name);
        json_append_format(buffer, ",\"kind\":%d,\"range\":", symbol->kind);
        json_append_range(buffer, symbol->location.range);
        json_append_format(buffer, "}");
    }
    json_append_format(buffer, "],\"semanticTokens\":[");
    for (index = 0U; index < tokens.length; ++index) {
        if (index != 0U) {
            json_append_format(buffer, ",");
        }
        json_append_format(buffer,
                           "%u",
                           (unsigned int)*(TZrUInt32 *)ZrCore_Array_Get(&tokens, index));
    }
    json_append_format(buffer, "]}");

    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    ZrCore_Array_Free(state, &symbols);
    ZrCore_Array_Free(state, &tokens);
    return buffer->isValid;
}

static SZrSymbol *lookup_global_symbol(SZrState *state,
                                       SZrLspContext *context,
                                       SZrString *uri,
                                       const TZrChar *name) {
    SZrSemanticAnalyzer *analyzer;
    SZrString *symbolName;

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return ZR_NULL;
    }
    symbolName = test_string(state, name);
    if (symbolName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable,
                                                symbolName,
                                                analyzer->symbolTable->globalScope);
}

static void test_incremental_parse_matches_clean_full_parse(SZrState *state) {
    static const TZrChar *before =
            "fn alpha(): int { return 1; }\n"
            "fn beta(): int { return 2; }\n";
    static const TZrChar *after =
            "fn alpha(): int { return 9; }\n"
            "fn beta(): int { return 2; }\n";
    SZrLspContext *incrementalContext = ZrLanguageServer_LspContext_New(state);
    SZrLspContext *cleanContext = ZrLanguageServer_LspContext_New(state);
    SZrString *incrementalUri = test_string(state, "file:///incremental-equivalence.zr");
    SZrString *cleanUri = test_string(state, "file:///incremental-equivalence.zr");
    SZrFileVersion *incrementalVersion;
    SZrFileVersion *cleanVersion;
    SZrTestJsonBuffer incrementalJson;
    SZrTestJsonBuffer cleanJson;
    SZrSymbol *incrementalAlpha;
    SZrSymbol *incrementalBeta;
    SZrSymbol *cleanAlpha;
    SZrSymbol *cleanBeta;

    check(incrementalContext != ZR_NULL && cleanContext != ZR_NULL &&
                  incrementalUri != ZR_NULL && cleanUri != ZR_NULL,
          "incremental equivalence setup must allocate contexts and URIs");
    if (incrementalContext == ZR_NULL || cleanContext == ZR_NULL ||
        incrementalUri == ZR_NULL || cleanUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, incrementalContext);
        ZrLanguageServer_LspContext_Free(state, cleanContext);
        return;
    }

    check(ZrLanguageServer_Lsp_UpdateDocument(state,
                                               incrementalContext,
                                               incrementalUri,
                                               before,
                                               strlen(before),
                                               1U),
          "incremental baseline document update must succeed");
    check(ZrLanguageServer_Lsp_UpdateDocument(state,
                                               incrementalContext,
                                               incrementalUri,
                                               after,
                                               strlen(after),
                                               2U),
          "equal-length declaration edit must succeed");
    check(ZrLanguageServer_Lsp_UpdateDocument(state,
                                               cleanContext,
                                               cleanUri,
                                               after,
                                               strlen(after),
                                               1U),
          "clean full-parse document update must succeed");

    incrementalVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(
            incrementalContext->parser, incrementalUri);
    cleanVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(cleanContext->parser, cleanUri);
    check(incrementalVersion != ZR_NULL && cleanVersion != ZR_NULL,
          "both contexts must expose their current file versions");
    if (incrementalVersion == ZR_NULL || cleanVersion == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, incrementalContext);
        ZrLanguageServer_LspContext_Free(state, cleanContext);
        return;
    }

    check(incrementalVersion->lastParseMode == ZR_INCREMENTAL_PARSE_MODE_DECLARATION_REPARSE,
          "equal-length declaration edit must use declaration reparse");
    check(cleanVersion->lastParseMode == ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE,
          "clean comparison context must use full reparse");
    check(ast_nodes_equal(incrementalVersion->ast, cleanVersion->ast, 0U),
          "incremental and clean parses must produce equal AST shape, values, and ranges");

    check(capture_lsp_json(state, incrementalContext, incrementalUri, &incrementalJson) &&
                  capture_lsp_json(state, cleanContext, cleanUri, &cleanJson),
          "both contexts must produce serializable public LSP responses");
    check(incrementalJson.isValid && cleanJson.isValid &&
                  strcmp(incrementalJson.content, cleanJson.content) == 0,
          "incremental and clean contexts must produce equal diagnostic, symbol, and token JSON");

    incrementalAlpha = lookup_global_symbol(state, incrementalContext, incrementalUri, "alpha");
    incrementalBeta = lookup_global_symbol(state, incrementalContext, incrementalUri, "beta");
    cleanAlpha = lookup_global_symbol(state, cleanContext, cleanUri, "alpha");
    cleanBeta = lookup_global_symbol(state, cleanContext, cleanUri, "beta");
    check(incrementalAlpha != ZR_NULL && incrementalBeta != ZR_NULL &&
                  cleanAlpha != ZR_NULL && cleanBeta != ZR_NULL,
          "both analyses must publish alpha and beta symbols");
    if (incrementalAlpha != ZR_NULL && incrementalBeta != ZR_NULL &&
        cleanAlpha != ZR_NULL && cleanBeta != ZR_NULL) {
        check(incrementalAlpha->semanticId != ZR_SEMANTIC_ID_INVALID &&
                      incrementalBeta->semanticId != ZR_SEMANTIC_ID_INVALID &&
                      cleanAlpha->semanticId != ZR_SEMANTIC_ID_INVALID &&
                      cleanBeta->semanticId != ZR_SEMANTIC_ID_INVALID &&
                      incrementalAlpha->semanticId != incrementalBeta->semanticId &&
                      cleanAlpha->semanticId != cleanBeta->semanticId,
              "symbol identities must remain valid and distinct within each analysis");
        check((incrementalAlpha->semanticTypeId == incrementalBeta->semanticTypeId) ==
                      (cleanAlpha->semanticTypeId == cleanBeta->semanticTypeId) &&
                      incrementalAlpha->semanticTypeId != ZR_SEMANTIC_ID_INVALID &&
                      cleanAlpha->semanticTypeId != ZR_SEMANTIC_ID_INVALID,
              "TypeId relations must match the clean full analysis");
    }

    ZrLanguageServer_LspContext_Free(state, incrementalContext);
    ZrLanguageServer_LspContext_Free(state, cleanContext);
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    SZrState *state;

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        fprintf(stderr, "unable to create test runtime\n");
        return 1;
    }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    test_incremental_parse_matches_clean_full_parse(state);

    ZrCore_GlobalState_Free(global);
    printf("LSP incremental equivalence: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
