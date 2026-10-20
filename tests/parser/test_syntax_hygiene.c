#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_stream.h"
#include "zr_vm_parser/type_system.h"
#include "compiler/compiler_internal.h"

static SZrState *g_state;
static SZrSemanticContext *g_semantic;
static SZrSyntaxContextTable *g_contextTable;

static SZrFileRange syntax_hygiene_range(TZrSize start, TZrSize end) {
    SZrFileRange range;
    memset(&range, 0, sizeof(range));
    range.start.offset = start;
    range.end.offset = end;
    return range;
}

static TZrBool syntax_hygiene_make_contexts(
        TZrUInt64 stableIdentity,
        SZrSyntaxContext *outCallSite,
        SZrSyntaxContext *outDefSite) {
    SZrSyntaxExpansionInfo expansion;
    TZrSyntaxExpansionId expansionId;

    if (outCallSite == ZR_NULL || outDefSite == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&expansion, 0, sizeof(expansion));
    expansion.stableIdentity = stableIdentity;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    if (!ZrParser_SyntaxContextTable_RegisterExpansion(
                g_contextTable, &expansion, &expansionId)) {
        return ZR_FALSE;
    }
    *outCallSite = ZrParser_SyntaxContext_CallSite(g_contextTable, expansionId);
    *outDefSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    return ZrParser_SyntaxContext_IsValid(*outCallSite) &&
           ZrParser_SyntaxContext_IsValid(*outDefSite);
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    g_semantic = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_semantic);
}

