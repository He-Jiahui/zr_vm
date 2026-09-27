#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

/* 每个 Unity 用例独占 VM state；解析后的 AST 与编译器外壳另由用例持有。 */
static SZrState *g_state;

/* 为基数变化的两组用例分别建立解析和推断所需的 VM 环境。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* Unity 断言中止后仍进入此钩子，但它只持有 g_state。 */
/* BUG: 创建编译器后任一断言失败会经 Unity longjmp 越过用例末尾的
 * InferredType_Free/Ast_Free/destroy_compiler_state；此处无法释放 malloc 的 cs
 * 及当时已取得的原生资源，后一用例仍会继续运行。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 建立可独立推断循环和后继表达式的编译器上下文；成功后须配对销毁。 */
static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

/* 结束类型环境和语义上下文生命周期，再释放测试分配的编译器外壳。 */
static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

/* 将写入目标的初始闭区间放入类型环境，作为循环合并的进入路径。 */
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

/* 同一元素类型下只切换数组基数事实，隔离 foreach 零次路径的影响。 */
static void register_int64_array_variable(SZrCompilerState *cs,
                                           const char *name,
                                           TZrBool isNonempty) {
    SZrInferredType arrayType;
    SZrInferredType elementType;

    ZrParser_InferredType_Init(g_state, &arrayType, ZR_VALUE_TYPE_ARRAY);
    arrayType.protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE);
    ZrParser_InferredType_Init(g_state, &elementType, ZR_VALUE_TYPE_INT64);
    /* BUG: RawMalloc 失败时 Array_Init 仍留下空 head，紧接的 Push 会断言或写空指针；
     * 当前夹具未检测分配结果，OOM 无法作为 Unity 断言失败收尾。 */
    ZrCore_Array_Init(g_state, &arrayType.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &arrayType.elementTypes, &elementType);
    if (isNonempty) {
        arrayType.hasArraySizeConstraint = ZR_TRUE;
        arrayType.arrayFixedSize = 2;
        arrayType.arrayMinSize = 2;
        arrayType.arrayMaxSize = 2;
    }
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &arrayType));
    ZrParser_InferredType_Free(g_state, &elementType);
    ZrParser_InferredType_Free(g_state, &arrayType);
}

/* 借出脚本语句；缺少预期结构时交给用例断言报告。 */
static SZrAstNode *statement_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= index) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

/* 借出循环后表达式，使类型结果和节点数值事实观察同一个 AST。 */
static SZrAstNode *expression_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

/* 共用解析和推断链；调用方仅传入基数假设及对应的循环后区间。 */
static void assert_foreach_assignment_range(TZrBool isNonempty,
                                            TZrInt64 expectedMin,
                                            TZrInt64 expectedMax) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *foreachStatement;
    SZrAstNode *finalExpression;
    SZrInferredType foreachType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (var item in items) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_foreach_cardinality_dataflow_test.zr",
            strlen("numeric_foreach_cardinality_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    foreachStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_array_variable(cs, "items", isNonempty);
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &foreachType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(foreachStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOREACH_LOOP, foreachStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, foreachStatement, &foreachType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    /* 类型区间与语义事实须一致，且这个有界加法不应报告溢出。 */
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(expectedMin, result.minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(expectedMin, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &foreachType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

/* 基数未知时保留原值 5 和循环写入值 10，循环后的 +1 为 [6,11]。 */
static void test_unknown_cardinality_foreach_assignment_keeps_zero_iteration_path(void) {
    assert_foreach_assignment_range(ZR_FALSE, 6, 11);
}

/* 已知数组含两个元素时排除零次路径，循环后的 +1 收窄为 [11,11]。 */
static void test_nonempty_foreach_assignment_drops_zero_iteration_path(void) {
    assert_foreach_assignment_range(ZR_TRUE, 11, 11);
}

/* 两个 Unity 用例独立运行，并由 CMake 纳入 language_pipeline CTest 聚合入口。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unknown_cardinality_foreach_assignment_keeps_zero_iteration_path);
    RUN_TEST(test_nonempty_foreach_assignment_drops_zero_iteration_path);
    return UNITY_END();
}
