#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "compiler/expression/compile_expression_internal.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"

enum { PARENT_SNAPSHOT_LIMIT = 64 };

typedef enum EParentArrayKind {
    PARENT_ARRAY_DIRECT,
    PARENT_ARRAY_PLACE_PROJECTIONS,
    PARENT_ARRAY_CFG_EDGES
} EParentArrayKind;

typedef struct SParentArraySnapshot {
    EParentArrayKind kind;
    const SZrArray *array;
    TZrSize index;
    SZrArray header;
    SZrCompilerArraySnapshot contents;
    const char *label;
} SParentArraySnapshot;

static SZrState *g_state;
static SZrParserState g_parser;
static TZrBool g_parserInitialized;
static SZrAstNode *g_ast;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrFunction *g_parentFunction;
static SZrCompilerSemanticIrIsolation g_parentIr;
static SParentArraySnapshot g_snapshots[PARENT_SNAPSHOT_LIMIT];
static TZrSize g_snapshotCount;
static TZrUInt32 g_seedStackSlot;
static TZrTypeId g_seedTypeId;
static TZrSymbolId g_seedSymbolId;
static TZrPlaceId g_seedPlaceId;
static TZrValueId g_seedValueId;
static SZrFileRange g_seedRange;

void setUp(void) {
    memset(&g_parser, 0, sizeof(g_parser));
    memset(&g_compiler, 0, sizeof(g_compiler));
    memset(&g_parentIr, 0, sizeof(g_parentIr));
    memset(g_snapshots, 0, sizeof(g_snapshots));
    g_parserInitialized = ZR_FALSE;
    g_compilerInitialized = ZR_FALSE;
    g_ast = ZR_NULL;
    g_parentFunction = ZR_NULL;
    g_snapshotCount = 0U;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrSize index;

    if (g_compilerInitialized) {
        for (index = 0U; index < g_snapshotCount; ++index) {
            compiler_release_array_snapshot(
                    &g_compiler, &g_snapshots[index].contents);
        }
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    g_snapshotCount = 0U;
    if (g_parserInitialized) {
        ZrParser_State_Free(&g_parser);
        g_parserInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        /* Current and generated functions are raw GC objects owned by the VM. */
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
    g_parentFunction = ZR_NULL;
}

static SZrAstNodeArray *parse_source(const char *source, const char *sourceName) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)sourceName);

    TEST_ASSERT_NOT_NULL(name);
    ZrParser_State_Init(&g_parser, g_state, source, strlen(source), name);
    g_parserInitialized = ZR_TRUE;
    g_parser.suppressErrorOutput = ZR_TRUE;
    g_ast = ZrParser_ParseWithState(&g_parser);
    TEST_ASSERT_FALSE_MESSAGE(g_parser.hasError, g_parser.errorMessage);
    ZrParser_State_Free(&g_parser);
    g_parserInitialized = ZR_FALSE;
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    return g_ast->data.script.statements;
}

static void assert_range_equal(SZrFileRange expected, SZrFileRange actual) {
    TEST_ASSERT_EQUAL_PTR(expected.source, actual.source);
    TEST_ASSERT_EQUAL_UINT64(expected.start.offset, actual.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expected.end.offset, actual.end.offset);
    TEST_ASSERT_EQUAL_INT(expected.start.line, actual.start.line);
    TEST_ASSERT_EQUAL_INT(expected.start.column, actual.start.column);
    TEST_ASSERT_EQUAL_INT(expected.end.line, actual.end.line);
    TEST_ASSERT_EQUAL_INT(expected.end.column, actual.end.column);
}

static void assert_seed_identity(void) {
    const SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_find_slot(&g_compiler, g_seedStackSlot);
    const SZrParserPlace *place;
    const SZrSemanticIrValue *value;

    TEST_ASSERT_NOT_NULL(slot);
    TEST_ASSERT_EQUAL_UINT32(g_seedStackSlot, slot->stackSlot);
    TEST_ASSERT_EQUAL_UINT32(g_seedTypeId, slot->typeId);
    TEST_ASSERT_EQUAL_UINT32(g_seedSymbolId, slot->symbolId);
    TEST_ASSERT_EQUAL_UINT32(g_seedPlaceId, slot->placeId);
    TEST_ASSERT_EQUAL_UINT32(g_seedValueId, slot->valueId);
    place = ZrParser_PlaceGraph_Get(&g_compiler.preSemanticIr.places, slot->placeId);
    TEST_ASSERT_NOT_NULL(place);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_BASE_LOCAL, place->base.kind);
    TEST_ASSERT_EQUAL_UINT32(g_seedStackSlot, place->base.identity);
    TEST_ASSERT_EQUAL_UINT32(g_seedTypeId, place->typeId);
    assert_range_equal(g_seedRange, place->sourceRange);
    value = ZrParser_SemanticIr_Value(&g_compiler.preSemanticIr, slot->valueId);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_UINT32(g_seedTypeId, value->typeId);
}