void tearDown(void) {
    if (g_semantic != ZR_NULL) {
        ZrParser_SemanticContext_Free(g_semantic);
        g_semantic = ZR_NULL;
    }
    if (g_contextTable != ZR_NULL) {
        ZrParser_SyntaxContextTable_Release(g_contextTable);
        g_contextTable = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void test_marked_ast_identifier_keeps_context_and_spelling(void) {
    static const TZrChar source[] = "let x = x;";
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *pattern;
    SZrAstNode *value;
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    SZrIdentifier clonedIdentifier;

    sourceName = ZrCore_String_CreateFromNative(g_state, "syntax_hygiene.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);

    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    pattern = declaration->data.variableDeclaration.pattern;
    value = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, pattern->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, value->type);

    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(0x1001U, &callSite, &defSite));
    pattern->data.identifier.syntaxContext = defSite;
    value->data.identifier.syntaxContext = callSite;
    clonedIdentifier = pattern->data.identifier;

    TEST_ASSERT_EQUAL_STRING(
            ZrCore_String_GetNativeString(pattern->data.identifier.name),
            ZrCore_String_GetNativeString(value->data.identifier.name));
    TEST_ASSERT_FALSE(
            ZrParser_SyntaxContext_Equal(
                    pattern->data.identifier.syntaxContext,
                    value->data.identifier.syntaxContext));
    TEST_ASSERT_TRUE(
            ZrParser_SyntaxContext_Equal(
                    clonedIdentifier.syntaxContext,
                    pattern->data.identifier.syntaxContext));

    ZrParser_Ast_Free(g_state, ast);
}

static void test_binding_key_selects_same_spelling_by_context_and_scope(void) {
    SZrString *name;
    SZrSemanticScopeFact rootScope;
    SZrSemanticScopeFact nestedScope;
    TZrSemanticScopeId rootScopeId;
    TZrSemanticScopeId nestedScopeId;
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    TZrSymbolId callSiteSymbolId;
    TZrSymbolId defSiteSymbolId;
    SZrSemanticVisibleSymbolFact visible;
    SZrSemanticBindingKey key;
    const SZrSemanticSymbolRecord *resolved;

    name = ZrCore_String_CreateFromNative(g_state, "x");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(0x1002U, &callSite, &defSite));

    memset(&rootScope, 0, sizeof(rootScope));
    rootScope.parentScopeId = ZR_SEMANTIC_ID_INVALID;
    rootScope.kind = ZR_SEMANTIC_SCOPE_KIND_MODULE;
    rootScope.range = syntax_hygiene_range(0U, 10U);
    rootScopeId = ZrParser_Semantic_PublishScopeFact(g_semantic, &rootScope);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, rootScopeId);

    memset(&nestedScope, 0, sizeof(nestedScope));
    nestedScope.parentScopeId = rootScopeId;
    nestedScope.kind = ZR_SEMANTIC_SCOPE_KIND_BLOCK;
    nestedScope.range = syntax_hygiene_range(0U, 10U);
    nestedScopeId = ZrParser_Semantic_PublishScopeFact(g_semantic, &nestedScope);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, nestedScopeId);

    callSiteSymbolId = ZrParser_Semantic_RegisterSymbolWithContext(
            g_semantic,
            name,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            syntax_hygiene_range(1U, 2U),
            callSite);
    defSiteSymbolId = ZrParser_Semantic_RegisterSymbolWithContext(
            g_semantic,
            name,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            syntax_hygiene_range(3U, 4U),
            defSite);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, callSiteSymbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, defSiteSymbolId);
    TEST_ASSERT_NOT_EQUAL(callSiteSymbolId, defSiteSymbolId);

    memset(&visible, 0, sizeof(visible));
    visible.scopeId = rootScopeId;
    visible.symbolId = callSiteSymbolId;
    visible.access = ZR_ACCESS_PUBLIC;
    visible.declarationOrder = 1U;
    visible.declarationRange = syntax_hygiene_range(1U, 2U);
    visible.isAccessible = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Semantic_PublishVisibleSymbolFact(g_semantic, &visible));

    visible.scopeId = nestedScopeId;
    visible.symbolId = defSiteSymbolId;
    visible.declarationOrder = 1U;
    visible.declarationRange = syntax_hygiene_range(3U, 4U);
    TEST_ASSERT_TRUE(ZrParser_Semantic_PublishVisibleSymbolFact(g_semantic, &visible));

    key.name = name;
    key.syntaxContext = defSite;
    resolved = ZrParser_Semantic_FindSymbolByBindingKey(
            g_semantic,
            nestedScopeId,
            &key,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_UINT32(defSiteSymbolId, resolved->id);
    TEST_ASSERT_EQUAL_STRING("x", ZrCore_String_GetNativeString(resolved->name));

    key.syntaxContext = callSite;
    resolved = ZrParser_Semantic_FindSymbolByBindingKey(
            g_semantic,
            nestedScopeId,
            &key,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_UINT32(callSiteSymbolId, resolved->id);
}

static void test_marked_ast_types_and_closure_capture_stay_context_distinct(void) {
    static const TZrChar source[] = "let x = 1; x = 2;";
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *pattern;
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    SZrTypeEnvironment *typeEnv;
    SZrTypeEnvironment *closureEnv;
    SZrString *name;
    SZrInferredType intType;
    SZrInferredType stringType;
    SZrInferredType resolved;
    SZrFileRange declarationRange;
    TZrSymbolId symbolId;
    const SZrTypeBinding *binding;

    sourceName = ZrCore_String_CreateFromNative(g_state, "marked_types.zr");
    ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(0x1003U, &callSite, &defSite));

    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    pattern = declaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(pattern);
    pattern->data.identifier.syntaxContext = defSite;
    name = pattern->data.identifier.name;
    declarationRange = declaration->location;

    typeEnv = ZrParser_TypeEnvironment_New(g_state);
    closureEnv = ZrParser_TypeEnvironment_New(g_state);
    TEST_ASSERT_NOT_NULL(typeEnv);
    TEST_ASSERT_NOT_NULL(closureEnv);
    typeEnv->semanticContext = g_semantic;
    closureEnv->semanticContext = g_semantic;

    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(g_state, &stringType, ZR_VALUE_TYPE_STRING);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableExWithContext(
            g_state, typeEnv, name, &intType, declaration, declarationRange, defSite));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableWithContext(
            g_state, typeEnv, name, &stringType, callSite));

    ZrParser_InferredType_Init(g_state, &resolved, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupVariableWithContext(
            g_state, typeEnv, name, defSite, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resolved.baseType);
    ZrParser_InferredType_Free(g_state, &resolved);
    ZrParser_InferredType_Init(g_state, &resolved, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupVariableWithContext(
            g_state, typeEnv, name, callSite, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, resolved.baseType);
    ZrParser_InferredType_Free(g_state, &resolved);

    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            typeEnv, name, defSite);
    TEST_ASSERT_NOT_NULL(binding);
    symbolId = binding->symbolId;
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, symbolId);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterClosureCapture(
            g_state,
            closureEnv,
            name,
            &intType,
            symbolId,
            binding->typeId,
            declarationRange,
            0U,
            1U));
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            closureEnv, name, defSite);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(symbolId, binding->symbolId);

    ZrParser_InferredType_Free(g_state, &intType);
    ZrParser_InferredType_Free(g_state, &stringType);
    ZrParser_TypeEnvironment_Free(g_state, closureEnv);
    ZrParser_TypeEnvironment_Free(g_state, typeEnv);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_marked_ast_runtime_shadow_and_closure_capture(void) {
    static const TZrChar source[] =
            "fn outer() {\n"
            "    var x = 1;\n"
            "    var x = 2;\n"
            "    fn inner() { return x; }\n"
            "    return inner() + x;\n"
            "}\n"
            "return outer();";
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *outer;
    SZrAstNode *outerDefSiteX;
    SZrAstNode *outerCallSiteX;
    SZrAstNode *inner;
    SZrAstNode *innerReturn;
    SZrAstNode *outerReturn;
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "marked_runtime.zr");
    ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(0x1004U, &callSite, &defSite));

    outer = ast->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, outer->type);
    outerDefSiteX = outer->data.functionDeclaration.body->data.block.body->nodes[0]
                            ->data.variableDeclaration.pattern;
    outerCallSiteX = outer->data.functionDeclaration.body->data.block.body->nodes[1]
                             ->data.variableDeclaration.pattern;
    inner = outer->data.functionDeclaration.body->data.block.body->nodes[2];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, inner->type);
    innerReturn = inner->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, innerReturn->type);
    TEST_ASSERT_NOT_NULL(innerReturn->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_IDENTIFIER_LITERAL,
            innerReturn->data.returnStatement.expr->type);
    outerReturn = outer->data.functionDeclaration.body->data.block.body->nodes[3];
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, outerReturn->type);
    TEST_ASSERT_NOT_NULL(outerReturn->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_BINARY_EXPRESSION,
            outerReturn->data.returnStatement.expr->type);
    TEST_ASSERT_NOT_NULL(outerReturn->data.returnStatement.expr->data.binaryExpression.right);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_IDENTIFIER_LITERAL,
            outerReturn->data.returnStatement.expr->data.binaryExpression.right->type);

    outerDefSiteX->data.identifier.syntaxContext = defSite;
    outerCallSiteX->data.identifier.syntaxContext = callSite;
    innerReturn->data.returnStatement.expr->data.identifier.syntaxContext = defSite;
    outerReturn->data.returnStatement.expr->data.binaryExpression.right
            ->data.identifier.syntaxContext = callSite;

    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);
    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

