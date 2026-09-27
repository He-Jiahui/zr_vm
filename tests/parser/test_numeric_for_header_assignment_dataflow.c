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

/* 每个 Unity 用例独占一个 VM state；语义环境与 AST 的 native 所有权仍由用例负责。 */
static SZrState *g_state;

/* 隔离字符串、语义上下文和 VM 资源，供当前用例的解析与推断共用。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* Unity 即使中止用例也会调用此处；这里只能收回 g_state。 */
/* BUG: 编译器初始化后的断言或用例后续断言一旦失败，Unity longjmp
 * 跳过函数末尾的 destroy_compiler_state/Ast_Free/InferredType_Free；此处不持有
 * malloc 的 cs，故失败路径遗留编译器状态及当时已取得的 AST/类型资源。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 在堆上建立可独立调用类型推断的编译器上下文；成功后必须配对销毁。 */
static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

/* 先释放推断器持有的语义/类型环境，再释放测试自行分配的外壳。 */
static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

/* 注册未定值布尔条件，使循环数据流保留零次与多次执行路径。 */
static void register_bool_variable(SZrCompilerState *cs, const char *name) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_BOOL);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &type));
    ZrParser_InferredType_Free(g_state, &type);
}

/* 给目标变量种下闭区间；环境复制临时类型，调用方随后释放临时值。 */
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

/* 从脚本借出指定语句，结构不符时交由用例断言报告失败。 */
static SZrAstNode *statement_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= index) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

/* 借出循环后表达式，确保推断与语义事实查询指向同一 AST 节点。 */
static SZrAstNode *expression_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

/* for 初始化先赋 1，未知条件允许跳过或执行将来赋 10 的 step；
 * 循环后的 +1 必须同时更新推断区间和该节点的语义数值事实。 */
static void test_for_init_and_step_assignment_joins_header_ranges(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (narrowed = 1; flag; narrowed = 10) {\n"
            "    flag;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_header_assignment_dataflow_test.zr",
            strlen("numeric_for_header_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
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

    /* 两个观察面应给出同一 [2,11]，且该区间不会触及 int64 溢出。 */
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

/* 独立可执行入口；CMake 还把它纳入 language_pipeline 的 CTest 清单。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_for_init_and_step_assignment_joins_header_ranges);
    return UNITY_END();
}
