#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/cfg.h"
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
    range.source = ZrCore_String_Create(g_state, "cfg_constant_conditions_test.zr", 31);
    return range;
}

static SZrAstNode *test_node(EZrAstNodeType type, TZrSize startOffset, TZrSize endOffset) {
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

static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 80);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

static SZrAstNode *block_with_statement(SZrAstNode *statement,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

static SZrAstNode *boolean_literal(TZrBool value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
}

static SZrAstNode *integer_literal(TZrInt64 value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    return literal;
}

static SZrAstNode *float_literal(TZrDouble value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_FLOAT_LITERAL, startOffset, endOffset);

    literal->data.floatLiteral.value = value;
    literal->data.floatLiteral.isSingle = ZR_FALSE;
    return literal;
}

static SZrAstNode *string_literal(const TZrChar *value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_STRING_LITERAL, startOffset, endOffset);

    literal->data.stringLiteral.value = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)value);
    TEST_ASSERT_NOT_NULL(literal->data.stringLiteral.value);
    return literal;
}

static SZrAstNode *char_literal(TZrChar value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_CHAR_LITERAL, startOffset, endOffset);

    literal->data.charLiteral.value = value;
    return literal;
}

static SZrAstNode *identifier_node(const TZrChar *name, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *identifier = test_node(ZR_AST_IDENTIFIER_LITERAL, startOffset, endOffset);

    identifier->data.identifier.name = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);
    TEST_ASSERT_NOT_NULL(identifier->data.identifier.name);
    return identifier;
}

static SZrAstNode *unary_expression(const TZrChar *op,
                                    SZrAstNode *argument,
                                    TZrSize startOffset,
                                    TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_UNARY_EXPRESSION, startOffset, endOffset);

    expression->data.unaryExpression.op.op = op;
    expression->data.unaryExpression.argument = argument;
    return expression;
}

static SZrAstNode *unary_not_expression(SZrAstNode *argument,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    return unary_expression("!", argument, startOffset, endOffset);
}

static SZrAstNode *logical_expression(SZrAstNode *left,
                                      const TZrChar *op,
                                      SZrAstNode *right,
                                      TZrSize startOffset,
                                      TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_LOGICAL_EXPRESSION, startOffset, endOffset);

    expression->data.logicalExpression.left = left;
    expression->data.logicalExpression.op = op;
    expression->data.logicalExpression.right = right;
    return expression;
}

static SZrAstNode *binary_expression(SZrAstNode *left,
                                     const TZrChar *op,
                                     SZrAstNode *right,
                                     TZrSize startOffset,
                                     TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_BINARY_EXPRESSION, startOffset, endOffset);

    expression->data.binaryExpression.left = left;
    expression->data.binaryExpression.op.op = op;
    expression->data.binaryExpression.right = right;
    return expression;
}

static SZrAstNode *if_statement(SZrAstNode *condition,
                                SZrAstNode *thenBlock,
                                SZrAstNode *elseBlock) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 64);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = elseBlock;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

static SZrAstNode *while_statement(SZrAstNode *condition, SZrAstNode *body) {
    SZrAstNode *whileNode = test_node(ZR_AST_WHILE_LOOP, 0, 64);

    whileNode->data.whileLoop.cond = condition;
    whileNode->data.whileLoop.block = body;
    whileNode->data.whileLoop.isStatement = ZR_TRUE;
    return whileNode;
}

static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