/* Rebuild the fixture through native token constructors so contexts are input
 * to the parser. Its plain identifiers and integers have no decoded escapes. */
static void syntax_hygiene_append_runtime_tree(
        SZrTokenStream *destination,
        TZrTokenSourceId destinationSource,
        const SZrTokenStream *source,
        const SZrTokenTree *tree,
        SZrSyntaxContext callSite,
        SZrSyntaxContext defSite) {
    SZrTokenSpan span = ZrParser_TokenTree_Span(tree);
    SZrSyntaxContext context = ZrParser_TokenTree_Context(tree);
    TZrSize index;

    span.sourceId = destinationSource;
    if (ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_GROUP) {
        SZrTokenStream *childrenStream = ZrParser_TokenStream_Create(
                g_state->global, g_contextTable, ZR_NULL);
        TZrTokenSourceId childrenSource;
        TZrSize sourceLength;
        TZrSize sourceNameLength;
        const TZrChar *sourceBytes = ZrParser_TokenStream_SourceBytes(
                source, ZrParser_TokenTree_Span(tree).sourceId, &sourceLength);
        const TZrChar *sourceName = ZrParser_TokenStream_SourceName(
                source, ZrParser_TokenTree_Span(tree).sourceId, &sourceNameLength);
        const SZrTokenTree *children[64];
        TZrSize childCount = ZrParser_TokenTree_GroupChildCount(tree);
        SZrTokenSpan open = ZrParser_TokenTree_GroupOpenSpan(tree);
        SZrTokenSpan close = ZrParser_TokenTree_GroupCloseSpan(tree);

        TEST_ASSERT_NOT_NULL(childrenStream);
        TEST_ASSERT_TRUE(childCount <= sizeof(children) / sizeof(children[0]));
        TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
                childrenStream, sourceName, sourceNameLength,
                sourceBytes, sourceLength, &childrenSource));
        for (TZrSize child = 0U; child < childCount; child++) {
            syntax_hygiene_append_runtime_tree(
                    childrenStream, childrenSource, source,
                    ZrParser_TokenTree_GroupChildAt(tree, child), callSite, defSite);
            children[child] = ZrParser_TokenStream_At(childrenStream, child);
            TEST_ASSERT_NOT_NULL(children[child]);
        }
        TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(childrenStream));
        open.sourceId = destinationSource;
        close.sourceId = destinationSource;
        TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendGroup(
                destination, ZrParser_TokenTree_GroupDelimiter(tree), open, close,
                children, childCount, span, context,
                ZR_NULL, 0U, ZR_NULL, 0U, &index));
        ZrParser_TokenStream_Release(childrenStream);
    } else {
        SZrTokenNativeSemInfo semInfo;
        SZrTokenText raw = ZrParser_TokenTree_Text(tree);
        const TZrChar *rawBytes = ZrParser_TokenStream_TextBytes(source, raw);

        TEST_ASSERT_TRUE(ZrParser_TokenTree_NativeSemInfo(tree, &semInfo));
        TEST_ASSERT_NOT_NULL(rawBytes);
        raw.sourceId = destinationSource;
        if (semInfo.text.sourceId != ZR_TOKEN_SOURCE_INVALID) {
            semInfo.text = raw;
        }
        if (semInfo.token == ZR_TK_IDENTIFIER &&
            raw.length == 1U && rawBytes[0] == 'x') {
            TEST_ASSERT_TRUE(span.startLine >= 2 && span.startLine <= 5);
            context = span.startLine == 2 || span.startLine == 4 ? defSite : callSite;
        }
        switch (ZrParser_TokenTree_Kind(tree)) {
            case ZR_TOKEN_TREE_IDENT:
                TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
                        destination, semInfo, raw, span, context,
                        ZR_NULL, 0U, ZR_NULL, 0U, &index));
                break;
            case ZR_TOKEN_TREE_LITERAL:
                TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendLiteral(
                        destination, semInfo, raw, span, context,
                        ZR_NULL, 0U, ZR_NULL, 0U, &index));
                break;
            case ZR_TOKEN_TREE_PUNCT:
                TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
                        destination, semInfo, raw,
                        ZrParser_TokenTree_PunctCodepoint(tree),
                        ZrParser_TokenTree_PunctSpacing(tree), span, context,
                        ZR_NULL, 0U, ZR_NULL, 0U, &index));
                break;
            default:
                TEST_FAIL_MESSAGE("Unexpected runtime fixture token kind");
                break;
        }
    }
}

