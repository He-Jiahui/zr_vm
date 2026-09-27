#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

/* Unity 每例新建运行时；本文件验证编译写回的 union 穷尽性如何影响 CFG 的 default 可达性。 */
static SZrState *g_state;

/* 各用例共享同一初始化入口，失败断言由 Unity 跳到 tearDown。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 正常与断言提前退出都销毁本例运行时及其 GC 对象。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 编译语句需要独立编译状态；返回值由调用者配对 destroy_compiler_state。 */
static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

/* 先释放编译器内部数组和语义状态，再释放测试分配的外壳。 */
static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

/* 两个用例共用编译、建图、事实查询流程，只改变 union 分支是否穷尽。 */
static void assert_cfg_union_switch_default_reachability(const char *source,
                                                         TZrBool expectUnreachable) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "cfg_union_exhaustiveness.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrCompilerState *cs = create_compiler_state();
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *variableStatement;
    SZrAstNode *switchStatement;
    SZrAstNode *defaultNode;
    SZrAstNode *defaultStatement;
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(context);

    variableStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(variableStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SWITCH_EXPRESSION, switchStatement->type);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    ZrParser_Statement_Compile(cs, variableStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, switchStatement);
    TEST_ASSERT_FALSE(cs->hasError);

    defaultNode = switchStatement->data.switchExpression.defaultCase;
    TEST_ASSERT_NOT_NULL(defaultNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SWITCH_DEFAULT, defaultNode->type);
    TEST_ASSERT_NOT_NULL(defaultNode->data.switchDefault.block);
    TEST_ASSERT_NOT_NULL(defaultNode->data.switchDefault.block->data.block.body);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)defaultNode->data.switchDefault.block->data.block.body->count);
    defaultStatement = defaultNode->data.switchDefault.block->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(defaultStatement);

    /* 编译 switch 已设置 isUnionExhaustive；CFG 直接消费该 AST 标志。 */
    ZrParser_Cfg_Init(g_state, &cfg);
    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, ast));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, defaultStatement->location);
    if (expectUnreachable) {
        TEST_ASSERT_NOT_NULL(fact);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH, fact->cause);
        TEST_ASSERT_EQUAL_PTR(switchStatement, fact->causeNode);
    } else {
        TEST_ASSERT_NULL(fact);
    }

    /* BUG: cs 与独立 context 创建成功后若断言失败，Unity longjmp 会跳过
     * 下方两个 Free；tearDown 不回收这些原生分配，包含 malloc 的 cs 外壳。 */
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_SemanticContext_Free(context);
    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

/* 全部变体被覆盖时，冗余 default 必须归因为穷尽分支。 */
static void test_cfg_marks_exhaustive_union_switch_default_unreachable(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var choice: Choice = Choice.Empty;\n"
            "switch (choice) {\n"
            "    (Empty) { var emptyHit = 1; }\n"
            "    (Num(value)) { var numberHit = value; }\n"
            "    () { var redundantDefault = 0; }\n"
            "}\n";

    assert_cfg_union_switch_default_reachability(source, ZR_TRUE);
}

/* 缺少 Num 变体时，default 仍是可达的兜底分支。 */
static void test_cfg_keeps_non_exhaustive_union_switch_default_reachable(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var choice: Choice = Choice.Empty;\n"
            "switch (choice) {\n"
            "    (Empty) { var emptyHit = 1; }\n"
            "    () { var reachableDefault = 0; }\n"
            "}\n";

    assert_cfg_union_switch_default_reachability(source, ZR_FALSE);
}

/* CMake 的独立 Unity 目标经 language_pipeline core 执行这两个用例。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_marks_exhaustive_union_switch_default_unreachable);
    RUN_TEST(test_cfg_keeps_non_exhaustive_union_switch_default_reachable);
    return UNITY_END();
}
