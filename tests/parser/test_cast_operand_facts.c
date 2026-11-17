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

/* Count synchronous parser error events, never a recovered AST or hasError. */
typedef struct SZrProbeDiagnostics {
    TZrUInt32 legacyErrors, structuredErrors, structuredNonErrors, invalidStructured;
} SZrProbeDiagnostics;

static void capture_probe_legacy_error(TZrPtr userData, const SZrFileRange *location,
                                       const TZrChar *message, EZrToken token) {
    SZrProbeDiagnostics *capture = userData;
    (void)location;
    (void)message;
    (void)token;
    ++capture->legacyErrors;
}

static void capture_probe_structured_error(TZrPtr userData, const SZrStructuredDiagnostic *diagnostic,
                                           EZrToken token) {
    SZrProbeDiagnostics *capture = userData;
    (void)token;
    if (diagnostic == ZR_NULL) {
        ++capture->invalidStructured;
        return;
    }
    switch (diagnostic->severity) {
        case ZR_STRUCTURED_DIAGNOSTIC_ERROR:
            ++capture->structuredErrors;
            break;
        case ZR_STRUCTURED_DIAGNOSTIC_WARNING:
        case ZR_STRUCTURED_DIAGNOSTIC_INFO:
        case ZR_STRUCTURED_DIAGNOSTIC_HINT:
            ++capture->structuredNonErrors;
            break;
        default:
            ++capture->invalidStructured;
            break;
    }
}

static SZrProbeDiagnostics parse_probe_source(const char *source) {
    SZrProbeDiagnostics capture = {0};
    SZrParserState parser = {0};
    SZrString *uri = ZrCore_String_CreateFromNative(g_state, "cast_probe_recovery.zr");
    TEST_ASSERT_NOT_NULL(uri);
    ZrParser_State_Init(&parser, g_state, source, strlen(source), uri);
    parser.errorCallback = capture_probe_legacy_error;
    parser.structuredErrorCallback = capture_probe_structured_error;
    parser.errorUserData = &capture;
    parser.suppressErrorOutput = ZR_TRUE;
    /* Parse only. No module resolution, compilation, or evaluation of the input. */
    g_ast = ZrParser_ParseWithState(&parser);
    ZrParser_State_Free(&parser);
    return capture; /* g_ast remains owned by tearDown, including on assertion failure. */
}

static const SZrAstNode *find_probe_binding(const char *name) {
    if (g_ast == ZR_NULL || g_ast->type != ZR_AST_SCRIPT) return ZR_NULL;
    const SZrAstNodeArray *statements = g_ast->data.script.statements;
    if (statements == ZR_NULL || statements->nodes == ZR_NULL) return ZR_NULL;
    for (TZrSize i = 0; i < statements->count; ++i) {
        const SZrAstNode *statement = statements->nodes[i];
        if (statement == ZR_NULL || statement->type != ZR_AST_VARIABLE_DECLARATION) continue;
        const SZrAstNode *binding = statement->data.variableDeclaration.pattern;
        if (binding == ZR_NULL || binding->type != ZR_AST_IDENTIFIER_LITERAL ||
            binding->data.identifier.name == ZR_NULL) continue;
        const char *spelling = ZrCore_String_GetNativeString(binding->data.identifier.name);
        if (spelling != ZR_NULL && strcmp(spelling, name) == 0) return statement;
    }
    return ZR_NULL;
}

static void assert_probe_reports_error(const char *source, TZrBool expectGood) {
    const SZrProbeDiagnostics capture = parse_probe_source(source);
    TEST_ASSERT_EQUAL_UINT32(0, capture.invalidStructured);
    TEST_ASSERT_TRUE_MESSAGE(capture.legacyErrors != 0 || capture.structuredErrors != 0, source);
    /* Fatal syntax may return no AST. Ordinary recovery must retain the next binding. */
    if (expectGood && g_ast != ZR_NULL) {
        TEST_ASSERT_NOT_NULL_MESSAGE(find_probe_binding("good"), "Recovered script lost the trailing good binding");
    }
}

static void test_standalone_cast_operand_error_is_reported(void) {
    assert_probe_reports_error("<int> import(\"ghost\")?.[0]; let good = 1;", ZR_TRUE);
}

static void test_type_literal_probe_error_is_reported(void) {
    assert_probe_reports_error("int[zr.import(\"ghost\")]; let good = 1;", ZR_TRUE);
}

static void test_explicit_generic_call_probe_error_is_reported(void) {
    assert_probe_reports_error("factory<int[zr.import(\"ghost\")]>(); let good = 1;", ZR_TRUE);
}

static void test_plain_import_recovery_reports_error(void) {
    assert_probe_reports_error("let broken = import(\"ghost\") + ; let good = 1;", ZR_TRUE);
}

static void test_generic_member_expression_reports_invalid_type_argument(void) {
    /* Whole-expression control, not a claim of a distinct generic-member commit bug. */
    assert_probe_reports_error("Foo<int[zr.import(\"ghost\")]>.member;", ZR_FALSE);
}

static void test_valid_cast_has_no_parser_diagnostic(void) {
    const SZrProbeDiagnostics capture = parse_probe_source("var result = <int> 3.5;");
    const SZrAstNode *declaration = find_probe_binding("result");
    TEST_ASSERT_EQUAL_UINT32(0, capture.invalidStructured);
    TEST_ASSERT_EQUAL_UINT32(0, capture.legacyErrors);
    TEST_ASSERT_EQUAL_UINT32(0, capture.structuredErrors);
    TEST_ASSERT_EQUAL_UINT32(0, capture.structuredNonErrors);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_CAST_EXPRESSION, declaration->data.variableDeclaration.value->type);
}

/* 独立 CTest cast_operand_facts 保留原四例，并执行全部新增语法回归。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cast_keeps_operand_type_distinct_from_target_type);
    RUN_TEST(test_nested_casts_publish_each_operand_type);
    RUN_TEST(test_cast_publishes_call_identity_and_original_return_type);
    RUN_TEST(test_unknown_operand_does_not_invent_a_resolved_call_target);
    RUN_TEST(test_standalone_cast_operand_error_is_reported);
    RUN_TEST(test_type_literal_probe_error_is_reported);
    RUN_TEST(test_explicit_generic_call_probe_error_is_reported);
    RUN_TEST(test_plain_import_recovery_reports_error);
    RUN_TEST(test_generic_member_expression_reports_invalid_type_argument);
    RUN_TEST(test_valid_cast_has_no_parser_diagnostic);
    return UNITY_END();
}
