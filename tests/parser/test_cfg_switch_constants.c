#include "unity.h"

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
    range.source = ZrCore_String_Create(g_state, "cfg_switch_constants_test.zr", 28);
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
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 96);

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

static SZrAstNode *identifier_node(const TZrChar *name, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *identifier = test_node(ZR_AST_IDENTIFIER_LITERAL, startOffset, endOffset);

    identifier->data.identifier.name = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);
    TEST_ASSERT_NOT_NULL(identifier->data.identifier.name);
    return identifier;
}

static SZrAstNode *unary_not_expression(SZrAstNode *argument,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_UNARY_EXPRESSION, startOffset, endOffset);

    expression->data.unaryExpression.op.op = "!";
    expression->data.unaryExpression.argument = argument;
    return expression;
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

static SZrAstNode *switch_case_node(SZrAstNode *value, SZrAstNode *body) {
    SZrAstNode *caseNode = test_node(ZR_AST_SWITCH_CASE, 24, 72);

    caseNode->data.switchCase.value = value;
    caseNode->data.switchCase.block = body;
    return caseNode;
}

static SZrAstNode *switch_default_node(SZrAstNode *body) {
    SZrAstNode *defaultNode = test_node(ZR_AST_SWITCH_DEFAULT, 76, 96);

    defaultNode->data.switchDefault.block = body;
    return defaultNode;
}

static SZrAstNode *switch_statement_with_case_and_default(SZrAstNode *expr,
                                                         SZrAstNode *caseNode,
                                                         SZrAstNode *defaultNode) {
    SZrAstNode *switchNode = test_node(ZR_AST_SWITCH_EXPRESSION, 0, 104);

    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(switchNode->data.switchExpression.cases);
    ZrParser_AstNodeArray_Add(g_state, switchNode->data.switchExpression.cases, caseNode);
    switchNode->data.switchExpression.defaultCase = defaultNode;
    switchNode->data.switchExpression.isStatement = ZR_TRUE;
    return switchNode;
}

static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

static void test_cfg_switch_matches_unary_not_selector(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *selectorLiteral = boolean_literal(ZR_FALSE, 9, 14);
    SZrAstNode *selector = unary_not_expression(selectorLiteral, 8, 14);
    SZrAstNode *caseValue = boolean_literal(ZR_TRUE, 28, 32);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(selector, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *defaultFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    defaultFact = reachability_fact_at(context, defaultNode);
    TEST_ASSERT_NOT_NULL(defaultFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, defaultFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, defaultFact->cause);
    TEST_ASSERT_EQUAL_PTR(selector, defaultFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_switch_matches_unary_not_case_value(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *selector = boolean_literal(ZR_TRUE, 8, 12);
    SZrAstNode *caseLiteral = boolean_literal(ZR_FALSE, 29, 34);
    SZrAstNode *caseValue = unary_not_expression(caseLiteral, 28, 34);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(selector, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *defaultFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    defaultFact = reachability_fact_at(context, defaultNode);
    TEST_ASSERT_NOT_NULL(defaultFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, defaultFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, defaultFact->cause);
    TEST_ASSERT_EQUAL_PTR(selector, defaultFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_switch_prunes_short_circuit_false_selector_case(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_FALSE, 8, 13);
    SZrAstNode *right = identifier_node("flag", 17, 21);
    SZrAstNode *selector = logical_expression(left, "&&", right, 8, 21);
    SZrAstNode *caseValue = boolean_literal(ZR_TRUE, 30, 34);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 42, 48);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 38, 52);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(selector, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *caseFact;
    const SZrSemanticReachabilityFact *defaultFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    caseFact = reachability_fact_at(context, caseNode);
    defaultFact = reachability_fact_at(context, defaultNode);
    TEST_ASSERT_NOT_NULL(caseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, caseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, caseFact->cause);
    TEST_ASSERT_EQUAL_PTR(selector, caseFact->causeNode);
    TEST_ASSERT_NULL(defaultFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_switch_matches_unary_not_selector);
    RUN_TEST(test_cfg_switch_matches_unary_not_case_value);
    RUN_TEST(test_cfg_switch_prunes_short_circuit_false_selector_case);
    return UNITY_END();
}
