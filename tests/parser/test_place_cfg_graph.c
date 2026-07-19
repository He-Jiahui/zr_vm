#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/place.h"

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

    memset(&range, 0, sizeof(range));
    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "place_cfg_test.zr", 17);
    return range;
}

static SZrAstNode *test_ast_node(EZrAstNodeType type,
                                 TZrSize startOffset,
                                 TZrSize endOffset) {
    SZrAstNode *node = (SZrAstNode *)ZrCore_Memory_RawMallocWithType(
            g_state->global,
            sizeof(SZrAstNode),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);

    TEST_ASSERT_NOT_NULL(node);
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->location = test_range(startOffset, endOffset);
    return node;
}

static SZrAstNode *empty_block(TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *block = test_ast_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1U);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    return block;
}

static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_ast_node(ZR_AST_SCRIPT, 0U, 64U);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1U);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

static SZrParserPlaceProjection projection_symbol(EZrParserPlaceProjectionKind kind,
                                                   TZrSymbolId symbolId) {
    SZrParserPlaceProjection projection;

    memset(&projection, 0, sizeof(projection));
    projection.kind = kind;
    projection.data.symbolId = symbolId;
    return projection;
}

static SZrParserPlaceProjection projection_index(EZrParserPlaceProjectionKind kind,
                                                  TZrUInt32 index) {
    SZrParserPlaceProjection projection;

    memset(&projection, 0, sizeof(projection));
    projection.kind = kind;
    projection.data.index = index;
    return projection;
}

static SZrParserPlaceProjection projection_value(TZrValueId valueId) {
    SZrParserPlaceProjection projection;

    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_INDEX;
    projection.data.valueId = valueId;
    return projection;
}

static TZrPlaceId add_local(SZrParserPlaceGraph *graph,
                            TZrSymbolId symbolId,
                            TZrSize startOffset) {
    SZrParserPlaceBase base;

    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = symbolId;
    return ZrParser_PlaceGraph_AddBase(
            graph,
            &base,
            5U,
            test_range(startOffset, startOffset + 1U));
}

static TZrPlaceId project(SZrParserPlaceGraph *graph,
                          TZrPlaceId parentId,
                          SZrParserPlaceProjection projection,
                          TZrSize startOffset) {
    return ZrParser_PlaceGraph_Project(
            graph,
            parentId,
            &projection,
            5U,
            test_range(startOffset, startOffset + 1U));
}