static void prepare_parent(SZrAstNodeArray *statements) {
    SZrAstNode *seed;
    SZrAstNode *pattern;
    const SZrTypeBinding *binding;
    const SZrCompilerSemanticIrSlot *slot;
    const SZrParserPlace *place;

    TEST_ASSERT_GREATER_THAN_UINT(0U, statements->count);
    seed = statements->nodes[0];
    TEST_ASSERT_NOT_NULL(seed);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, seed->type);
    pattern = seed->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, pattern->type);
    TEST_ASSERT_EQUAL_STRING("seed", ZrCore_String_GetNativeString(
            pattern->data.identifier.name));
    TEST_ASSERT_NOT_NULL(seed->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(seed->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL,
                          seed->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_INT64(7, seed->data.variableDeclaration.value->data.integerLiteral.value);

    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_compiler.currentAst = g_ast;
    g_compiler.scriptAst = g_ast;
    g_compiler.currentFunctionNode = g_ast;
    g_parentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_parentFunction);
    g_compiler.currentFunction = g_parentFunction;
    TEST_ASSERT_NOT_NULL(g_compiler.semanticContext);
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    TEST_ASSERT_TRUE(g_compiler.preSemanticIrInitialized);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    ZrParser_Statement_Compile(&g_compiler, seed);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    g_seedStackSlot = find_local_var_with_context(
            &g_compiler, pattern->data.identifier.name,
            pattern->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, g_seedStackSlot);

    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            g_compiler.typeEnv, pattern->data.identifier.name,
            pattern->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    g_seedTypeId = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, g_seedTypeId);
    TEST_ASSERT_EQUAL_UINT32(g_seedTypeId, binding->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    TEST_ASSERT_EQUAL_PTR(pattern->location.source, binding->declarationRange.source);
    g_seedSymbolId = binding->symbolId;
    slot = compiler_semantic_ir_find_slot(&g_compiler, g_seedStackSlot);
    TEST_ASSERT_NOT_NULL(slot);
    g_seedPlaceId = slot->placeId;
    g_seedValueId = slot->valueId;
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, g_seedPlaceId);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, g_seedValueId);
    place = ZrParser_PlaceGraph_Get(&g_compiler.preSemanticIr.places, g_seedPlaceId);
    TEST_ASSERT_NOT_NULL(place);
    g_seedRange = place->sourceRange;
    TEST_ASSERT_NOT_NULL(g_seedRange.source);
    TEST_ASSERT_EQUAL_PTR(seed->location.source, g_seedRange.source);
    TEST_ASSERT_EQUAL_INT(1, g_seedRange.start.line);
    TEST_ASSERT_GREATER_THAN_UINT64(g_seedRange.start.offset, g_seedRange.end.offset);
    assert_range_equal(binding->declarationRange, g_seedRange);
    assert_range_equal(pattern->location, g_seedRange);
    assert_seed_identity();
    TEST_ASSERT_GREATER_THAN_UINT64(0U, g_compiler.preSemanticIr.values.length);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, g_compiler.preSemanticIr.instructions.length);
}

static void snapshot_array(const SZrArray *array, EParentArrayKind kind,
                           TZrSize index, const char *label) {
    SParentArraySnapshot *snapshot;

    TEST_ASSERT_LESS_THAN_UINT64(PARENT_SNAPSHOT_LIMIT, g_snapshotCount);
    /* Register ownership before an assertion can leave through Unity. */
    snapshot = &g_snapshots[g_snapshotCount++];
    snapshot->kind = kind;
    snapshot->array = array;
    snapshot->index = index;
    snapshot->header = *array;
    snapshot->label = label;
    TEST_ASSERT_TRUE_MESSAGE(compiler_capture_array_snapshot(
            &g_compiler, array, &snapshot->contents), label);
}