static void test_native_tokens_runtime_shadow_and_closure_capture(void) {
    static const TZrChar source[] =
            "fn outer() {\n"
            "    var x = 1;\n"
            "    var x = 2;\n"
            "    fn inner() { return x; }\n"
            "    return inner() + x;\n"
            "}\n"
            "return outer();";
    static const TZrChar sourceName[] = "native_hygiene_runtime.zr";
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    SZrTokenStream *sourceStream = ZR_NULL;
    SZrTokenStream *markedStream;
    TZrTokenSourceId markedSource;
    SZrParserState parser;
    SZrAstNode *ast;
    SZrAstNodeArray *body;
    SZrAstNode *innerReturn;
    SZrAstNode *outerReturn;
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(0x1005U, &callSite, &defSite));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(callSite, defSite));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, g_contextTable, sourceName, sizeof(sourceName) - 1U,
            source, sizeof(source) - 1U, ZR_NULL, &sourceStream));
    markedStream = ZrParser_TokenStream_Create(g_state->global, g_contextTable, ZR_NULL);
    TEST_ASSERT_NOT_NULL(markedStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            markedStream, sourceName, sizeof(sourceName) - 1U,
            source, sizeof(source) - 1U, &markedSource));
    for (TZrSize index = 0U; index < ZrParser_TokenStream_Count(sourceStream); index++) {
        syntax_hygiene_append_runtime_tree(
                markedStream, markedSource, sourceStream,
                ZrParser_TokenStream_At(sourceStream, index), callSite, defSite);
    }
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(markedStream));
    ZrParser_TokenStream_Release(sourceStream);

    ZrParser_State_InitWithTokenStream(&parser, g_state, markedStream, ZR_NULL);
    ast = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(parser.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_DECLARATION, ast->data.script.statements->nodes[0]->type);
    body = ast->data.script.statements->nodes[0]
                   ->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_UINT(4U, body->count);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            defSite, body->nodes[0]->data.variableDeclaration.pattern
                             ->data.identifier.syntaxContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            callSite, body->nodes[1]->data.variableDeclaration.pattern
                              ->data.identifier.syntaxContext));
    innerReturn = body->nodes[2]->data.functionDeclaration.body->data.block.body->nodes[0];
    outerReturn = body->nodes[3];
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            defSite, innerReturn->data.returnStatement.expr->data.identifier.syntaxContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            callSite, outerReturn->data.returnStatement.expr->data.binaryExpression.right
                              ->data.identifier.syntaxContext));
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(markedStream);

    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);
    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