static void test_place_graph_covers_all_bases_and_projections(void) {
    static const EZrParserPlaceBaseKind baseKinds[] = {
        ZR_PARSER_PLACE_BASE_LOCAL,
        ZR_PARSER_PLACE_BASE_PARAMETER,
        ZR_PARSER_PLACE_BASE_THIS,
        ZR_PARSER_PLACE_BASE_STATIC,
        ZR_PARSER_PLACE_BASE_TEMPORARY,
        ZR_PARSER_PLACE_BASE_RETURN_SLOT,
        ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE,
    };
    SZrParserPlaceGraph graph;
    SZrParserPlaceBase base;
    SZrParserPlaceProjection projection;
    const SZrParserPlace *place;
    TZrPlaceId current;
    TZrSize index;

    ZrParser_PlaceGraph_Init(g_state, &graph);

    for (index = 0; index < ZR_ARRAY_COUNT(baseKinds); index++) {
        memset(&base, 0, sizeof(base));
        base.kind = baseKinds[index];
        base.identity = (TZrUInt32)(index + 1U);
        current = ZrParser_PlaceGraph_AddBase(
                &graph,
                &base,
                (TZrTypeId)(index + 10U),
                test_range(index * 2U, index * 2U + 1U));
        TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, current);
        place = ZrParser_PlaceGraph_Get(&graph, current);
        TEST_ASSERT_NOT_NULL(place);
        TEST_ASSERT_EQUAL_INT(baseKinds[index], place->base.kind);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)(index + 1U), place->base.identity);
        TEST_ASSERT_EQUAL_UINT64(index * 2U, place->sourceRange.start.offset);
    }

    current = add_local(&graph, 101U, 20U);
    projection = projection_symbol(ZR_PARSER_PLACE_PROJECTION_FIELD, 201U);
    current = project(&graph, current, projection, 22U);
    projection = projection_value(301U);
    current = project(&graph, current, projection, 24U);
    projection = projection_index(ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX, 3U);
    current = project(&graph, current, projection, 26U);
    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_DEREFERENCE;
    current = project(&graph, current, projection, 28U);
    projection = projection_symbol(ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT, 401U);
    current = project(&graph, current, projection, 30U);
    projection = projection_index(ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT, 2U);
    current = project(&graph, current, projection, 32U);

    place = ZrParser_PlaceGraph_Get(&graph, current);
    TEST_ASSERT_NOT_NULL(place);
    TEST_ASSERT_EQUAL_UINT64(6U, place->projections.length);
    TEST_ASSERT_EQUAL_UINT64(32U, place->sourceRange.start.offset);
    place = ZrParser_PlaceGraph_Get(&graph, place->parentId);
    TEST_ASSERT_NOT_NULL(place);
    TEST_ASSERT_EQUAL_UINT64(30U, place->sourceRange.start.offset);
    place = ZrParser_PlaceGraph_Get(&graph, current);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_PROJECTION_FIELD,
            ZrParser_Place_ProjectionAt(place, 0U)->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_PROJECTION_INDEX,
            ZrParser_Place_ProjectionAt(place, 1U)->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX,
            ZrParser_Place_ProjectionAt(place, 2U)->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_PROJECTION_DEREFERENCE,
            ZrParser_Place_ProjectionAt(place, 3U)->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT,
            ZrParser_Place_ProjectionAt(place, 4U)->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT,
            ZrParser_Place_ProjectionAt(place, 5U)->kind);

    ZrParser_PlaceGraph_Free(g_state, &graph);
}