static void snapshot_parent_ir(void) {
    TZrSize index;

    g_parentIr.function = g_compiler.preSemanticIr;
    g_parentIr.initialized = g_compiler.preSemanticIrInitialized;
    g_parentIr.validated = g_compiler.preSemanticIrValidated;
    g_parentIr.cfgActive = g_compiler.preSemanticIrCfgActive;
    g_parentIr.cfgStartupSuppressed = g_compiler.preSemanticIrCfgStartupSuppressed;
    g_parentIr.cfgStartupBlocked = g_compiler.preSemanticIrCfgStartupBlocked;
    g_parentIr.cfgTerminated = g_compiler.preSemanticIrCfgTerminated;
    g_parentIr.cfgAbruptIsLocal = g_compiler.preSemanticIrCfgAbruptIsLocal;
    g_parentIr.cfgCatchUsed = g_compiler.preSemanticIrCfgCatchUsed;
    g_parentIr.cfgBlock = g_compiler.preSemanticIrCfgBlock;
    g_parentIr.cfgStart = g_compiler.preSemanticIrCfgStart;
    g_parentIr.cfgCatchBlock = g_compiler.preSemanticIrCfgCatchBlock;
    g_parentIr.cfgFinallyPlan = g_compiler.preSemanticIrCfgFinallyPlan;
#define SNAPSHOT_IR_ARRAY(member) \
    snapshot_array(&g_compiler.preSemanticIr.member, PARENT_ARRAY_DIRECT, 0U, #member)
    SNAPSHOT_IR_ARRAY(places.places);
    SNAPSHOT_IR_ARRAY(cfg.blocks);
    SNAPSHOT_IR_ARRAY(locals);
    SNAPSHOT_IR_ARRAY(values);
    SNAPSHOT_IR_ARRAY(instructions);
    SNAPSHOT_IR_ARRAY(valueOperands);
    SNAPSHOT_IR_ARRAY(regions);
    SNAPSHOT_IR_ARRAY(cleanupScopes);
    SNAPSHOT_IR_ARRAY(sourceMap);
    SNAPSHOT_IR_ARRAY(loanFacts);
    SNAPSHOT_IR_ARRAY(escapeFacts);
    SNAPSHOT_IR_ARRAY(contiguousViewFacts);
    SNAPSHOT_IR_ARRAY(boundsFacts);
    SNAPSHOT_IR_ARRAY(scalarScratchProofs);
#undef SNAPSHOT_IR_ARRAY
    snapshot_array(&g_compiler.preSemanticIrSlots, PARENT_ARRAY_DIRECT, 0U, "slots");
    snapshot_array(&g_compiler.preSemanticIrReceiverLoanIds,
                   PARENT_ARRAY_DIRECT, 0U, "receiver loans");
    for (index = 0U; index < g_compiler.preSemanticIr.places.places.length; ++index) {
        const SZrParserPlace *place = (const SZrParserPlace *)ZrCore_Array_Get(
                &g_compiler.preSemanticIr.places.places, index);
        snapshot_array(&place->projections, PARENT_ARRAY_PLACE_PROJECTIONS,
                       index, "place projections");
    }
    for (index = 0U; index < g_compiler.preSemanticIr.cfg.blocks.length; ++index) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                &g_compiler.preSemanticIr.cfg.blocks, index);
        snapshot_array(&block->outgoingEdges, PARENT_ARRAY_CFG_EDGES,
                       index, "CFG edges");
    }
}

static const SZrArray *current_snapshot_array(const SParentArraySnapshot *snapshot) {
    if (snapshot->kind == PARENT_ARRAY_PLACE_PROJECTIONS) {
        const SZrParserPlace *place;

        TEST_ASSERT_LESS_THAN_UINT64(g_compiler.preSemanticIr.places.places.length,
                                    snapshot->index);
        place = (const SZrParserPlace *)ZrCore_Array_Get(
                &g_compiler.preSemanticIr.places.places, snapshot->index);
        return &place->projections;
    }
    if (snapshot->kind == PARENT_ARRAY_CFG_EDGES) {
        const SZrParserCfgBlock *block;

        TEST_ASSERT_LESS_THAN_UINT64(g_compiler.preSemanticIr.cfg.blocks.length,
                                    snapshot->index);
        block = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                &g_compiler.preSemanticIr.cfg.blocks, snapshot->index);
        return &block->outgoingEdges;
    }
    return snapshot->array;
}

static void assert_parent_ir_restored(void) {
    TZrSize index;

    TEST_ASSERT_EQUAL_PTR(g_parentFunction, g_compiler.currentFunction);
    TEST_ASSERT_EQUAL_PTR(g_parentIr.function.state, g_compiler.preSemanticIr.state);
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.function.symbolId, g_compiler.preSemanticIr.symbolId);
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.function.callableTypeId,
                            g_compiler.preSemanticIr.callableTypeId);
    TEST_ASSERT_EQUAL_PTR(g_parentIr.function.places.state,
                         g_compiler.preSemanticIr.places.state);
    TEST_ASSERT_EQUAL_PTR(g_parentIr.function.cfg.state, g_compiler.preSemanticIr.cfg.state);
    TEST_ASSERT_EQUAL_PTR(g_parentIr.function.cfg.semanticContext,
                         g_compiler.preSemanticIr.cfg.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.function.cfg.entryBlockId,
                            g_compiler.preSemanticIr.cfg.entryBlockId);
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.function.cfg.exitBlockId,
                            g_compiler.preSemanticIr.cfg.exitBlockId);
