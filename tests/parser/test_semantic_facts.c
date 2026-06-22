#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

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

static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "facts_test.zr", 13);
    return range;
}

static void init_identifier_node(SZrAstNode *node, TZrSize startOffset, TZrSize endOffset) {
    memset(node, 0, sizeof(*node));
    node->type = ZR_AST_IDENTIFIER_LITERAL;
    node->location = test_range(startOffset, endOffset);
}

static void init_identifier_node_with_range(SZrAstNode *node, SZrFileRange range) {
    memset(node, 0, sizeof(*node));
    node->type = ZR_AST_IDENTIFIER_LITERAL;
    node->location = range;
}

static TZrBool find_source_range(const TZrChar *content,
                                 SZrString *sourceName,
                                 const TZrChar *needle,
                                 TZrSize occurrence,
                                 TZrSize extraOffset,
                                 TZrSize width,
                                 SZrFileRange *outRange) {
    const TZrChar *match;
    const TZrChar *cursor;
    TZrSize currentOccurrence = 0;
    TZrSize offset;
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    if (content == ZR_NULL || needle == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    match += extraOffset;
    cursor = content;
    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
        cursor++;
    }

    offset = (TZrSize)(match - content);
    memset(outRange, 0, sizeof(*outRange));
    outRange->source = sourceName;
    outRange->start.offset = offset;
    outRange->start.line = line;
    outRange->start.column = column;
    outRange->end = outRange->start;
    outRange->end.offset = offset + width;
    outRange->end.column = column + (TZrInt32)width;
    return ZR_TRUE;
}

static void append_reference_fact(SZrSemanticContext *context,
                                  SZrAstNode *node,
                                  EZrSemanticReferenceKind kind,
                                  TZrSymbolId symbolId,
                                  SZrFileRange declarationRange) {
    SZrSemanticReferenceFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.declarationRange = declarationRange;
    fact.kind = kind;
    fact.symbolId = symbolId;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
}

static void append_reference_fact_with_definite_assignment(
        SZrSemanticContext *context,
        SZrAstNode *node,
        EZrSemanticReferenceKind kind,
        TZrSymbolId symbolId,
        SZrFileRange declarationRange,
        EZrSemanticDefiniteAssignmentState state) {
    SZrSemanticReferenceFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.declarationRange = declarationRange;
    fact.kind = kind;
    fact.symbolId = symbolId;
    fact.isResolved = ZR_TRUE;
    fact.hasDefiniteAssignmentState = ZR_TRUE;
    fact.definiteAssignmentState = state;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
}