static void test_place_overlap_reports_all_four_states(void) {
    SZrParserPlaceGraph graph;
    SZrParserPlaceBase externalBase;
    SZrParserPlaceProjection projection;
    TZrPlaceId localA;
    TZrPlaceId localAAgain;
    TZrPlaceId localB;
    TZrPlaceId fieldA;
    TZrPlaceId fieldAAgain;
    TZrPlaceId fieldB;
    TZrPlaceId tupleA;
    TZrPlaceId tupleB;
    TZrPlaceId constIndexA;
    TZrPlaceId constIndexB;
    TZrPlaceId dynamicIndexA;
    TZrPlaceId dynamicIndexB;
    TZrPlaceId unionA;
    TZrPlaceId unionB;
    TZrPlaceId dereferenceA;
    TZrPlaceId dereferenceB;
    TZrPlaceId externalA;
    TZrPlaceId externalB;

    ZrParser_PlaceGraph_Init(g_state, &graph);
    localA = add_local(&graph, 1U, 0U);
    localAAgain = add_local(&graph, 1U, 2U);
    localB = add_local(&graph, 2U, 4U);

    projection = projection_symbol(ZR_PARSER_PLACE_PROJECTION_FIELD, 10U);
    fieldA = project(&graph, localA, projection, 6U);
    fieldAAgain = project(&graph, localAAgain, projection, 8U);
    projection = projection_symbol(ZR_PARSER_PLACE_PROJECTION_FIELD, 11U);
    fieldB = project(&graph, localA, projection, 10U);

    projection = projection_index(ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT, 0U);
    tupleA = project(&graph, localA, projection, 12U);
    projection = projection_index(ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT, 1U);
    tupleB = project(&graph, localA, projection, 14U);

    projection = projection_index(ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX, 0U);
    constIndexA = project(&graph, localA, projection, 16U);
    projection = projection_index(ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX, 1U);
    constIndexB = project(&graph, localA, projection, 18U);

    projection = projection_value(20U);
    dynamicIndexA = project(&graph, localA, projection, 20U);
    projection = projection_value(21U);
    dynamicIndexB = project(&graph, localA, projection, 22U);

    projection = projection_symbol(ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT, 30U);
    unionA = project(&graph, localA, projection, 24U);
    projection = projection_symbol(ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT, 31U);
    unionB = project(&graph, localA, projection, 26U);

    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_DEREFERENCE;
    dereferenceA = project(&graph, localA, projection, 28U);
    dereferenceB = project(&graph, localB, projection, 30U);

    memset(&externalBase, 0, sizeof(externalBase));
    externalBase.kind = ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE;
    externalBase.identity = 40U;
    externalA = ZrParser_PlaceGraph_AddBase(&graph, &externalBase, 5U, test_range(32U, 33U));
    externalBase.identity = 41U;
    externalB = ZrParser_PlaceGraph_AddBase(&graph, &externalBase, 5U, test_range(34U, 35U));

    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_EQUAL,
            ZrParser_PlaceGraph_Overlap(&graph, fieldA, fieldAAgain));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_DISJOINT,
            ZrParser_PlaceGraph_Overlap(&graph, fieldA, fieldB));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_DISJOINT,
            ZrParser_PlaceGraph_Overlap(&graph, tupleA, tupleB));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_DISJOINT,
            ZrParser_PlaceGraph_Overlap(&graph, constIndexA, constIndexB));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_OVERLAP,
            ZrParser_PlaceGraph_Overlap(&graph, localA, fieldA));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_OVERLAP,
            ZrParser_PlaceGraph_Overlap(&graph, unionA, unionB));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_UNKNOWN,
            ZrParser_PlaceGraph_Overlap(&graph, dynamicIndexA, dynamicIndexB));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_UNKNOWN,
            ZrParser_PlaceGraph_Overlap(&graph, dereferenceA, dereferenceB));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_UNKNOWN,
            ZrParser_PlaceGraph_Overlap(&graph, externalA, externalB));

    ZrParser_PlaceGraph_Free(g_state, &graph);
}

static TZrUInt32 append_block(SZrParserCfg *cfg, EZrParserCfgBlockKind kind) {
    TZrUInt32 blockId = ZrParser_Cfg_AppendBlock(g_state, cfg, kind, ZR_NULL);

    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID, blockId);
    return blockId;
}

static void assert_edge_kind(const SZrParserCfgBlock *block,
                             TZrSize index,
                             EZrParserCfgEdgeKind expectedKind) {
    const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);

    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_INT(expectedKind, edge->kind);
}