#define ASSERT_IR_FLAG(saved, member) \
    TEST_ASSERT_EQUAL_INT(g_parentIr.saved, g_compiler.member)
    ASSERT_IR_FLAG(initialized, preSemanticIrInitialized);
    ASSERT_IR_FLAG(validated, preSemanticIrValidated);
    ASSERT_IR_FLAG(cfgActive, preSemanticIrCfgActive);
    ASSERT_IR_FLAG(cfgStartupSuppressed, preSemanticIrCfgStartupSuppressed);
    ASSERT_IR_FLAG(cfgStartupBlocked, preSemanticIrCfgStartupBlocked);
    ASSERT_IR_FLAG(cfgTerminated, preSemanticIrCfgTerminated);
    ASSERT_IR_FLAG(cfgAbruptIsLocal, preSemanticIrCfgAbruptIsLocal);
    ASSERT_IR_FLAG(cfgCatchUsed, preSemanticIrCfgCatchUsed);
#undef ASSERT_IR_FLAG
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.cfgBlock, g_compiler.preSemanticIrCfgBlock);
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.cfgStart, g_compiler.preSemanticIrCfgStart);
    TEST_ASSERT_EQUAL_UINT32(g_parentIr.cfgCatchBlock, g_compiler.preSemanticIrCfgCatchBlock);
    TEST_ASSERT_EQUAL_PTR(g_parentIr.cfgFinallyPlan, g_compiler.preSemanticIrCfgFinallyPlan);
    for (index = 0U; index < g_snapshotCount; ++index) {
        const SParentArraySnapshot *snapshot = &g_snapshots[index];
        const SZrArray *array = current_snapshot_array(snapshot);

        TEST_ASSERT_EQUAL_INT_MESSAGE(snapshot->header.isValid, array->isValid, snapshot->label);
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(snapshot->header.elementSize,
                                        array->elementSize, snapshot->label);
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(snapshot->header.length, array->length, snapshot->label);
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(snapshot->header.capacity, array->capacity, snapshot->label);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(snapshot->header.head, array->head, snapshot->label);
        if (snapshot->contents.length > 0U) {
            TEST_ASSERT_EQUAL_MEMORY_MESSAGE(
                    snapshot->contents.data, array->head,
                    snapshot->contents.length * snapshot->contents.elementSize, snapshot->label);
        }
    }
    assert_seed_identity();
}

static SZrAstNode *lambda_initializer(SZrAstNodeArray *statements, TZrSize index,
                                    EZrAstNodeType literalType) {
    SZrAstNode *declaration;
    SZrAstNode *lambda;
    SZrAstNode *body;
    SZrAstNode *returnNode;

    TEST_ASSERT_LESS_THAN_UINT64(statements->count, index);
    declaration = statements->nodes[index];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    lambda = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_TRUE(lambda->data.lambdaExpression.params == ZR_NULL ||
                     lambda->data.lambdaExpression.params->count == 0U);
    body = lambda->data.lambdaExpression.block;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT(1U, body->data.block.body->count);
    returnNode = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(returnNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    TEST_ASSERT_NOT_NULL(returnNode->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(literalType, returnNode->data.returnStatement.expr->type);
    TEST_ASSERT_EQUAL_PTR(g_seedRange.source, returnNode->location.source);
    TEST_ASSERT_GREATER_THAN_INT(g_seedRange.end.line, returnNode->location.start.line);
    return lambda;
}

static void test_named_function_preserves_parent_semantic_ir(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "fn helper(): int { var value = 11; return value; }\n",
            "lambda_ir_named_control.zr");

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statements->nodes[1]->type);
    prepare_parent(statements);
    snapshot_parent_ir();
    compile_function_declaration(&g_compiler, statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    assert_parent_ir_restored();
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&g_compiler));
}

static void test_float_lambda_preserves_parent_semantic_ir(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var child = fn() => { return 3.14; };\n",
            "lambda_ir_float.zr");
    SZrAstNode *lambda;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    prepare_parent(statements);
    lambda = lambda_initializer(statements, 1U, ZR_AST_FLOAT_LITERAL);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, lambda);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    assert_parent_ir_restored();
}

static void test_integer_lambda_temporary_does_not_collide_with_parent_local(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var child = fn() => { return 42; };\n",
            "lambda_ir_integer_collision.zr");
    SZrAstNode *lambda;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    prepare_parent(statements);
    TEST_ASSERT_EQUAL_UINT32(0U, g_seedStackSlot);
    lambda = lambda_initializer(statements, 1U, ZR_AST_INTEGER_LITERAL);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, lambda);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    assert_parent_ir_restored();
}

static void test_sibling_lambdas_leave_parent_ir_unchanged(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var first = fn() => { return 3.14; };\n"
            "var second = fn() => { return 42; };\n",
            "lambda_ir_siblings.zr");
    SZrAstNode *first;
    SZrAstNode *second;

    TEST_ASSERT_EQUAL_UINT(3U, statements->count);
    prepare_parent(statements);
    first = lambda_initializer(statements, 1U, ZR_AST_FLOAT_LITERAL);
    second = lambda_initializer(statements, 2U, ZR_AST_INTEGER_LITERAL);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, first);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    compile_lambda_expression(&g_compiler, second);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    assert_parent_ir_restored();
    TEST_ASSERT_EQUAL_UINT64(2U, g_compiler.childFunctions.length);
}

