#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

#include <string.h>

static SZrState *g_state;
static SZrCompilerState g_compiler;
static SZrAstNode *g_ast;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    memset(&g_compiler, 0, sizeof(g_compiler));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_ast = ZR_NULL;
}

void tearDown(void) {
    ZrParser_CompilerState_Free(&g_compiler);
    ZrParser_Ast_Free(g_state, g_ast);
    ZrTests_Runtime_State_Destroy(g_state);
}

static SZrAstNode *infer_cast(const char *source, EZrValueType expectedType) {
    SZrString *uri = ZrCore_String_CreateFromNative(g_state, "cast_operand.zr");
    SZrAstNode *expression;
    SZrInferredType result;
    TZrBool success;
    EZrValueType actualType;

    g_ast = ZrParser_Parse(g_state, source, strlen(source), uri);
    TEST_ASSERT_NOT_NULL(g_ast);
    expression = g_ast->data.script.statements->nodes[0]->data.variableDeclaration.value;
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_CAST_EXPRESSION, expression->type);
    g_compiler.scriptAst = g_ast;
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(&g_compiler, expression, &result);
    actualType = result.baseType;
    ZrParser_InferredType_Free(g_state, &result);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(expectedType, actualType);
    return expression;
}

static void assert_expression_type(SZrAstNode *node, EZrValueType type) {
    const SZrSemanticExpressionFact *fact = ZrParser_SemanticFacts_FindExpressionByNode(
            g_compiler.semanticContext, node);
    TEST_ASSERT_NOT_NULL_MESSAGE(fact, "Cast operands must retain their own semantic facts");
    TEST_ASSERT_EQUAL_INT(type, fact->inferredType.baseType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, fact->typeId);
}

static void test_cast_keeps_operand_type_distinct_from_target_type(void) {
    SZrAstNode *expression = infer_cast("var result = <int> 3.5;", ZR_VALUE_TYPE_INT64);
    assert_expression_type(expression, ZR_VALUE_TYPE_INT64);
    assert_expression_type(expression->data.typeCastExpression.expression, ZR_VALUE_TYPE_DOUBLE);
}

static void test_nested_casts_publish_each_operand_type(void) {
    SZrAstNode *outer = infer_cast("var result = <float> <int> 3.5;", ZR_VALUE_TYPE_DOUBLE);
    SZrAstNode *inner = outer->data.typeCastExpression.expression;
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_CAST_EXPRESSION, inner->type);
    assert_expression_type(outer, ZR_VALUE_TYPE_DOUBLE);
    assert_expression_type(inner, ZR_VALUE_TYPE_INT64);
    assert_expression_type(inner->data.typeCastExpression.expression, ZR_VALUE_TYPE_DOUBLE);
}

static void test_cast_publishes_call_identity_and_original_return_type(void) {
    SZrInferredType returnType;
    SZrArray parameters;
    SZrAstNode *expression;
    SZrAstNode *operand;
    SZrParserSemanticCallQuery call;
    TZrBool registered;
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "measure");

    ZrCore_Array_Construct(&parameters);
    ZrParser_InferredType_Init(g_state, &returnType, ZR_VALUE_TYPE_DOUBLE);
    registered = ZrParser_TypeEnvironment_RegisterFunction(
            g_state, g_compiler.typeEnv, name, &returnType, &parameters);
    ZrParser_InferredType_Free(g_state, &returnType);
    TEST_ASSERT_TRUE(registered);
    expression = infer_cast("var result = <int> measure();", ZR_VALUE_TYPE_INT64);
    operand = expression->data.typeCastExpression.expression;
    assert_expression_type(operand, ZR_VALUE_TYPE_DOUBLE);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            g_compiler.semanticContext, operand->location, ZR_NULL, &call));
    TEST_ASSERT_NOT_NULL(call.reference);
    TEST_ASSERT_TRUE(call.reference->isResolved);
    TEST_ASSERT_TRUE(call.hasResolvedTarget);
    TEST_ASSERT_EQUAL_STRING("measure", ZrCore_String_GetNativeString(call.reference->name));
}

static void test_unknown_operand_does_not_invent_a_resolved_call_target(void) {
    SZrAstNode *expression = infer_cast("var result = <int> missing();", ZR_VALUE_TYPE_INT64);
    SZrParserSemanticCallQuery call = {0};
    (void)ZrParser_SemanticQuery_CallAt(g_compiler.semanticContext,
                                      expression->data.typeCastExpression.expression->location,
                                      ZR_NULL, &call);
    TEST_ASSERT_FALSE(call.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, call.targetSymbolId);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cast_keeps_operand_type_distinct_from_target_type);
    RUN_TEST(test_nested_casts_publish_each_operand_type);
    RUN_TEST(test_cast_publishes_call_identity_and_original_return_type);
    RUN_TEST(test_unknown_operand_does_not_invent_a_resolved_call_target);
    return UNITY_END();
}