static void syntax_hygiene_assert_native_owner_capture_rejected(
        const TZrChar *sourceName,
        const TZrChar *source,
        TZrUInt64 stableIdentity) {
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    SZrTokenStream *sourceStream = ZR_NULL;
    SZrTokenStream *markedStream;
    TZrTokenSourceId markedSource;
    SZrParserState parser;
    SZrAstNode *ast;
    SZrAstNodeArray *body;
    SZrAstNode *markedBinding;
    SZrAstNode *rootBinding;
    SZrAstNode *capture;
    SZrCompilerState compiler;
    TZrChar diagnostic[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrBool rejected;
    TZrBool ownershipDiagnostic;

    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(stableIdentity, &callSite, &defSite));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(callSite, ZR_SYNTAX_NAMESPACE_LOCAL),
            ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(defSite, ZR_SYNTAX_NAMESPACE_LOCAL),
            ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_FromText(
            g_state, g_contextTable, sourceName, strlen(sourceName),
            source, strlen(source), ZR_NULL, &sourceStream));
    markedStream = ZrParser_TokenStream_Create(g_state->global, g_contextTable, ZR_NULL);
    TEST_ASSERT_NOT_NULL(markedStream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            markedStream, sourceName, strlen(sourceName), source, strlen(source),
            &markedSource));
    for (TZrSize index = 0U; index < ZrParser_TokenStream_Count(sourceStream); index++) {
        syntax_hygiene_append_runtime_tree(
                markedStream, markedSource, sourceStream,
                ZrParser_TokenStream_At(sourceStream, index), callSite, defSite);
    }
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(markedStream));
    ZrParser_TokenStream_Release(sourceStream);

    ZrParser_State_InitWithTokenStream(&parser, g_state, markedStream, ZR_NULL);
    ast = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(parser.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_EQUAL_UINT(3U, ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_DECLARATION, ast->data.script.statements->nodes[1]->type);
    body = ast->data.script.statements->nodes[1]
                   ->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_TRUE(body->count >= 4U);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->nodes[body->count - 4U]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->nodes[body->count - 3U]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, body->nodes[body->count - 2U]->type);
    markedBinding = body->nodes[body->count - 4U]->data.variableDeclaration.pattern;
    rootBinding = body->nodes[body->count - 3U]->data.variableDeclaration.pattern;
    capture = body->nodes[body->count - 2U]->data.functionDeclaration.body
                      ->data.block.body->nodes[0]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(markedBinding);
    TEST_ASSERT_NOT_NULL(rootBinding);
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, capture->type);
    TEST_ASSERT_EQUAL_STRING("x", ZrCore_String_GetNativeString(markedBinding->data.identifier.name));
    TEST_ASSERT_EQUAL_STRING("x", ZrCore_String_GetNativeString(rootBinding->data.identifier.name));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            defSite, markedBinding->data.identifier.syntaxContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            callSite, rootBinding->data.identifier.syntaxContext));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            defSite, capture->data.identifier.syntaxContext));
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(markedStream);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_script(&compiler, ast);
    rejected = compiler.hasError;
    ownershipDiagnostic = compiler.errorMessage != ZR_NULL &&
            strstr(compiler.errorMessage,
                   "Borrowed and loaned owners cannot escape through closure capture") != ZR_NULL;
    snprintf(diagnostic, sizeof(diagnostic), "%s",
             compiler.errorMessage != ZR_NULL ? compiler.errorMessage : "No compiler diagnostic");
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_TRUE_MESSAGE(
            rejected,
            "A def-site borrowed or loaned capture must not use the root integer binding's type");
    TEST_ASSERT_TRUE_MESSAGE(ownershipDiagnostic, diagnostic);
}

