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

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

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

static void assert_for_body_assignment_before_break_range(const char *source,
                                                          const char *sourceNameChars) {
    assert_for_body_assignment_before_break_range_equals(source, sourceNameChars, 11, 11);
}

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
