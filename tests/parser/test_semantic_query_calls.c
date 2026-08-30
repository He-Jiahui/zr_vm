#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_calls.h"
#include "zr_vm_parser/semantic_query.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void call_release_compiler_function(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }
    if (cs->topLevelFunction != ZR_NULL &&
        cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }
    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static const SZrSemanticSymbolRecord *call_find_symbol_by_node(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static SZrFileRange call_source_position(const TZrChar *source,
                                         SZrString *sourceName,
                                         const TZrChar *needle,
                                         TZrSize occurrence) {
    const TZrChar *cursor = source;
    const TZrChar *match = ZR_NULL;
    SZrFileRange range;
    TZrSize offset;

    while (occurrence-- > 0U) {
        cursor = strstr(cursor, needle);
        TEST_ASSERT_NOT_NULL(cursor);
        cursor += strlen(needle);
    }
    match = strstr(cursor, needle);
    TEST_ASSERT_NOT_NULL(match);
    offset = (TZrSize)(match - source);
    memset(&range, 0, sizeof(range));
    range.source = sourceName;
    range.start.offset = offset;
    range.end.offset = offset + strlen(needle);
    range.start.line = 1;
    range.end.line = 1;
    range.start.column = (TZrInt32)offset + 1;
    range.end.column = range.start.column + (TZrInt32)strlen(needle);
    return range;
}

static void test_compiled_call_edges_publish_stable_incoming_and_outgoing(void) {
    const TZrChar *source =
            "fn callee(): int { return 1; }\n"
            "fn caller(): int { return callee(); }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_edges.zr");
    SZrAstNode *ast;
    SZrAstNode *calleeNode;
    SZrAstNode *callerNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *callee;
    const SZrSemanticSymbolRecord *caller;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    calleeNode = ast->data.script.statements->nodes[0];
    callerNode = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(calleeNode);
    TEST_ASSERT_NOT_NULL(callerNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    callee = call_find_symbol_by_node(cs.semanticContext, calleeNode);
    caller = call_find_symbol_by_node(cs.semanticContext, callerNode);
    TEST_ASSERT_NOT_NULL(callee);
    TEST_ASSERT_NOT_NULL(caller);
    ZrParser_SemanticCalls_Reset(cs.semanticContext);
    cs.semanticContext->scopeFacts.length = 0U;
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_PublishSource(
            cs.semanticContext, ast));
    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            cs.semanticContext, caller->id, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(caller->id, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(callee->id, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED,
                          edge->resolution);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, edge->callableTypeId);
    TEST_ASSERT_TRUE(edge->hasTargetDeclarationRange);
    TEST_ASSERT_EQUAL_UINT(callee->location.start.offset,
                           edge->targetDeclarationRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_IncomingCalls(
            cs.semanticContext, callee->id, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_EQUAL_UINT(caller->id, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(callee->id, edge->targetSymbolId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallEdgesAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "callee", 1U),
            ZR_NULL,
            &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_EQUAL_UINT(caller->id, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(callee->id, edge->targetSymbolId);

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_OutgoingCalls(
            cs.semanticContext, ZR_SEMANTIC_ID_INVALID, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_IncomingCalls(
            cs.semanticContext, ZR_SEMANTIC_ID_INVALID, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);

    ZrCore_Array_Free(g_state, &edges);
    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unresolved_call_edge_never_selects_same_name_target(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticScopeFact scope;
    SZrSemanticReferenceFact callReference;
    TZrSymbolId callerId;
    TZrSymbolId sameNameTargetId;
    SZrArray edges;
    SZrArray candidates;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(context);
    callerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "caller"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            11U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("caller", ZR_NULL, "caller", 0U));
    sameNameTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            12U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("target", ZR_NULL, "target", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, callerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, sameNameTargetId);

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    scope.ownerSymbolId = callerId;
    scope.range = call_source_position("target()", ZR_NULL, "target", 0U);
    scope.range.end.offset = strlen("target()");
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &scope));

    memset(&callReference, 0, sizeof(callReference));
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.range = call_source_position("target()", ZR_NULL, "target", 0U);
    callReference.typeId = 12U;
    callReference.name = ZrCore_String_CreateFromNative(g_state, "target");
    callReference.isResolved = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, callerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(callerId, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED,
                          edge->resolution);
    TEST_ASSERT_NOT_EQUAL_UINT(sameNameTargetId, edge->targetSymbolId);

    ZrCore_Array_Construct(&candidates);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallCandidatesAt(
            context, callReference.range, ZR_NULL, &candidates));
    TEST_ASSERT_EQUAL_UINT(0U, candidates.length);

    ZrCore_Array_Free(g_state, &candidates);
    ZrCore_Array_Free(g_state, &edges);
    ZrParser_SemanticContext_Free(context);
}

static void test_duplicate_function_records_canonicalize_by_ast_identity(void) {
    const TZrChar *source = "target()";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode callerNode;
    SZrAstNode targetNode;
    SZrSemanticScopeFact scope;
    SZrSemanticReferenceFact callReference;
    TZrSymbolId callerId;
    TZrSymbolId canonicalTargetId;
    TZrSymbolId duplicateTargetId;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(context);
    memset(&callerNode, 0, sizeof(callerNode));
    memset(&targetNode, 0, sizeof(targetNode));
    callerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "caller"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            11U,
            ZR_SEMANTIC_ID_INVALID,
            &callerNode,
            call_source_position("caller", ZR_NULL, "caller", 0U));
    canonicalTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            12U,
            ZR_SEMANTIC_ID_INVALID,
            &targetNode,
            call_source_position(source, ZR_NULL, "target", 0U));
    duplicateTargetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "displayAlias"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            13U,
            ZR_SEMANTIC_ID_INVALID,
            &targetNode,
            call_source_position(source, ZR_NULL, "target", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, callerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, canonicalTargetId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, duplicateTargetId);

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    scope.ownerSymbolId = callerId;
    scope.range = call_source_position(source, ZR_NULL, "target", 0U);
    scope.range.end.offset = strlen(source);
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &scope));

    memset(&callReference, 0, sizeof(callReference));
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.range = call_source_position(source, ZR_NULL, "target", 0U);
    callReference.declarationRange = callReference.range;
    callReference.typeId = 12U;
    callReference.symbolId = canonicalTargetId;
    callReference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &callReference));
    callReference.typeId = 13U;
    callReference.symbolId = duplicateTargetId;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, callerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(canonicalTargetId, edge->targetSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT(duplicateTargetId, edge->targetSymbolId);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_IncomingCalls(
            context, duplicateTargetId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);

    ZrCore_Array_Free(g_state, &edges);
    ZrParser_SemanticContext_Free(context);
}