static void test_native_tokens_borrow_capture_rejects_root_type_shadow(void) {
    static const TZrChar source[] =
            "resource class Box {} fn outer() { var seed = own Box(); var owner = share(seed);\n"
            "    var x: ref readonly Box = ref owner;\n"
            "    var x = 1;\n"
            "    fn inner() { x; return 1; }\n"
            "    return inner;\n"
            "}\n"
            "return 0;";

    syntax_hygiene_assert_native_owner_capture_rejected(
            "native_borrow_hygiene.zr", source, 0x1006U);
}

static void test_native_tokens_loan_capture_rejects_root_type_shadow(void) {
    static const TZrChar source[] =
            "resource class Box {} fn outer() { var seed = own Box();\n"
            "    var x = ref seed;\n"
            "    var x = 1;\n"
            "    fn inner() { x; return 1; }\n"
            "    return inner;\n"
            "}\n"
            "return 0;";

    syntax_hygiene_assert_native_owner_capture_rejected(
            "native_loan_hygiene.zr", source, 0x1007U);
}

static SZrTokenSpan syntax_hygiene_single_line_span(
        TZrTokenSourceId sourceId, TZrSize start, TZrSize end) {
    SZrTokenSpan span = ZrParser_TokenSpan_None();
    span.sourceId = sourceId;
    span.startOffset = start;
    span.endOffset = end;
    span.startLine = 1;
    span.endLine = 1;
    span.startColumn = (TZrInt32)start + 1;
    span.endColumn = (TZrInt32)end + 1;
    return span;
}

