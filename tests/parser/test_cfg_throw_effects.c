#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
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

static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(node);
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            node->location);
}

static SZrAstNode *parse_source(const char *source) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(source);
    sourceName = ZrCore_String_Create(
            g_state,
            "cfg_throw_effects_test.zr",
            strlen("cfg_throw_effects_test.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *first_statement(SZrAstNode *script) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_TRUE(script->data.script.statements->count > 0);
    return script->data.script.statements->nodes[0];
}

static SZrAstNode *first_catch_statement_at(SZrAstNode *tryNode, TZrSize index) {
    SZrAstNode *catchNode;
    SZrAstNode *catchBody;

    TEST_ASSERT_NOT_NULL(tryNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryNode->type);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    TEST_ASSERT_TRUE(
            tryNode->data.tryCatchFinallyStatement.catchClauses->count > index);

    catchNode = tryNode->data.tryCatchFinallyStatement.catchClauses->nodes[index];
    TEST_ASSERT_NOT_NULL(catchNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CATCH_CLAUSE, catchNode->type);

    catchBody = catchNode->data.catchClause.block;
    TEST_ASSERT_NOT_NULL(catchBody);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, catchBody->type);
    TEST_ASSERT_NOT_NULL(catchBody->data.block.body);
    TEST_ASSERT_TRUE(catchBody->data.block.body->count > 0);
    return catchBody->data.block.body->nodes[0];
}

static SZrAstNode *first_catch_statement(SZrAstNode *tryNode) {
    return first_catch_statement_at(tryNode, 0);
}

static void test_cfg_marks_catch_unreachable_for_nonthrowing_lambda_iife(void) {
    const char *source =
            "try {\n"
            "    ((value: int) -> { return value + 1; })(1);\n"
            "} catch (e) {\n"
            "    \"caught\";\n"
            "}\n";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *script = parse_source(source);
    SZrAstNode *tryNode = first_statement(script);
    SZrAstNode *catchStmt = first_catch_statement(tryNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, catchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_uses_lambda_iife_throw_profile_for_typed_catch_matching(void) {
    const char *source =
            "try {\n"
            "    (() -> { throw \"boom\"; })();\n"
            "} catch (e: int) {\n"
            "    \"int\";\n"
            "} catch (e: string) {\n"
            "    \"string\";\n"
            "} catch (e) {\n"
            "    \"all\";\n"
            "}\n";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *script = parse_source(source);
    SZrAstNode *tryNode = first_statement(script);
    SZrAstNode *intCatchStmt = first_catch_statement_at(tryNode, 0);
    SZrAstNode *stringCatchStmt = first_catch_statement_at(tryNode, 1);
    SZrAstNode *catchAllStmt = first_catch_statement_at(tryNode, 2);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, intCatchStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    TEST_ASSERT_NULL(reachability_fact_at(context, stringCatchStmt));

    fact = reachability_fact_at(context, catchAllStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_marks_catch_unreachable_for_nonthrowing_lambda_iife);
    RUN_TEST(test_cfg_uses_lambda_iife_throw_profile_for_typed_catch_matching);
    return UNITY_END();
}