static void test_cfg_edges_are_extensible_and_typed(void) {
    SZrParserCfg cfg;
    SZrParserCfgBlock *block;
    TZrUInt32 normalSource;
    TZrUInt32 branchSource;
    TZrUInt32 switchSource;
    TZrUInt32 exceptionSource;
    TZrUInt32 cleanupSource;
    TZrUInt32 returnSource;
    TZrUInt32 suspendSource;
    TZrUInt32 resumeSource;
    TZrUInt32 targets[12];
    TZrSize index;

    ZrParser_Cfg_Init(g_state, &cfg);
    normalSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_ENTRY);
    branchSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    switchSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    exceptionSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    cleanupSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_CLEANUP);
    returnSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_STATEMENT);
    suspendSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_SUSPENSION);
    resumeSource = append_block(&cfg, ZR_PARSER_CFG_BLOCK_SUSPENSION);
    for (index = 0; index < ZR_ARRAY_COUNT(targets); index++) {
        targets[index] = append_block(&cfg, ZR_PARSER_CFG_BLOCK_JOIN);
    }

    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, normalSource, targets[0], ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, branchSource, targets[1], ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, branchSource, targets[2], ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    for (index = 0; index < 4U; index++) {
        TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
                &cfg,
                switchSource,
                targets[index + 3U],
                ZR_PARSER_CFG_EDGE_SWITCH_CASE,
                ZR_NULL));
    }
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, switchSource, targets[7], ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, exceptionSource, targets[8], ZR_PARSER_CFG_EDGE_EXCEPTION, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, cleanupSource, targets[9], ZR_PARSER_CFG_EDGE_CLEANUP, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, returnSource, targets[10], ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, suspendSource, targets[11], ZR_PARSER_CFG_EDGE_SUSPEND, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg, resumeSource, targets[11], ZR_PARSER_CFG_EDGE_RESUME, ZR_NULL));

    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, switchSource);
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_UINT32(5U, block->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_SWITCH, block->terminatorKind);
    for (index = 0; index < 4U; index++) {
        assert_edge_kind(block, index, ZR_PARSER_CFG_EDGE_SWITCH_CASE);
    }
    assert_edge_kind(block, 4U, ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT);

    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, branchSource);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_BRANCH, block->terminatorKind);
    assert_edge_kind(block, 0U, ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge_kind(block, 1U, ZR_PARSER_CFG_EDGE_FALSE_BRANCH);

    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, exceptionSource);
    assert_edge_kind(block, 0U, ZR_PARSER_CFG_EDGE_EXCEPTION);
    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, cleanupSource);
    assert_edge_kind(block, 0U, ZR_PARSER_CFG_EDGE_CLEANUP);
    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, returnSource);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_RETURN, block->terminatorKind);
    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, suspendSource);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_SUSPEND, block->terminatorKind);
    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, resumeSource);
    assert_edge_kind(block, 0U, ZR_PARSER_CFG_EDGE_RESUME);

    ZrParser_Cfg_Free(g_state, &cfg);
}

static void test_cfg_builder_labels_return_and_branch_edges(void) {
    SZrParserCfg cfg;
    SZrAstNode *returnNode = test_ast_node(ZR_AST_RETURN_STATEMENT, 0U, 6U);
    SZrAstNode *returnScript = script_with_statement(returnNode);
    SZrAstNode *condition;
    SZrAstNode *ifNode;
    SZrAstNode *ifScript;
    const SZrParserCfgBlock *returnBlock = ZR_NULL;
    const SZrParserCfgBlock *ifBlock = ZR_NULL;
    TZrSize index;

    ZrParser_Cfg_Init(g_state, &cfg);
    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, returnScript));
    for (index = 0; index < cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, index);
        if (block != ZR_NULL && block->statement == returnNode) {
            returnBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_RETURN, returnBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(1U, returnBlock->successorCount);
    assert_edge_kind(returnBlock, 0U, ZR_PARSER_CFG_EDGE_RETURN);
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, returnScript);

    condition = test_ast_node(ZR_AST_IDENTIFIER_LITERAL, 4U, 8U);
    ifNode = test_ast_node(ZR_AST_IF_EXPRESSION, 0U, 32U);
    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = empty_block(10U, 16U);
    ifNode->data.ifExpression.elseExpr = empty_block(20U, 28U);
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    ifScript = script_with_statement(ifNode);

    ZrParser_Cfg_Init(g_state, &cfg);
    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, ifScript));
    for (index = 0; index < cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, index);
        if (block != ZR_NULL && block->statement == ifNode) {
            ifBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(ifBlock);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_BRANCH, ifBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, ifBlock->successorCount);
    assert_edge_kind(ifBlock, 0U, ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    assert_edge_kind(ifBlock, 1U, ZR_PARSER_CFG_EDGE_FALSE_BRANCH);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, ifScript);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_place_graph_covers_all_bases_and_projections);
    RUN_TEST(test_place_overlap_reports_all_four_states);
    RUN_TEST(test_cfg_edges_are_extensible_and_typed);
    RUN_TEST(test_cfg_builder_labels_return_and_branch_edges);
    return UNITY_END();
}
