#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

/* 每个 Unity 用例的运行时状态由 setUp 创建、tearDown 销毁；本文件的推断 helper 借用它。 */
static SZrState *g_state;

/* 为本次用例创建独立运行时状态，供随后建立的编译器及 AST 使用。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 回收本次用例的运行时状态；测试/helper 的局部编译器外壳仍需在正常路径显式释放。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 返回的 malloc 外壳归调用方，正常路径交给 destroy_compiler_state。
 * TODO: Unity 默认断言跳转会绕过局部尾部清理；需在本目标注入初始化后断言失败，
 * 核对 CompilerState_Free/free 及 AST、推断类型的回收覆盖。 */
static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

/* 在运行时状态仍有效时释放编译器内部资源，再回收调用方拥有的 malloc 外壳。 */
static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

/* 为后继区间推断设置外层 narrowed 的初始事实；环境复制类型后即可释放临时推断类型。 */
static void register_int64_range_variable(SZrCompilerState *cs,
                                           const char *name,
                                           TZrInt64 minValue,
                                           TZrInt64 maxValue) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    type.hasRangeConstraint = ZR_TRUE;
    type.minValue = minValue;
    type.maxValue = maxValue;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &type));
    ZrParser_InferredType_Free(g_state, &type);
}

static SZrAstNode *statement_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= index) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

static SZrAstNode *expression_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

/* 常量比较恒假仍保留 init 的赋值；后继表达式必须为 2，不能混入不可达循环体的 10。 */
static void test_for_constant_false_comparison_applies_init_without_body_join(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (narrowed = 1; 1 == 2; ) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_constant_false_condition_assignment_dataflow_test.zr",
            strlen("numeric_for_constant_false_condition_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(2, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

/* 恒假 for 的头部 step 不应为循环后的 step+1 提供范围；这里只观察无范围和无 numeric fact。 */
static void test_for_false_condition_var_init_does_not_leak_header_binding(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (var step: int = 10; false; ) {\n"
            "    narrowed = step;\n"
            "}\n"
            "step + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_false_condition_var_init_no_leak_dataflow_test.zr",
            strlen("numeric_for_false_condition_var_init_no_leak_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_FALSE(result.hasRangeConstraint);
    TEST_ASSERT_NULL(numericFact);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

/* 以循环后的 narrowed+1 为观察点，核对推断区间与 numeric fact 区间相同且无溢出可能。 */
static void assert_for_body_assignment_before_break_range_equals(const char *source,
                                                                 const char *sourceNameChars,
                                                                 TZrInt64 expectedMin,
                                                                 TZrInt64 expectedMax) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;

    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)sourceNameChars,
            strlen(sourceNameChars));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(expectedMin, result.minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(expectedMin, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

/* 单一 break 路径把 narrowed 写为 10，统一要求后继 narrowed+1 精确为 11。 */
static void assert_for_body_assignment_before_break_range(const char *source,
                                                          const char *sourceNameChars) {
    assert_for_body_assignment_before_break_range_equals(source, sourceNameChars, 11, 11);
}

/* 恒真入口使首次 body 写 10 必达；break 出口应给出 11，不应并入初始 5 的后继 6。 */
static void test_for_true_condition_body_assignment_before_break_joins_at_least_once(void) {
    const char *source =
            "for (; true; ) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_body_assignment_before_break_dataflow_test.zr");
}

/* 省略条件也使首次 body 写 10 必达；break 出口应给出 11，不应保留零次迭代路径。 */
static void test_for_omitted_condition_body_assignment_before_break_joins_at_least_once(void) {
    const char *source =
            "for (;;) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_omitted_condition_body_assignment_before_break_dataflow_test.zr");
}

/* body 写 10 后立即 break；不可达 step 写 20 不得把后继 11 扩成含 21 的范围。 */
static void test_for_true_condition_step_assignment_body_assignment_before_break_skips_step(void) {
    const char *source =
            "for (; true; narrowed = 20) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_step_assignment_body_assignment_before_break_dataflow_test.zr");
}

/* 省略条件下 body 写 10 后 break，step 仍不可达；后继必须精确为 11。 */
static void test_for_omitted_condition_step_assignment_body_assignment_before_break_skips_step(
        void) {
    const char *source =
            "for (;; narrowed = 20) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_omitted_condition_step_assignment_body_assignment_before_break_dataflow_test.zr");
}

/* 必达 body 写 10 覆盖 init 写 1；break 后必须为 11，不能包含 init 的后继 2。 */
static void test_for_true_condition_assignment_init_body_assignment_before_break_joins_at_least_once(
        void) {
    const char *source =
            "for (narrowed = 1; true; ) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_assignment_init_body_assignment_before_break_dataflow_test.zr");
}

/* 省略条件使 body 写 10 覆盖 init 写 1；后继只接受 11，不包含零次迭代的 2。 */
static void test_for_omitted_condition_assignment_init_body_assignment_before_break_joins_at_least_once(
        void) {
    const char *source =
            "for (narrowed = 1;;) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_omitted_condition_assignment_init_body_assignment_before_break_dataflow_test.zr");
}

/* 循环体读取头部 var step 并写 narrowed 后 break；后继 11 验证头部值在 body 内可用。 */
static void test_for_true_condition_var_init_body_assignment_before_break_joins_at_least_once(
        void) {
    const char *source =
            "for (var step: int = 10; true; ) {\n"
            "    narrowed = step;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_var_init_body_assignment_before_break_dataflow_test.zr");
}