static void test_captured_lambda_followed_by_parent_store_returns_eight(void) {
    SZrAstNodeArray *statements = parse_source(
            "fn outer(): int {\n"
            "  var captured = 4;\n"
            "  var direct = (fn(delta: int) => { return captured + delta; })(3);\n"
            "  direct = direct + 1;\n"
            "  return direct;\n"
            "}\n"
            "return outer();\n",
            "lambda_ir_capture_store.zr");
    SZrAstNode *body;
    SZrAstNode *store;
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statements->nodes[0]->type);
    body = statements->nodes[0]->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT(4U, body->data.block.body->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->data.block.body->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->data.block.body->nodes[1]->type);
    store = body->data.block.body->nodes[2];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, store->type);
    TEST_ASSERT_NOT_NULL(store->data.expressionStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, store->data.expressionStatement.expr->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, body->data.block.body->nodes[3]->type);
    function = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "captured lambda and later parent store must compile");
    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result), "captured lambda and later parent store must execute");
    TEST_ASSERT_EQUAL_INT64(8, result);
}

static void test_captured_lambda_parent_store_inside_source_if_returns_eight(void) {
    SZrAstNodeArray *statements = parse_source(
            "pub enum Mode: int { Idle = 1; Hot = 2; }\n"
            "fn outer(): int {\n"
            "  var captured = 4;\n"
            "  var direct = (fn(delta: int) => { return captured + delta; })(3);\n"
            "  var mode = Mode.Hot;\n"
            "  if (mode == Mode.Hot) { direct = direct + 1; }\n"
            "  return direct;\n"
            "}\n"
            "return outer();\n",
            "lambda_ir_capture_if_store.zr");
    SZrAstNode *body;
    SZrAstNode *branch;
    SZrAstNode *store;
    SZrFunction *function;
    TZrBool compiled;
    TZrBool executed = ZR_FALSE;
    TZrInt64 result = 0;

    TEST_ASSERT_EQUAL_UINT(3U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ENUM_DECLARATION, statements->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statements->nodes[1]->type);
    body = statements->nodes[1]->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT(5U, body->data.block.body->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->data.block.body->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->data.block.body->nodes[1]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, body->data.block.body->nodes[2]->type);
    branch = body->data.block.body->nodes[3];
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, branch->type);
    TEST_ASSERT_TRUE(branch->data.ifExpression.isStatement);
    TEST_ASSERT_NOT_NULL(branch->data.ifExpression.condition);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branch->data.ifExpression.condition->type);
    TEST_ASSERT_EQUAL_STRING("==", branch->data.ifExpression.condition->data.binaryExpression.op.op);
    TEST_ASSERT_NOT_NULL(branch->data.ifExpression.thenExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, branch->data.ifExpression.thenExpr->type);
    TEST_ASSERT_NOT_NULL(branch->data.ifExpression.thenExpr->data.block.body);
    TEST_ASSERT_EQUAL_UINT(1U, branch->data.ifExpression.thenExpr->data.block.body->count);
    store = branch->data.ifExpression.thenExpr->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, store->type);
    TEST_ASSERT_NOT_NULL(store->data.expressionStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, store->data.expressionStatement.expr->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, body->data.block.body->nodes[4]->type);

    function = ZrParser_Compiler_Compile(g_state, g_ast);
    compiled = (TZrBool)(function != ZR_NULL);
    if (compiled) {
        executed = ZrTests_Runtime_Function_ExecuteExpectInt64(
                g_state, function, &result);
    }
    ZrParser_Ast_Free(g_state, g_ast);
    g_ast = ZR_NULL;
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;

    TEST_ASSERT_TRUE_MESSAGE(compiled,
            "captured lambda and parent store inside the source enum if must compile");
    TEST_ASSERT_TRUE_MESSAGE(executed,
            "captured lambda and parent store inside the source enum if must execute");
    TEST_ASSERT_EQUAL_INT64(8, result);
}