static void test_approximate_call_expression_never_publishes_call_edge(void) {
    const TZrChar *source = "callee()";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticScopeFact scope;
    SZrSemanticReferenceFact callReference;
    SZrSemanticExpressionFact expression;
    TZrSymbolId callerId;
    TZrSymbolId calleeId;
    SZrArray edges;

    TEST_ASSERT_NOT_NULL(context);
    callerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "caller"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            11U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position(source, ZR_NULL, "callee", 0U));
    calleeId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "callee"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            12U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position(source, ZR_NULL, "callee", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, callerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, calleeId);

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    scope.ownerSymbolId = callerId;
    scope.range = call_source_position(source, ZR_NULL, "callee", 0U);
    scope.range.end.offset = strlen(source);
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &scope));

    memset(&expression, 0, sizeof(expression));
    expression.range = scope.range;
    expression.exactness = ZR_SEMANTIC_FACT_APPROXIMATE;
    expression.hasCallInfo = ZR_TRUE;
    expression.callTargetRange = call_source_position(source, ZR_NULL, "callee", 0U);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &expression));

    memset(&callReference, 0, sizeof(callReference));
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.range = expression.callTargetRange;
    callReference.declarationRange = expression.callTargetRange;
    callReference.typeId = 12U;
    callReference.symbolId = calleeId;
    callReference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, callerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);

    ZrCore_Array_Free(g_state, &edges);
    ZrParser_SemanticContext_Free(context);
}