static void test_cfg_folds_unary_not_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *innerLiteral = boolean_literal(ZR_FALSE, 5, 10);
    SZrAstNode *condition = unary_not_expression(innerLiteral, 4, 10);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 18, 26);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 42, 50);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 14, 30);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 38, 54);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_unary_not_true_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *innerLiteral = boolean_literal(ZR_TRUE, 8, 12);
    SZrAstNode *condition = unary_not_expression(innerLiteral, 7, 12);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 20, 30);
    SZrAstNode *body = block_with_statement(bodyStmt, 16, 34);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_logical_and_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_TRUE, 4, 8);
    SZrAstNode *right = boolean_literal(ZR_FALSE, 12, 17);
    SZrAstNode *condition = logical_expression(left, "&&", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_logical_or_false_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_FALSE, 8, 13);
    SZrAstNode *right = boolean_literal(ZR_FALSE, 17, 22);
    SZrAstNode *condition = logical_expression(left, "||", right, 8, 22);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 30, 40);
    SZrAstNode *body = block_with_statement(bodyStmt, 26, 44);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_short_circuit_false_and_unknown_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_FALSE, 4, 9);
    SZrAstNode *right = identifier_node("flag", 13, 17);
    SZrAstNode *condition = logical_expression(left, "&&", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_short_circuit_true_or_unknown_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_TRUE, 4, 8);
    SZrAstNode *right = identifier_node("flag", 12, 16);
    SZrAstNode *condition = logical_expression(left, "||", right, 4, 16);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 24, 32);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 20, 36);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 44, 60);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_equality_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = integer_literal(1, 4, 5);
    SZrAstNode *right = integer_literal(2, 9, 10);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 10);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 18, 26);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 42, 50);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 14, 30);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 38, 54);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_relational_false_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = integer_literal(1, 8, 9);
    SZrAstNode *right = integer_literal(0, 12, 13);
    SZrAstNode *condition = binary_expression(left, "<", right, 8, 13);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 21, 31);
    SZrAstNode *body = block_with_statement(bodyStmt, 17, 35);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_string_equality_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = string_literal("red", 4, 9);
    SZrAstNode *right = string_literal("blue", 13, 19);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 19);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 27, 35);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 51, 59);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 23, 39);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 47, 63);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_char_inequality_false_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = char_literal('a', 8, 11);
    SZrAstNode *right = char_literal('a', 15, 18);
    SZrAstNode *condition = binary_expression(left, "!=", right, 8, 18);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 26, 36);
    SZrAstNode *body = block_with_statement(bodyStmt, 22, 40);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_float_relational_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = float_literal(1.5, 4, 7);
    SZrAstNode *right = float_literal(2.5, 11, 14);
    SZrAstNode *condition = binary_expression(left, ">=", right, 4, 14);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 22, 30);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 46, 54);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 18, 34);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 42, 58);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_mixed_kind_equality_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = integer_literal(1, 4, 5);
    SZrAstNode *right = string_literal("1", 9, 12);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 12);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 20, 28);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 44, 52);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 16, 32);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 40, 56);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_addition_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *sum = binary_expression(integer_literal(1, 5, 6),
                                        "+",
                                        integer_literal(1, 9, 10),
                                        5,
                                        10);
    SZrAstNode *right = integer_literal(2, 15, 16);
    SZrAstNode *condition = binary_expression(sum, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_subtraction_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *difference = binary_expression(integer_literal(3, 5, 6),
                                               "-",
                                               integer_literal(1, 9, 10),
                                               5,
                                               10);
    SZrAstNode *right = integer_literal(2, 15, 16);
    SZrAstNode *condition = binary_expression(difference, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_multiplication_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *product = binary_expression(integer_literal(2, 5, 6),
                                            "*",
                                            integer_literal(3, 9, 10),
                                            5,
                                            10);
    SZrAstNode *right = integer_literal(6, 15, 16);
    SZrAstNode *condition = binary_expression(product, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_division_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *quotient = binary_expression(integer_literal(9, 5, 6),
                                             "/",
                                             integer_literal(3, 9, 10),
                                             5,
                                             10);
    SZrAstNode *right = integer_literal(3, 15, 16);
    SZrAstNode *condition = binary_expression(quotient, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_modulo_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *remainder = binary_expression(integer_literal(10, 5, 7),
                                              "%",
                                              integer_literal(4, 10, 11),
                                              5,
                                              11);
    SZrAstNode *right = integer_literal(2, 16, 17);
    SZrAstNode *condition = binary_expression(remainder, "==", right, 4, 18);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 26, 34);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 50, 58);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 22, 38);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 46, 62);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_folded_integer_relational_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *sum = binary_expression(integer_literal(1, 5, 6),
                                        "+",
                                        integer_literal(2, 9, 10),
                                        5,
                                        10);
    SZrAstNode *right = integer_literal(2, 14, 15);
    SZrAstNode *condition = binary_expression(sum, ">", right, 4, 16);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 24, 32);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 20, 36);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 44, 60);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_unary_minus_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("-",
                                        integer_literal(3, 6, 7),
                                        5,
                                        7);
    SZrAstNode *right = integer_literal(-3, 12, 14);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 15);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 23, 31);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 47, 55);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 19, 35);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 43, 59);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_unary_plus_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("+",
                                        integer_literal(3, 6, 7),
                                        5,
                                        7);
    SZrAstNode *right = integer_literal(3, 12, 13);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 14);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 22, 30);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 46, 54);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 18, 34);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 42, 58);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_integer_bitwise_not_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("~",
                                        integer_literal(0, 6, 7),
                                        5,
                                        7);
    SZrAstNode *right = integer_literal(-1, 12, 14);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 15);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 23, 31);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 47, 55);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 19, 35);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 43, 59);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_keeps_overflowed_integer_unary_minus_condition_unknown(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("-",
                                        integer_literal(INT64_MIN, 6, 15),
                                        5,
                                        15);
    SZrAstNode *right = integer_literal(INT64_MIN, 20, 29);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 30);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 38, 46);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 62, 70);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 34, 50);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 58, 74);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    TEST_ASSERT_NULL(reachability_fact_at(context, thenStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, elseStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_folds_unary_not_false_if_condition);
    RUN_TEST(test_cfg_folds_unary_not_true_while_condition);
    RUN_TEST(test_cfg_folds_logical_and_false_if_condition);
    RUN_TEST(test_cfg_folds_logical_or_false_while_condition);
    RUN_TEST(test_cfg_folds_short_circuit_false_and_unknown_if_condition);
    RUN_TEST(test_cfg_folds_short_circuit_true_or_unknown_if_condition);
    RUN_TEST(test_cfg_folds_integer_equality_false_if_condition);
    RUN_TEST(test_cfg_folds_integer_relational_false_while_condition);
    RUN_TEST(test_cfg_folds_string_equality_false_if_condition);
    RUN_TEST(test_cfg_folds_char_inequality_false_while_condition);
    RUN_TEST(test_cfg_folds_float_relational_false_if_condition);
    RUN_TEST(test_cfg_folds_mixed_kind_equality_false_if_condition);
    RUN_TEST(test_cfg_folds_integer_addition_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_subtraction_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_multiplication_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_division_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_modulo_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_folded_integer_relational_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_unary_plus_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_unary_minus_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_bitwise_not_equality_true_if_condition);
    RUN_TEST(test_cfg_keeps_overflowed_integer_unary_minus_condition_unknown);
    return UNITY_END();
}