static SZrAstNode *lambda_with_local_initializer(
        SZrAstNodeArray *statements, EZrAstNodeType literalType,
        EZrAstNodeType finalStatementType) {
    SZrAstNode *declaration;
    SZrAstNode *lambda;
    SZrAstNode *body;
    SZrAstNode *local;
    SZrAstNode *last;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    declaration = statements->nodes[1];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    lambda = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_TRUE(lambda->data.lambdaExpression.params == ZR_NULL ||
                     lambda->data.lambdaExpression.params->count == 0U);
    body = lambda->data.lambdaExpression.block;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT(2U, body->data.block.body->count);
    local = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(local);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, local->type);
    TEST_ASSERT_NOT_NULL(local->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                          local->data.variableDeclaration.pattern->type);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(
            local->data.variableDeclaration.pattern->data.identifier.name));
    TEST_ASSERT_NOT_NULL(local->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(local->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(literalType, local->data.variableDeclaration.value->type);
    if (literalType == ZR_AST_INTEGER_LITERAL) {
        TEST_ASSERT_EQUAL_INT64(42, local->data.variableDeclaration.value->data.integerLiteral.value);
    } else {
        TEST_ASSERT_EQUAL_DOUBLE(3.14, local->data.variableDeclaration.value->data.floatLiteral.value);
    }
    TEST_ASSERT_EQUAL_PTR(g_seedRange.source, local->location.source);
    TEST_ASSERT_GREATER_THAN_INT(g_seedRange.end.line, local->location.start.line);
    last = body->data.block.body->nodes[1];
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_EQUAL_INT(finalStatementType, last->type);
    if (finalStatementType == ZR_AST_RETURN_STATEMENT) {
        TEST_ASSERT_NOT_NULL(last->data.returnStatement.expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, last->data.returnStatement.expr->type);
        TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(
                last->data.returnStatement.expr->data.identifier.name));
    } else {
        TEST_ASSERT_TRUE(last->data.breakContinueStatement.isBreak);
        TEST_ASSERT_NULL(last->data.breakContinueStatement.expr);
    }
    return lambda;
}

static void assert_child_local_identity(SZrAstNode *lambda, EZrValueType valueType) {
    SZrFunction *child;
    const SZrFunctionTypedLocalBinding *local;
    SZrAstNode *pattern = lambda->data.lambdaExpression.block->data.block.body
            ->nodes[0]->data.variableDeclaration.pattern;
    TZrTypeId typeId = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, valueType);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, typeId);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.childFunctions.length);
    child = *(SZrFunction **)ZrCore_Array_Get(&g_compiler.childFunctions, 0U);
    TEST_ASSERT_NOT_NULL(child);
    TEST_ASSERT_EQUAL_UINT32(1U, child->typedLocalBindingLength);
    TEST_ASSERT_NOT_NULL(child->typedLocalBindings);
    local = &child->typedLocalBindings[0];
    TEST_ASSERT_EQUAL_PTR(pattern->data.identifier.name, local->name);
    TEST_ASSERT_EQUAL_UINT32(0U, local->stackSlot);
    TEST_ASSERT_EQUAL_INT(valueType, local->type.baseType);
    TEST_ASSERT_EQUAL_UINT32(typeId, local->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, local->symbolId);
    TEST_ASSERT_NOT_EQUAL(g_seedSymbolId, local->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, local->placeId);
    TEST_ASSERT_EQUAL_UINT32(pattern->location.start.line, local->declarationStartLine);
    TEST_ASSERT_EQUAL_UINT32(pattern->location.start.column, local->declarationStartColumn);
    TEST_ASSERT_EQUAL_UINT32(pattern->location.end.line, local->declarationEndLine);
    TEST_ASSERT_EQUAL_UINT32(pattern->location.end.column, local->declarationEndColumn);
}

static void test_integer_lambda_local_initializer_preserves_parent_semantic_ir(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var child = fn() => { var value: int = 42; return value; };\n",
            "lambda_ir_integer_local.zr");
    SZrAstNode *lambda;

    prepare_parent(statements);
    TEST_ASSERT_EQUAL_UINT32(0U, g_seedStackSlot);
    lambda = lambda_with_local_initializer(
            statements, ZR_AST_INTEGER_LITERAL, ZR_AST_RETURN_STATEMENT);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, lambda);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    assert_child_local_identity(lambda, ZR_VALUE_TYPE_INT64);
    assert_parent_ir_restored();
}

static void test_float_lambda_local_initializer_preserves_parent_semantic_ir(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var child = fn() => { var value: float = 3.14; return value; };\n",
            "lambda_ir_float_local.zr");
    SZrAstNode *lambda;

    prepare_parent(statements);
    TEST_ASSERT_EQUAL_UINT32(0U, g_seedStackSlot);
    lambda = lambda_with_local_initializer(
            statements, ZR_AST_FLOAT_LITERAL, ZR_AST_RETURN_STATEMENT);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, lambda);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    assert_child_local_identity(lambda, ZR_VALUE_TYPE_DOUBLE);
    assert_parent_ir_restored();
}

static void test_rejected_lambda_body_restores_parent_semantic_ir(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var child = fn() => { var value: float = 3.14; break; };\n",
            "lambda_ir_rejected_body.zr");
    SZrAstNode *lambda;

    prepare_parent(statements);
    TEST_ASSERT_EQUAL_UINT32(0U, g_seedStackSlot);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.loopLabelStack.length);
    lambda = lambda_with_local_initializer(
            statements, ZR_AST_FLOAT_LITERAL, ZR_AST_BREAK_CONTINUE_STATEMENT);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, lambda);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "break statement not inside a loop"));
    assert_range_equal(lambda->data.lambdaExpression.block->data.block.body
            ->nodes[1]->location, g_compiler.errorLocation);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.childFunctions.length);
    assert_parent_ir_restored();
}