static void test_nested_function_scope_without_owner_fails_closed(void) {
    const TZrChar *source = "xxcallee()";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticScopeFact outerScope;
    SZrSemanticScopeFact nestedScope;
    SZrSemanticReferenceFact callReference;
    SZrFileRange callRange;
    TZrSymbolId outerId;
    TZrSymbolId calleeId;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(context);
    callRange = call_source_position(source, ZR_NULL, "callee", 0U);
    outerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "outer"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            11U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            callRange);
    calleeId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "callee"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            12U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            callRange);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, outerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, calleeId);

    memset(&outerScope, 0, sizeof(outerScope));
    outerScope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    outerScope.ownerSymbolId = outerId;
    outerScope.range = callRange;
    outerScope.range.start.offset = 0U;
    outerScope.range.end.offset = strlen(source);
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &outerScope));

    memset(&nestedScope, 0, sizeof(nestedScope));
    nestedScope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    nestedScope.ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    nestedScope.range = callRange;
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &nestedScope));

    memset(&callReference, 0, sizeof(callReference));
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.range = callRange;
    callReference.declarationRange = callRange;
    callReference.typeId = 12U;
    callReference.symbolId = calleeId;
    callReference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallEdgesAt(
            context, callRange, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(calleeId, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE,
                          edge->resolution);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, outerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);

    ZrCore_Array_Free(g_state, &edges);
    ZrParser_SemanticContext_Free(context);
}

