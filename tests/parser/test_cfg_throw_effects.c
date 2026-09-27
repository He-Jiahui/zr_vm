#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

/* 同一套 CFG 事实查询覆盖 lambda 调用、catch 类型匹配与 receiver guard 抛出边。 */
static SZrState *g_state;

/* Unity 为每例创建独立运行时，避免语义事实跨例残留。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 即使断言中止用例，Unity 仍调用此处销毁运行时。 */
/* BUG: 下方独立 context 或编译器内 semanticContext 创建成功后若断言失败，
 * 各自的显式 Free 被 Unity longjmp 跳过，运行时析构不回收原生分配。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 以具体语句位置查 CFG 输出的不可达事实；无事实表示可达。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(
        SZrSemanticContext *context,
        SZrAstNode *node) {
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(node);
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
            context,
            node->location);
}

/* 源名归运行时 GC 管理，返回的 AST 由用例或公共断言器释放。 */
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

/* lambda 场景从脚本第一条语句取得 try 节点。 */
static SZrAstNode *first_statement(SZrAstNode *script) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_TRUE(script->data.script.statements->count > 0);
    return script->data.script.statements->nodes[0];
}

/* 验证 catch 结构并取其首条语句作为事实查询点。 */
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

/* 单 catch 的 receiver guard 场景复用第零个 catch 提取逻辑。 */
static SZrAstNode *first_catch_statement(SZrAstNode *tryNode) {
    return first_catch_statement_at(tryNode, 0);
}

/* 先编译填充 receiver guard 语义事实，再用同一 context 建函数 CFG。 */
static void assert_guard_catch_reachability(
        const char *source,
        TZrBool expectedReachable) {
    SZrAstNode *script = parse_source(source);
    SZrAstNode *functionNode;
    SZrAstNode *tryNode;
    SZrAstNode *catchStmt;
    SZrAstNodeArray *functionBody;
    const SZrSemanticReachabilityFact *fact;
    SZrCompilerState compiler;
    SZrParserCfg cfg;
    TZrSize statementIndex;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            2u, (TZrUInt32)script->data.script.statements->count);
    functionNode = script->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    functionBody = functionNode->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL(functionBody);
    tryNode = ZR_NULL;
    for (statementIndex = 0u;
         statementIndex < functionBody->count;
         statementIndex++) {
        if (functionBody->nodes[statementIndex] != ZR_NULL &&
            functionBody->nodes[statementIndex]->type ==
                    ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
            tryNode = functionBody->nodes[statementIndex];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(tryNode);
    catchStmt = first_catch_statement(tryNode);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_script(&compiler, script);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);

    /* 无 semanticContext 的建图入口无法消费编译阶段的 guard 事实。 */
    ZrParser_Cfg_Init(g_state, &cfg);
    TEST_ASSERT_TRUE(ZrParser_Cfg_BuildWithSemanticContext(
            g_state, &cfg, functionNode, compiler.semanticContext));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(
            compiler.semanticContext, &cfg));
    fact = reachability_fact_at(compiler.semanticContext, catchStmt);
    if (expectedReachable) {
        TEST_ASSERT_NULL(fact);
    } else {
        TEST_ASSERT_NOT_NULL(fact);
        TEST_ASSERT_EQUAL_INT(
                ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    }

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

/* 无抛出的 lambda 立即调用不应向 catch 建可达边。 */
static void test_cfg_marks_catch_unreachable_for_nonthrowing_lambda_iife(void) {
    const char *source =
            "try {\n"
            "    (fn(value: int): int { return value + 1; })(1);\n"
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

/* lambda 抛出 string 时，仅同类型 catch 可达，异型和后续兜底均不可达。 */
static void test_cfg_uses_lambda_iife_throw_profile_for_typed_catch_matching(void) {
    const char *source =
            "try {\n"
            "    (fn(): void { throw \"boom\"; })();\n"
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

/* weak 直接成员读取需要空引用 guard，其 catch 保持可达。 */
static void test_cfg_marks_direct_weak_receiver_guard_as_throwing(void) {
    const char *source =
            "resource class Box { pub var value: int; }\n"
            "fn read(weak: Weak<Box>): int {\n"
            "  try { weak.value; }\n"
            "  catch (error: NullReferenceError) { return 1; }\n"
            "  return 0;\n"
            "}\n";

    assert_guard_catch_reachability(source, ZR_TRUE);
}

/* wake 后的可空接收者直接读取仍可能触发 guard 抛出。 */
static void test_cfg_marks_direct_nullable_receiver_guard_as_throwing(void) {
    const char *source =
            "resource class Box { pub var value: int; }\n"
            "fn read(weak: Weak<Box>): int {\n"
            "  var box = wake(weak);\n"
            "  try { box.value; }\n"
            "  catch (error: NullReferenceError) { return 1; }\n"
            "  return 0;\n"
            "}\n";

    assert_guard_catch_reachability(source, ZR_TRUE);
}

/* 可选链读取绕开直接 guard，不应让空引用 catch 可达。 */
static void test_cfg_keeps_optional_weak_guard_out_of_throw_profile(void) {
    const char *source =
            "resource class Box { pub var value: int; }\n"
            "fn read(weak: Weak<Box>): int {\n"
            "  try { weak?.value; }\n"
            "  catch (error: NullReferenceError) { return 1; }\n"
            "  return 0;\n"
            "}\n";

    assert_guard_catch_reachability(source, ZR_FALSE);
}

/* 单独 wake 不等于直接读取，不能把它误计为 guard 抛出源。 */
static void test_cfg_keeps_explicit_wake_out_of_throw_profile(void) {
    const char *source =
            "resource class Box { pub var value: int; }\n"
            "fn read(weak: Weak<Box>): int {\n"
            "  try { wake(weak); }\n"
            "  catch (error: NullReferenceError) { return 1; }\n"
            "  return 0;\n"
            "}\n";

    assert_guard_catch_reachability(source, ZR_FALSE);
}

/* CMake 将本 Unity 目标追加到 language_pipeline core 的执行列表。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_marks_catch_unreachable_for_nonthrowing_lambda_iife);
    RUN_TEST(test_cfg_uses_lambda_iife_throw_profile_for_typed_catch_matching);
    RUN_TEST(test_cfg_marks_direct_weak_receiver_guard_as_throwing);
    RUN_TEST(test_cfg_marks_direct_nullable_receiver_guard_as_throwing);
    RUN_TEST(test_cfg_keeps_optional_weak_guard_out_of_throw_profile);
    RUN_TEST(test_cfg_keeps_explicit_wake_out_of_throw_profile);
    return UNITY_END();
}