#include "test_lambda_local_range_cases.h"

static void test_integer_lambda_local_metadata_ignores_parent_same_name_float(void) {
    SZrAstNodeArray *statements = parse_source(
            "var seed: int = 7;\n"
            "var value: float = 3.14;\n"
            "var child = fn() => { var value: int = 42; return value; };\n",
            "lambda_ir_same_name_local_metadata.zr");
    SZrAstNode *parentDecl, *parentPattern, *lambda, *body, *childDecl, *childPattern;
    SZrTypeEnvironment *parentEnv;
    const SZrTypeBinding *binding;
    const SZrCanonicalTypeNode *canonical;
    SZrParserSemanticSymbolQuery query = {0};
    const SZrSemanticReferenceFact *declaration;
    SZrFileRange parentRange;
    struct {
        TZrBool compiled, metadataPresent, queried, declarationMatches, rangeMatches;
        TZrBool parentBindingRestored, parentQueryMatches;
        TZrTypeId typeId, queryTypeId, expectedTypeId, parentTypeId;
        TZrSymbolId symbolId, querySymbolId, parentSymbolId, seedSymbolId;
        TZrPlaceId placeId;
        TZrUInt32 stackSlot, canonicalKind, canonicalPrimitive;
        EZrValueType baseType, parentBaseType;
        char error[256];
    } out = {0};

#define LAMBDA_METADATA_SAME_RANGE(left, right) \
    ((left).source == (right).source && \
     (left).start.offset == (right).start.offset && (left).end.offset == (right).end.offset && \
     (left).start.line == (right).start.line && (left).end.line == (right).end.line && \
     (left).start.column == (right).start.column && (left).end.column == (right).end.column)

    TEST_ASSERT_EQUAL_UINT(3U, statements->count);
    prepare_parent(statements);
    parentDecl = statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, parentDecl->type);
    parentPattern = parentDecl->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(parentPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, parentPattern->type);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(
            parentPattern->data.identifier.name));
    ZrParser_Statement_Compile(&g_compiler, parentDecl);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    parentEnv = g_compiler.typeEnv;
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            parentEnv, parentPattern->data.identifier.name,
            parentPattern->data.identifier.syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    assert_range_equal(parentPattern->location, binding->declarationRange);
    parentRange = binding->declarationRange;
    out.parentTypeId = binding->typeId;
    out.parentSymbolId = binding->symbolId;
    out.parentBaseType = binding->type.baseType;
    out.expectedTypeId = g_seedTypeId;
    out.seedSymbolId = g_seedSymbolId;
    canonical = ZrParser_CanonicalType_Find(g_compiler.semanticContext, out.parentTypeId);
    TEST_ASSERT_NOT_NULL(canonical);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, canonical->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, canonical->data.primitive.valueType);

    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statements->nodes[2]->type);
    lambda = statements->nodes[2]->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_TRUE(lambda->data.lambdaExpression.params == ZR_NULL ||
                     lambda->data.lambdaExpression.params->count == 0U);
    body = lambda->data.lambdaExpression.block;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT(2U, body->data.block.body->count);
    childDecl = body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, childDecl->type);
    childPattern = childDecl->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(childPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, childPattern->type);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(
            childPattern->data.identifier.name));
    TEST_ASSERT_NOT_NULL(childDecl->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(childDecl->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, childDecl->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_INT64(42, childDecl->data.variableDeclaration.value->data.integerLiteral.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, body->data.block.body->nodes[1]->type);
    snapshot_parent_ir();
    compile_lambda_expression(&g_compiler, lambda);
    out.compiled = !g_compiler.hasError;
    if (g_compiler.errorMessage != ZR_NULL) {
        strncpy(out.error, g_compiler.errorMessage, sizeof(out.error) - 1U);
        out.error[sizeof(out.error) - 1U] = '\0';
    }
    if (g_compiler.childFunctions.isValid && g_compiler.childFunctions.head != ZR_NULL &&
            g_compiler.childFunctions.length == 1U) {
        const SZrFunction *child = *(SZrFunction **)ZrCore_Array_Get(
                &g_compiler.childFunctions, 0U);
        if (child != ZR_NULL && child->typedLocalBindingLength == 1U &&
                child->typedLocalBindings != ZR_NULL) {
            const SZrFunctionTypedLocalBinding *local = &child->typedLocalBindings[0];
            out.metadataPresent = local->name == childPattern->data.identifier.name;
            out.baseType = local->type.baseType;
            out.typeId = local->typeId;
            out.symbolId = local->symbolId;
            out.placeId = local->placeId;
            out.stackSlot = local->stackSlot;
            out.rangeMatches = lambda_local_range_matches(local, childPattern->location);
            canonical = ZrParser_CanonicalType_Find(g_compiler.semanticContext, local->typeId);
            if (canonical != ZR_NULL) {
                out.canonicalKind = (TZrUInt32)canonical->kind;
                if (canonical->kind == ZR_CANONICAL_TYPE_PRIMITIVE)
                    out.canonicalPrimitive = (TZrUInt32)canonical->data.primitive.valueType;
            }
        }
    }
    out.queried = ZrParser_SemanticQuery_SymbolAt(
            g_compiler.semanticContext, childPattern->location, ZR_NULL, &query);
    if (out.queried) {
        out.queryTypeId = query.typeId;
        out.querySymbolId = query.symbolId;
        declaration = ZrParser_SemanticQuery_DeclarationOf(
                g_compiler.semanticContext, query.symbolId, ZR_NULL);
        out.declarationMatches = declaration != ZR_NULL &&
                declaration->typeId == query.typeId &&
                query.kind == ZR_SEMANTIC_SYMBOL_KIND_VARIABLE &&
                query.declarationNode == childDecl &&
                LAMBDA_METADATA_SAME_RANGE(query.declarationRange, childPattern->location) &&
                LAMBDA_METADATA_SAME_RANGE(declaration->range, childPattern->location);
    }
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            g_compiler.typeEnv, parentPattern->data.identifier.name,
            parentPattern->data.identifier.syntaxContext);
    out.parentBindingRestored = g_compiler.typeEnv == parentEnv && binding != ZR_NULL &&
            binding->typeId == out.parentTypeId && binding->symbolId == out.parentSymbolId &&
            binding->type.baseType == out.parentBaseType && binding->hasDeclarationRange &&
            LAMBDA_METADATA_SAME_RANGE(binding->declarationRange, parentRange);
    if (ZrParser_SemanticQuery_SymbolAt(
            g_compiler.semanticContext, parentPattern->location, ZR_NULL, &query)) {
        declaration = ZrParser_SemanticQuery_DeclarationOf(
                g_compiler.semanticContext, query.symbolId, ZR_NULL);
        out.parentQueryMatches = declaration != ZR_NULL &&
                query.symbolId == out.parentSymbolId && query.typeId == out.parentTypeId &&
                declaration->typeId == out.parentTypeId &&
                query.kind == ZR_SEMANTIC_SYMBOL_KIND_VARIABLE &&
                query.declarationNode == parentDecl &&
                LAMBDA_METADATA_SAME_RANGE(query.declarationRange, parentRange) &&
                LAMBDA_METADATA_SAME_RANGE(declaration->range, parentRange);
    }
    assert_parent_ir_restored();
#undef LAMBDA_METADATA_SAME_RANGE
    tearDown();

    TEST_ASSERT_TRUE_MESSAGE(out.compiled, out.error);
    TEST_ASSERT_TRUE(out.metadataPresent);
    TEST_ASSERT_TRUE(out.queried);
    TEST_ASSERT_TRUE(out.declarationMatches);
    TEST_ASSERT_TRUE(out.rangeMatches);
    TEST_ASSERT_EQUAL_UINT32(0U, out.stackSlot);
    TEST_ASSERT_EQUAL_UINT32(out.expectedTypeId, out.typeId);
    TEST_ASSERT_EQUAL_UINT32(out.queryTypeId, out.typeId);
    TEST_ASSERT_EQUAL_UINT32(out.querySymbolId, out.symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, out.symbolId);
    TEST_ASSERT_NOT_EQUAL(out.parentSymbolId, out.symbolId);
    TEST_ASSERT_NOT_EQUAL(out.seedSymbolId, out.symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, out.placeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, out.canonicalKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, out.canonicalPrimitive);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, out.parentBaseType);
    TEST_ASSERT_NOT_EQUAL(out.parentTypeId, out.typeId);
    TEST_ASSERT_TRUE(out.parentBindingRestored);
    TEST_ASSERT_TRUE(out.parentQueryMatches);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, out.baseType);
    assert_lambda_local_range_coordinate_boundaries();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_named_function_preserves_parent_semantic_ir);
    RUN_TEST(test_float_lambda_preserves_parent_semantic_ir);
    RUN_TEST(test_integer_lambda_temporary_does_not_collide_with_parent_local);
    RUN_TEST(test_sibling_lambdas_leave_parent_ir_unchanged);
    RUN_TEST(test_captured_lambda_followed_by_parent_store_returns_eight);
    RUN_TEST(test_captured_lambda_parent_store_inside_source_if_returns_eight);
    RUN_TEST(test_integer_lambda_local_initializer_preserves_parent_semantic_ir);
    RUN_TEST(test_float_lambda_local_initializer_preserves_parent_semantic_ir);
    RUN_TEST(test_rejected_lambda_body_restores_parent_semantic_ir);
    RUN_TEST(test_integer_lambda_local_metadata_ignores_parent_same_name_float);
    return UNITY_END();
}