static void test_lambda_call_edge_uses_lambda_caller_identity(void) {
    const TZrChar *source =
            "fn callee(): int { return 1; }\n"
            "fn outer(): int {\n"
            "    var callback = fn(): int => { return callee(); };\n"
            "    return callback();\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_lambda_call_edges.zr");
    SZrAstNode *ast;
    SZrAstNode *calleeNode;
    SZrAstNode *outerNode;
    SZrAstNode *lambdaNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *callee;
    const SZrSemanticSymbolRecord *outer;
    const SZrSemanticSymbolRecord *lambda;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    calleeNode = ast->data.script.statements->nodes[0];
    outerNode = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(calleeNode);
    TEST_ASSERT_NOT_NULL(outerNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, outerNode->type);
    TEST_ASSERT_NOT_NULL(outerNode->data.functionDeclaration.body);
    TEST_ASSERT_NOT_NULL(outerNode->data.functionDeclaration.body->data.block.body);
    lambdaNode = outerNode->data.functionDeclaration.body->data.block.body->nodes[0]
            ->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambdaNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambdaNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    callee = call_find_symbol_by_node(cs.semanticContext, calleeNode);
    outer = call_find_symbol_by_node(cs.semanticContext, outerNode);
    lambda = call_find_symbol_by_node(cs.semanticContext, lambdaNode);
    TEST_ASSERT_NOT_NULL(callee);
    TEST_ASSERT_NOT_NULL(outer);
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_NOT_EQUAL_UINT(outer->id, lambda->id);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, lambda->kind);

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            cs.semanticContext, lambda->id, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(lambda->id, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(callee->id, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED,
                          edge->resolution);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_IncomingCalls(
            cs.semanticContext, callee->id, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(lambda->id, edge->callerSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT(outer->id, edge->callerSymbolId);

    ZrCore_Array_Free(g_state, &edges);
    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_returned_unowned_lambda_call_edge_fails_closed(void) {
    const TZrChar *source =
            "fn callee(): int { return 1; }\n"
            "fn makeRunner(): fn() -> int {\n"
            "    return fn(): int => { return callee(); };\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_returned_lambda_call_edges.zr");
    SZrAstNode *ast;
    SZrAstNode *calleeNode;
    SZrAstNode *makerNode;
    SZrAstNode *lambdaNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *callee;
    const SZrSemanticSymbolRecord *maker;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    calleeNode = ast->data.script.statements->nodes[0];
    makerNode = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(calleeNode);
    TEST_ASSERT_NOT_NULL(makerNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, makerNode->type);
    TEST_ASSERT_NOT_NULL(makerNode->data.functionDeclaration.body);
    TEST_ASSERT_NOT_NULL(makerNode->data.functionDeclaration.body->data.block.body);
    lambdaNode = makerNode->data.functionDeclaration.body->data.block.body->nodes[0]
            ->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(lambdaNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambdaNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    callee = call_find_symbol_by_node(cs.semanticContext, calleeNode);
    maker = call_find_symbol_by_node(cs.semanticContext, makerNode);
    TEST_ASSERT_NOT_NULL(callee);
    TEST_ASSERT_NOT_NULL(maker);
    TEST_ASSERT_NULL(call_find_symbol_by_node(cs.semanticContext, lambdaNode));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_IncomingCalls(
            cs.semanticContext, callee->id, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(callee->id, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE,
                          edge->resolution);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            cs.semanticContext, maker->id, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);

    ZrCore_Array_Free(g_state, &edges);
    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_call_candidates_project_resolved_overload_set(void) {
    const TZrChar *source =
            "fn choose(value: int): int { return value; }\n"
            "fn choose(value: string): int { return 0; }\n"
            "fn caller(): int { return choose(1); }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_candidates.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *intChoice;
    const SZrSemanticSymbolRecord *stringChoice;
    SZrParserSemanticCallQuery call;
    SZrArray candidates;
    SZrFileRange expectedCallRange;
    SZrFileRange expectedTargetRange;
    TZrSize selectedCount = 0U;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(3U, ast->data.script.statements->count);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    intChoice = call_find_symbol_by_node(
            cs.semanticContext, ast->data.script.statements->nodes[0]);
    stringChoice = call_find_symbol_by_node(
            cs.semanticContext, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_NOT_NULL(intChoice);
    TEST_ASSERT_NOT_NULL(stringChoice);
    TEST_ASSERT_NOT_EQUAL_UINT(intChoice->id, stringChoice->id);

    memset(&call, 0, sizeof(call));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "choose", 2U),
            ZR_NULL,
            &call));
    TEST_ASSERT_TRUE(call.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT(intChoice->id, call.targetSymbolId);
    expectedTargetRange = call_source_position(source, sourceName, "choose", 2U);
    expectedCallRange = expectedTargetRange;
    expectedCallRange.end.offset += strlen("(1)");
    TEST_ASSERT_EQUAL_PTR(sourceName, call.callSiteRange.source);
    TEST_ASSERT_EQUAL_UINT64(expectedCallRange.start.offset, call.callSiteRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expectedCallRange.end.offset, call.callSiteRange.end.offset);
    TEST_ASSERT_EQUAL_PTR(sourceName, call.callTargetRange.source);
    TEST_ASSERT_EQUAL_UINT64(expectedTargetRange.start.offset, call.callTargetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expectedTargetRange.end.offset, call.callTargetRange.end.offset);
    TEST_ASSERT_EQUAL_UINT(1U, call.argumentCount);
    TEST_ASSERT_FALSE(call.hasNamedArguments);
    TEST_ASSERT_FALSE(call.isMemberCall);

    ZrCore_Array_Construct(&candidates);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallCandidatesAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "choose", 2U),
            ZR_NULL,
            &candidates));
    TEST_ASSERT_EQUAL_UINT(2U, candidates.length);
    for (TZrSize index = 0U; index < candidates.length; index++) {
        const SZrParserSemanticCallCandidateQuery *candidate =
                (const SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                        &candidates, index);

        TEST_ASSERT_NOT_NULL(candidate);
        TEST_ASSERT_TRUE(candidate->symbolId == intChoice->id ||
                         candidate->symbolId == stringChoice->id);
        if (candidate->isSelected) {
            selectedCount++;
            TEST_ASSERT_EQUAL_UINT(call.targetSymbolId, candidate->symbolId);
            TEST_ASSERT_EQUAL_UINT(intChoice->typeId, candidate->callableTypeId);
        }
    }
    TEST_ASSERT_EQUAL_UINT(1U, selectedCount);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallCandidatesAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "choose", 2U),
            ZR_NULL,
            &candidates));
    TEST_ASSERT_EQUAL_UINT(2U, candidates.length);

    ZrCore_Array_Free(g_state, &candidates);
    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_call_candidates_fail_closed_for_approximate_call_fact(void) {
    const TZrChar *source =
            "fn choose(value: int): int { return value; }\n"
            "fn choose(value: string): int { return 0; }\n"
            "fn caller(): int { return choose(1); }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_candidates_exactness.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrFileRange callPosition;
    const SZrSemanticExpressionFact *expression;
    SZrParserSemanticCallQuery call;
    SZrArray candidates;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    callPosition = call_source_position(source, sourceName, "choose", 2U);
    memset(&call, 0, sizeof(call));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, callPosition, ZR_NULL, &call));
    expression = call.expression;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_TRUE(expression->hasCallInfo);
    ((SZrSemanticExpressionFact *)expression)->exactness =
            ZR_SEMANTIC_FACT_APPROXIMATE;

    memset(&call, 0, sizeof(call));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, callPosition, ZR_NULL, &call));
    TEST_ASSERT_NOT_NULL(call.expression);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_APPROXIMATE,
                          call.expression->exactness);

    ZrCore_Array_Construct(&candidates);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallCandidatesAt(
            cs.semanticContext, callPosition, ZR_NULL, &candidates));
    TEST_ASSERT_EQUAL_UINT(0U, candidates.length);

    ZrCore_Array_Free(g_state, &candidates);
    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_format_call_fails_closed_for_approximate_call_fact(void) {
    const TZrChar *source =
            "fn choose(value: int): int { return value; }\n"
            "fn choose(value: string): int { return 0; }\n"
            "fn caller(): int { return choose(1); }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_format_exactness.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrFileRange callPosition;
    SZrParserSemanticCallQuery call;
    TZrChar display[128] = "stale display";

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    callPosition = call_source_position(source, sourceName, "choose", 2U);
    memset(&call, 0, sizeof(call));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, callPosition, ZR_NULL, &call));
    TEST_ASSERT_NOT_NULL(call.expression);
    ((SZrSemanticExpressionFact *)call.expression)->exactness =
            ZR_SEMANTIC_FACT_APPROXIMATE;

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &call, display, sizeof(display)));
    TEST_ASSERT_EQUAL_CHAR('\0', display[0]);

    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_format_call_fails_closed_for_mismatched_reference_fact(void) {
    const TZrChar *source =
            "fn first(value: int): int { return value; }\n"
            "fn second(value: string): int { return 0; }\n"
            "fn caller(): int { var value = first(1); return second(\"x\"); }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_format_coherence.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrParserSemanticCallQuery first;
    SZrParserSemanticCallQuery second;
    TZrChar display[128] = "stale display";

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "first", 1U),
            ZR_NULL,
            &first));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "second", 1U),
            ZR_NULL,
            &second));
    TEST_ASSERT_NOT_NULL(first.reference);
    TEST_ASSERT_NOT_NULL(second.reference);
    first.reference = second.reference;

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &first, display, sizeof(display)));
    TEST_ASSERT_EQUAL_CHAR('\0', display[0]);

    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#include "test_semantic_query_call_edge_refinement_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compiled_call_edges_publish_stable_incoming_and_outgoing);
    RUN_TEST(test_unresolved_call_edge_never_selects_same_name_target);
    RUN_TEST(test_duplicate_function_records_canonicalize_by_ast_identity);
    RUN_TEST(test_approximate_call_expression_never_publishes_call_edge);
    RUN_TEST(test_nested_function_scope_without_owner_fails_closed);
    RUN_TEST(test_lambda_call_edge_uses_lambda_caller_identity);
    RUN_TEST(test_returned_unowned_lambda_call_edge_fails_closed);
    RUN_TEST(test_call_candidates_project_resolved_overload_set);
    RUN_TEST(test_call_candidates_fail_closed_for_approximate_call_fact);
    RUN_TEST(test_format_call_fails_closed_for_approximate_call_fact);
    RUN_TEST(test_format_call_fails_closed_for_mismatched_reference_fact);
    RUN_TEST(test_resolved_call_edge_supersedes_unresolved_same_site);
    return UNITY_END();
}
