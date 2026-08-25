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
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(cs.semanticContext));
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compiled_call_edges_publish_stable_incoming_and_outgoing);
    RUN_TEST(test_unresolved_call_edge_never_selects_same_name_target);
    RUN_TEST(test_call_candidates_project_resolved_overload_set);
    return UNITY_END();
}