/* 省略条件下 body 必达且能读取头部 step=10；break 后 narrowed+1 必须为 11。 */
static void test_for_omitted_condition_var_init_body_assignment_before_break_joins_at_least_once(
        void) {
    const char *source =
            "for (var step: int = 10;;) {\n"
            "    narrowed = step;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_omitted_condition_var_init_body_assignment_before_break_dataflow_test.zr");
}

/* body 的 10 覆盖 init 的 1 并在 step 前 break；后继只接受 11，排除 2 与 21。 */
static void
test_for_true_condition_assignment_init_step_assignment_body_assignment_before_break_skips_step(
        void) {
    const char *source =
            "for (narrowed = 1; true; narrowed = 20) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_assignment_init_step_assignment_body_assignment_before_break_dataflow_test.zr");
}

/* 省略条件的必达 body 覆盖 init，并由 break 跳过 step；后继只接受 11。 */
static void
test_for_omitted_condition_assignment_init_step_assignment_body_assignment_before_break_skips_step(
        void) {
    const char *source =
            "for (narrowed = 1;; narrowed = 20) {\n"
            "    narrowed = 10;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_omitted_condition_assignment_init_step_assignment_body_assignment_before_break_dataflow_test.zr");
}

/* body 从头部 step 取得 10 后 break；不可达 step 写 narrowed=20 不得污染后继 11。 */
static void test_for_true_condition_var_init_step_assignment_body_assignment_before_break_skips_step(
        void) {
    const char *source =
            "for (var step: int = 10; true; narrowed = 20) {\n"
            "    narrowed = step;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_var_init_step_assignment_body_assignment_before_break_dataflow_test.zr");
}

/* 省略条件下 body 读取头部 step=10 并 break；后继 11 不能混入 step 写 20。 */
static void
test_for_omitted_condition_var_init_step_assignment_body_assignment_before_break_skips_step(
        void) {
    const char *source =
            "for (var step: int = 10;; narrowed = 20) {\n"
            "    narrowed = step;\n"
            "    break;\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_omitted_condition_var_init_step_assignment_body_assignment_before_break_dataflow_test.zr");
}

/* 两个分支都在 step 前 break；后继范围为 [11,13]，不包含 step 写 20 的后继 21。 */
static void
test_for_true_condition_step_assignment_nested_if_break_branches_skip_step(void) {
    const char *source =
            "for (; true; narrowed = 20) {\n"
            "    if (flag) {\n"
            "        narrowed = 10;\n"
            "        break;\n"
            "    } else {\n"
            "        narrowed = 12;\n"
            "        break;\n"
            "    }\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range_equals(
            source,
            "numeric_for_true_condition_step_assignment_nested_if_break_branches_dataflow_test.zr",
            11,
            13);
}

/* 常量 true 的嵌套分支必达且 break；不存在落到 step 写 20 的路径，后继必须为 11。 */
static void
test_for_true_condition_step_assignment_known_true_if_break_branch_skip_step(void) {
    const char *source =
            "for (; true; narrowed = 20) {\n"
            "    if (true) {\n"
            "        narrowed = 10;\n"
            "        break;\n"
            "    }\n"
            "}\n"
            "narrowed + 1;\n";

    assert_for_body_assignment_before_break_range(
            source,
            "numeric_for_true_condition_step_assignment_known_true_if_break_branch_dataflow_test.zr");
}

/* Unity 依次运行 16 个 for 常量条件与 break 数据流场景，并把汇总结果返回给套件 runner。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_for_constant_false_comparison_applies_init_without_body_join);
    RUN_TEST(test_for_false_condition_var_init_does_not_leak_header_binding);
    RUN_TEST(test_for_true_condition_body_assignment_before_break_joins_at_least_once);
    RUN_TEST(test_for_omitted_condition_body_assignment_before_break_joins_at_least_once);
    RUN_TEST(test_for_true_condition_step_assignment_body_assignment_before_break_skips_step);
    RUN_TEST(test_for_omitted_condition_step_assignment_body_assignment_before_break_skips_step);
    RUN_TEST(test_for_true_condition_assignment_init_body_assignment_before_break_joins_at_least_once);
    RUN_TEST(test_for_omitted_condition_assignment_init_body_assignment_before_break_joins_at_least_once);
    RUN_TEST(test_for_true_condition_var_init_body_assignment_before_break_joins_at_least_once);
    RUN_TEST(test_for_omitted_condition_var_init_body_assignment_before_break_joins_at_least_once);
    RUN_TEST(
            test_for_true_condition_assignment_init_step_assignment_body_assignment_before_break_skips_step);
    RUN_TEST(
            test_for_omitted_condition_assignment_init_step_assignment_body_assignment_before_break_skips_step);
    RUN_TEST(test_for_true_condition_var_init_step_assignment_body_assignment_before_break_skips_step);
    RUN_TEST(
            test_for_omitted_condition_var_init_step_assignment_body_assignment_before_break_skips_step);
    RUN_TEST(test_for_true_condition_step_assignment_nested_if_break_branches_skip_step);
    RUN_TEST(test_for_true_condition_step_assignment_known_true_if_break_branch_skip_step);
    return UNITY_END();
}