static void test_native_template_interpolation_keeps_literal_context(void) {
    static const TZrChar source[] = "return `${x}`;";
    static const TZrChar sourceName[] = "native_template_hygiene.zr";
    SZrSyntaxContext callSite;
    SZrSyntaxContext defSite;
    SZrSyntaxContext root = ZrParser_SyntaxContext_TableRoot(g_contextTable);
    SZrSyntaxContext embeddedContext = ZrParser_SyntaxContext_Root();
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText raw;
    TZrSize index;
    SZrParserState parser;
    SZrAstNode *ast;
    SZrAstNode *expression;
    SZrAstNodeArray *segments;
    TZrSize interpolatedCount = 0U;

    TEST_ASSERT_TRUE(syntax_hygiene_make_contexts(0x1008U, &callSite, &defSite));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(root, defSite));
    stream = ZrParser_TokenStream_Create(g_state->global, g_contextTable, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AddSource(
            stream, sourceName, sizeof(sourceName) - 1U,
            source, sizeof(source) - 1U, &sourceId));

    semInfo.token = ZR_TK_RETURN;
    raw = (SZrTokenText){sourceId, 0U, 6U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendIdent(
            stream, semInfo, raw, syntax_hygiene_single_line_span(sourceId, 0U, 6U),
            root, ZR_NULL, 0U, ZR_NULL, 0U, &index));
    semInfo.token = ZR_TK_TEMPLATE_STRING;
    TEST_ASSERT_TRUE(ZrParser_TokenStream_CopyText(stream, "${x}", 4U, &semInfo.text));
    raw = (SZrTokenText){sourceId, 7U, 6U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendLiteral(
            stream, semInfo, raw, syntax_hygiene_single_line_span(sourceId, 7U, 13U),
            defSite, ZR_NULL, 0U, ZR_NULL, 0U, &index));
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.token = ZR_TK_SEMICOLON;
    raw = (SZrTokenText){sourceId, 13U, 1U};
    TEST_ASSERT_TRUE(ZrParser_TokenStream_AppendPunct(
            stream, semInfo, raw, ';', ZR_TOKEN_SPACING_ALONE,
            syntax_hygiene_single_line_span(sourceId, 13U, 14U),
            root, ZR_NULL, 0U, ZR_NULL, 0U, &index));
    TEST_ASSERT_TRUE(ZrParser_TokenStream_Seal(stream));

    ZrParser_State_InitWithTokenStream(&parser, g_state, stream, ZR_NULL);
    ast = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(parser.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, ast->data.script.statements->nodes[0]->type);
    expression = ast->data.script.statements->nodes[0]->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL, expression->type);
    segments = expression->data.templateStringLiteral.segments;
    TEST_ASSERT_NOT_NULL(segments);
    for (TZrSize segment = 0U; segment < segments->count; segment++) {
        if (segments->nodes[segment]->type == ZR_AST_INTERPOLATED_SEGMENT) {
            SZrAstNode *identifier = segments->nodes[segment]->data.interpolatedSegment.expression;
            TEST_ASSERT_NOT_NULL(identifier);
            TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
            TEST_ASSERT_EQUAL_STRING("x", ZrCore_String_GetNativeString(identifier->data.identifier.name));
            embeddedContext = identifier->data.identifier.syntaxContext;
            interpolatedCount++;
        }
    }
    ZrParser_State_Free(&parser);
    ZrParser_TokenStream_Release(stream);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_EQUAL_UINT(1U, interpolatedCount);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_SyntaxContext_Equal(defSite, embeddedContext),
            "An embedded identifier must inherit the native template literal's syntax context");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_marked_ast_identifier_keeps_context_and_spelling);
    RUN_TEST(test_binding_key_selects_same_spelling_by_context_and_scope);
    RUN_TEST(test_marked_ast_types_and_closure_capture_stay_context_distinct);
    RUN_TEST(test_marked_ast_runtime_shadow_and_closure_capture);
    RUN_TEST(test_native_tokens_runtime_shadow_and_closure_capture);
    RUN_TEST(test_native_tokens_borrow_capture_rejects_root_type_shadow);
    RUN_TEST(test_native_tokens_loan_capture_rejects_root_type_shadow);
    RUN_TEST(test_native_template_interpolation_keeps_literal_context);
    return UNITY_END();
}
