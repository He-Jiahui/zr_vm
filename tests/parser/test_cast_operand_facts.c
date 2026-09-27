#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

#include <string.h>

/* Unity 每例独立的运行时；语义事实与 AST 均不得跨例复用。 */
static SZrState *g_state;
/* 编译状态由本文件静态持有，事实查询结果借用其中的 semanticContext。 */
static SZrCompilerState g_compiler;
/* 当前解析树供用例读取，实际释放责任在 tearDown。 */
static SZrAstNode *g_ast;

/* 先创建运行时再初始化编译状态，供每例独立推断。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    memset(&g_compiler, 0, sizeof(g_compiler));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_ast = ZR_NULL;
}

/* 断言中止后 Unity 仍执行此回调；先释放事实和 AST，再销毁运行时。 */
/* BUG: 前一例结束后若本例运行时创建失败，setUp 在重置静态状态前跳出；
 * 此处先用旧 state 清理编译器，若继续执行还会访问上一例已释放的 AST。 */
void tearDown(void) {
    ZrParser_CompilerState_Free(&g_compiler);
    ZrParser_Ast_Free(g_state, g_ast);
    ZrTests_Runtime_State_Destroy(g_state);
}

/* 只推断根 cast 的结果类型；返回节点借用 g_ast，内部操作数事实仍需单独查询。 */
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
    /* cast 推断先遍历 operand 并记录其原类型，再把目标类型返回给调用者。 */
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(&g_compiler, expression, &result);
    actualType = result.baseType;
    ZrParser_InferredType_Free(g_state, &result);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(expectedType, actualType);
    return expression;
}

/* 按 AST 身份检索借用的表达式事实，并要求 canonical type id 有效。 */
static void assert_expression_type(SZrAstNode *node, EZrValueType type) {
    const SZrSemanticExpressionFact *fact = ZrParser_SemanticFacts_FindExpressionByNode(
            g_compiler.semanticContext, node);
    TEST_ASSERT_NOT_NULL_MESSAGE(fact, "Cast operands must retain their own semantic facts");
    TEST_ASSERT_EQUAL_INT(type, fact->inferredType.baseType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, fact->typeId);
}

/* float 操作数的事实不能被外层 int 转换结果覆盖。 */
static void test_cast_keeps_operand_type_distinct_from_target_type(void) {
    SZrAstNode *expression = infer_cast("var result = <int> 3.5;", ZR_VALUE_TYPE_INT64);
    assert_expression_type(expression, ZR_VALUE_TYPE_INT64);
    assert_expression_type(expression->data.typeCastExpression.expression, ZR_VALUE_TYPE_DOUBLE);
}

/* 嵌套 cast 的每一层及最内字面量应各保留独立类型事实。 */
static void test_nested_casts_publish_each_operand_type(void) {
    SZrAstNode *outer = infer_cast("var result = <float> <int> 3.5;", ZR_VALUE_TYPE_DOUBLE);
    SZrAstNode *inner = outer->data.typeCastExpression.expression;
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_CAST_EXPRESSION, inner->type);
    assert_expression_type(outer, ZR_VALUE_TYPE_DOUBLE);
    assert_expression_type(inner, ZR_VALUE_TYPE_INT64);
    assert_expression_type(inner->data.typeCastExpression.expression, ZR_VALUE_TYPE_DOUBLE);
}

/* 已注册函数调用作为 cast 操作数时，原返回类型和解析出的调用身份都应保留。 */
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
    /* 注册会复制返回类型；本地空参数容器无需转移所有权。 */
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

/* 未知调用不应获得已解析目标；本例不要求 CallAt 必须返回一条未解析记录。 */
static void test_unknown_operand_does_not_invent_a_resolved_call_target(void) {
    SZrAstNode *expression = infer_cast("var result = <int> missing();", ZR_VALUE_TYPE_INT64);
    SZrParserSemanticCallQuery call = {0};
    (void)ZrParser_SemanticQuery_CallAt(g_compiler.semanticContext,
                                      expression->data.typeCastExpression.expression->location,
                                      ZR_NULL, &call);
    TEST_ASSERT_FALSE(call.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, call.targetSymbolId);
}

/* 独立 CTest cast_operand_facts 经此入口执行四个回归场景。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cast_keeps_operand_type_distinct_from_target_type);
    RUN_TEST(test_nested_casts_publish_each_operand_type);
    RUN_TEST(test_cast_publishes_call_identity_and_original_return_type);
    RUN_TEST(test_unknown_operand_does_not_invent_a_resolved_call_target);
    return UNITY_END();
}