static void test_semantic_context_initializes_fact_arrays(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_TRUE(context->expressionFacts.isValid);
    TEST_ASSERT_TRUE(context->referenceFacts.isValid);
    TEST_ASSERT_TRUE(context->numericFacts.isValid);
    TEST_ASSERT_TRUE(context->reachabilityFacts.isValid);
    TEST_ASSERT_TRUE(context->logicalFacts.isValid);
    TEST_ASSERT_TRUE(context->ownershipFacts.isValid);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->expressionFacts.length);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_expression_fact_roundtrip_by_node_and_position(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode fakeNode;
    SZrInferredType inferredType;
    SZrSemanticExpressionFact fact;
    const SZrSemanticExpressionFact *foundByNode;
    const SZrSemanticExpressionFact *foundByPosition;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fakeNode, 0, sizeof(fakeNode));
    fakeNode.type = ZR_AST_INTEGER_LITERAL;
    fakeNode.location = test_range(4, 6);
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);

    memset(&fact, 0, sizeof(fact));
    fact.node = &fakeNode;
    fact.range = fakeNode.location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.valueKind = ZR_SEMANTIC_VALUE_KIND_INT64;
    fact.hasConstant = ZR_TRUE;
    fact.constantValue.int64Value = 42;
    ZrParser_InferredType_Copy(g_state, &fact.inferredType, &inferredType);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &fact));
    foundByNode = ZrParser_SemanticFacts_FindExpressionByNode(context, &fakeNode);
    foundByPosition = ZrParser_SemanticFacts_FindExpressionAtPosition(context, test_range(5, 5));
    TEST_ASSERT_NOT_NULL(foundByNode);
    TEST_ASSERT_NOT_NULL(foundByPosition);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, foundByNode->inferredType.baseType);
    TEST_ASSERT_TRUE(foundByPosition->hasConstant);
    TEST_ASSERT_EQUAL_INT64(42, foundByPosition->constantValue.int64Value);

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_logical_fact_roundtrip_by_node_and_position(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode fakeNode;
    SZrSemanticLogicalFact fact;
    const SZrSemanticLogicalFact *foundByNode;
    const SZrSemanticLogicalFact *foundByPosition;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fakeNode, 0, sizeof(fakeNode));
    fakeNode.type = ZR_AST_LOGICAL_EXPRESSION;
    fakeNode.location = test_range(10, 24);

    memset(&fact, 0, sizeof(fact));
    fact.node = &fakeNode;
    fact.range = fakeNode.location;
    fact.kind = ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = ZR_TRUE;
    fact.knownValue = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendLogical(context, &fact));
    foundByNode = ZrParser_SemanticFacts_FindLogicalByNode(context, &fakeNode);
    foundByPosition = ZrParser_SemanticFacts_FindLogicalAtPosition(context, test_range(16, 16));
    TEST_ASSERT_NOT_NULL(foundByNode);
    TEST_ASSERT_NOT_NULL(foundByPosition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT, foundByPosition->kind);
    TEST_ASSERT_TRUE(foundByPosition->hasKnownValue);
    TEST_ASSERT_TRUE(foundByPosition->knownValue);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_numeric_fact_by_node_prefers_segmented_range(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode fakeNode;
    SZrSemanticNumericFact broadFact;
    SZrSemanticNumericFact segmentedFact;
    const SZrSemanticNumericFact *foundByNode;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fakeNode, 0, sizeof(fakeNode));
    fakeNode.type = ZR_AST_BINARY_EXPRESSION;
    fakeNode.location = test_range(4, 12);

    memset(&broadFact, 0, sizeof(broadFact));
    broadFact.node = &fakeNode;
    broadFact.range = fakeNode.location;
    broadFact.kind = ZR_SEMANTIC_NUMERIC_FACT_PROMOTION;
    broadFact.exactness = ZR_SEMANTIC_FACT_EXACT;
    broadFact.sourceType = ZR_VALUE_TYPE_INT64;
    broadFact.targetType = ZR_VALUE_TYPE_INT64;
    broadFact.hasRange = ZR_TRUE;
    broadFact.minValue = 1;
    broadFact.maxValue = 256;
    broadFact.hasUnsignedRange = ZR_TRUE;
    broadFact.minUnsignedValue = 1;
    broadFact.maxUnsignedValue = 256;

    segmentedFact = broadFact;
    segmentedFact.rangeSegmentCount = 2;
    segmentedFact.rangeSegments[0].minValue = 1;
    segmentedFact.rangeSegments[0].maxValue = 10;
    segmentedFact.rangeSegments[1].minValue = 22;
    segmentedFact.rangeSegments[1].maxValue = 256;

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendNumeric(context, &broadFact));
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendNumeric(context, &segmentedFact));

    foundByNode = ZrParser_SemanticFacts_FindNumericByNode(context, &fakeNode);
    TEST_ASSERT_NOT_NULL(foundByNode);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)foundByNode->rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(1, foundByNode->rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(10, foundByNode->rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(22, foundByNode->rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(256, foundByNode->rangeSegments[1].maxValue);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_context_reset_clears_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticReachabilityFact fact;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    fact.range = test_range(10, 20);
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = ZR_SEMANTIC_REACHABILITY_AFTER_RETURN;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReachability(context, &fact));
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)context->reachabilityFacts.length);

    ZrParser_SemanticContext_Reset(context);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->reachabilityFacts.length);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->expressionFacts.length);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_facts_resolve_linear_definite_assignment_from_reference_order(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode readBeforeWriteNode;
    SZrAstNode writeNode;
    SZrAstNode readAfterWriteNode;
    SZrAstNode unknownDeclarationNode;
    SZrAstNode unknownReadNode;
    const SZrSemanticReferenceFact *readBeforeWrite;
    const SZrSemanticReferenceFact *write;
    const SZrSemanticReferenceFact *readAfterWrite;
    const SZrSemanticReferenceFact *unknownRead;

    TEST_ASSERT_NOT_NULL(context);
    init_identifier_node(&declarationNode, 0, 3);
    init_identifier_node(&readBeforeWriteNode, 8, 11);
    init_identifier_node(&writeNode, 16, 19);
    init_identifier_node(&readAfterWriteNode, 24, 27);
    init_identifier_node(&unknownDeclarationNode, 32, 35);
    init_identifier_node(&unknownReadNode, 40, 43);

    append_reference_fact_with_definite_assignment(
            context,
            &declarationNode,
            ZR_SEMANTIC_REFERENCE_DECLARATION,
            7,
            declarationNode.location,
            ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT);
    append_reference_fact(context,
                          &readBeforeWriteNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          7,
                          declarationNode.location);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          7,
                          declarationNode.location);
    append_reference_fact(context,
                          &readAfterWriteNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          7,
                          declarationNode.location);
    append_reference_fact(context,
                          &unknownDeclarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          8,
                          unknownDeclarationNode.location);
    append_reference_fact(context,
                          &unknownReadNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          8,
                          unknownDeclarationNode.location);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(context));

    readBeforeWrite = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            test_range(9, 9),
            ZR_SEMANTIC_REFERENCE_READ);
    write = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            test_range(17, 17),
            ZR_SEMANTIC_REFERENCE_WRITE);
    readAfterWrite = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            test_range(25, 25),
            ZR_SEMANTIC_REFERENCE_READ);
    unknownRead = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            test_range(41, 41),
            ZR_SEMANTIC_REFERENCE_READ);

    TEST_ASSERT_NOT_NULL(readBeforeWrite);
    TEST_ASSERT_TRUE(readBeforeWrite->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT,
                          readBeforeWrite->definiteAssignmentState);

    TEST_ASSERT_NOT_NULL(write);
    TEST_ASSERT_TRUE(write->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                          write->definiteAssignmentState);

    TEST_ASSERT_NOT_NULL(readAfterWrite);
    TEST_ASSERT_TRUE(readAfterWrite->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                          readAfterWrite->definiteAssignmentState);

    TEST_ASSERT_NOT_NULL(unknownRead);
    TEST_ASSERT_FALSE(unknownRead->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN,
                          unknownRead->definiteAssignmentState);

    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_definite_assignment_marks_self_initializer_read_uninit(void) {
    const TZrChar *source =
            "choose(): int {\n"
            "    var seed: int = seed;\n"
            "    return seed;\n"
            "}\n";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *bodyNode;
    SZrAstNode *declarationNode;
    SZrAstNode *declarationPattern;
    SZrAstNode *initializerRead;
    const SZrSemanticReferenceFact *readFact;

    TEST_ASSERT_NOT_NULL(context);
    sourceName = ZrCore_String_Create(g_state,
                                      "facts_cfg_self_initializer_test.zr",
                                      strlen("facts_cfg_self_initializer_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)ast->data.script.statements->count);

    functionNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    bodyNode = functionNode->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(bodyNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, bodyNode->type);
    TEST_ASSERT_NOT_NULL(bodyNode->data.block.body);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (TZrUInt32)bodyNode->data.block.body->count);

    declarationNode = bodyNode->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(declarationNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declarationNode->type);
    declarationPattern = declarationNode->data.variableDeclaration.pattern;
    initializerRead = declarationNode->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(declarationPattern);
    TEST_ASSERT_NOT_NULL(initializerRead);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, declarationPattern->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, initializerRead->type);

    append_reference_fact(context,
                          declarationPattern,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          91,
                          declarationPattern->location);
    append_reference_fact(context,
                          initializerRead,
                          ZR_SEMANTIC_REFERENCE_READ,
                          91,
                          declarationPattern->location);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(context, ast));
    readFact = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            initializerRead->location,
            ZR_SEMANTIC_REFERENCE_READ);

    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_TRUE(readFact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT,
                          readFact->definiteAssignmentState);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_definite_assignment_joins_finally_read_from_normal_and_return_paths(void) {
    const TZrChar *source =
            "choose(flag: bool): int {\n"
            "    var seed: int;\n"
            "    try {\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "            return seed;\n"
            "        }\n"
            "    } finally {\n"
            "        return seed;\n"
            "    }\n"
            "}\n";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange declarationRange;
    SZrFileRange writeRange;
    SZrFileRange finallyReadRange;
    SZrAstNode declarationNode;
    SZrAstNode writeNode;
    SZrAstNode finallyReadNode;
    const SZrSemanticReferenceFact *readFact;

    TEST_ASSERT_NOT_NULL(context);
    sourceName = ZrCore_String_Create(g_state,
                                      "facts_cfg_finally_join_test.zr",
                                      strlen("facts_cfg_finally_join_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "var seed",
                                       0,
                                       strlen("var "),
                                       strlen("seed"),
                                       &declarationRange));
    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "seed = 1",
                                       0,
                                       0,
                                       strlen("seed"),
                                       &writeRange));
    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "return seed",
                                       1,
                                       strlen("return "),
                                       strlen("seed"),
                                       &finallyReadRange));

    init_identifier_node_with_range(&declarationNode, declarationRange);
    init_identifier_node_with_range(&writeNode, writeRange);
    init_identifier_node_with_range(&finallyReadNode, finallyReadRange);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          103,
                          declarationRange);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          103,
                          declarationRange);
    append_reference_fact(context,
                          &finallyReadNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          103,
                          declarationRange);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(context, ast));
    readFact = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            finallyReadRange,
            ZR_SEMANTIC_REFERENCE_READ);

    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_TRUE(readFact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT,
                          readFact->definiteAssignmentState);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_definite_assignment_preserves_true_loop_break_write(void) {
    const TZrChar *source =
            "choose(): int {\n"
            "    var seed: int;\n"
            "    while (true) {\n"
            "        seed = 1;\n"
            "        break;\n"
            "    }\n"
            "    return seed;\n"
            "}\n";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange declarationRange;
    SZrFileRange writeRange;
    SZrFileRange finalReadRange;
    SZrAstNode declarationNode;
    SZrAstNode writeNode;
    SZrAstNode finalReadNode;
    const SZrSemanticReferenceFact *readFact;

    TEST_ASSERT_NOT_NULL(context);
    sourceName = ZrCore_String_Create(g_state,
                                      "facts_cfg_true_loop_break_test.zr",
                                      strlen("facts_cfg_true_loop_break_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "seed: int",
                                       0,
                                       0,
                                       strlen("seed"),
                                       &declarationRange));
    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "seed = 1",
                                       0,
                                       0,
                                       strlen("seed"),
                                       &writeRange));
    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "return seed",
                                       0,
                                       strlen("return "),
                                       strlen("seed"),
                                       &finalReadRange));

    init_identifier_node_with_range(&declarationNode, declarationRange);
    init_identifier_node_with_range(&writeNode, writeRange);
    init_identifier_node_with_range(&finalReadNode, finalReadRange);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          105,
                          declarationRange);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          105,
                          declarationRange);
    append_reference_fact(context,
                          &finalReadNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          105,
                          declarationRange);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(context, ast));
    readFact = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            finalReadRange,
            ZR_SEMANTIC_REFERENCE_READ);

    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_TRUE(readFact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                          readFact->definiteAssignmentState);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_reaching_definitions_clears_finally_read_from_normal_and_return_paths(void) {
    const TZrChar *source =
            "choose(flag: bool): int {\n"
            "    var seed: int;\n"
            "    try {\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "            return seed;\n"
            "        }\n"
            "    } finally {\n"
            "        return seed;\n"
            "    }\n"
            "}\n";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange declarationRange;
    SZrFileRange writeRange;
    SZrFileRange finallyReadRange;
    SZrAstNode declarationNode;
    SZrAstNode writeNode;
    SZrAstNode finallyReadNode;
    const SZrSemanticReferenceFact *writeFact;
    const SZrSemanticReferenceFact *readFact;

    TEST_ASSERT_NOT_NULL(context);
    sourceName = ZrCore_String_Create(g_state,
                                      "facts_cfg_reaching_finally_join_test.zr",
                                      strlen("facts_cfg_reaching_finally_join_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "seed: int",
                                       0,
                                       0,
                                       strlen("seed"),
                                       &declarationRange));
    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "seed = 1",
                                       0,
                                       0,
                                       strlen("seed"),
                                       &writeRange));
    TEST_ASSERT_TRUE(find_source_range(source,
                                       sourceName,
                                       "return seed",
                                       1,
                                       strlen("return "),
                                       strlen("seed"),
                                       &finallyReadRange));

    init_identifier_node_with_range(&declarationNode, declarationRange);
    init_identifier_node_with_range(&writeNode, writeRange);
    init_identifier_node_with_range(&finallyReadNode, finallyReadRange);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          104,
                          declarationRange);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          104,
                          declarationRange);
    append_reference_fact(context,
                          &finallyReadNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          104,
                          declarationRange);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(context));
    writeFact = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            writeRange,
            ZR_SEMANTIC_REFERENCE_WRITE);
    readFact = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            finallyReadRange,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(writeFact);
    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_TRUE(readFact->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)writeFact->range.start.offset,
                             (TZrUInt32)readFact->definitionRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(context, ast));
    readFact = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            context,
            finallyReadRange,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_FALSE(readFact->hasDefinitionRange);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_context_initializes_fact_arrays);
    RUN_TEST(test_semantic_expression_fact_roundtrip_by_node_and_position);
    RUN_TEST(test_semantic_logical_fact_roundtrip_by_node_and_position);
    RUN_TEST(test_semantic_numeric_fact_by_node_prefers_segmented_range);
    RUN_TEST(test_semantic_context_reset_clears_facts);
    RUN_TEST(test_semantic_facts_resolve_linear_definite_assignment_from_reference_order);
    RUN_TEST(test_cfg_definite_assignment_marks_self_initializer_read_uninit);
    RUN_TEST(test_cfg_definite_assignment_joins_finally_read_from_normal_and_return_paths);
    RUN_TEST(test_cfg_definite_assignment_preserves_true_loop_break_write);
    RUN_TEST(test_cfg_reaching_definitions_clears_finally_read_from_normal_and_return_paths);
    return UNITY_END();
}
