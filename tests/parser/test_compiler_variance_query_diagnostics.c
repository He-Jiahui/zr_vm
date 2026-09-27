#include <string.h>

#include "unity.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/variance.h"

#include "harness/runtime_support.h"

/* Unity 每个用例各建运行时；解析树与编译器状态由用例另行释放。 */
static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    /* BUG: Unity 断言失败会跳过用例末尾的 Ast_Free/CompilerState_Free；
     * 这里只销毁运行时，局部原生资源未按所有权契约清理。 */
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/** @brief 核对模块查询中的 variance 诊断元数据，并统计发布数量。
 * @note diagnostics 借用编译器语义上下文，须在 CompilerState_Free 前读取。
 */
static TZrSize count_variance_diagnostics(
        const SZrParserSemanticQueryDiagnostics *diagnostics) {
    TZrSize count = 0U;

    /* TODO: 当前只核对数量及共同字段，无法证明三处具体违规都被覆盖。
     * 需逐项核对源码范围与上下文；事实层仅按范围、代码及消息去重。 */
    for (TZrSize index = 0U;
         diagnostics != ZR_NULL && index < diagnostics->count;
         index++) {
        const SZrStructuredDiagnostic *diagnostic = &diagnostics->items[index];
        const TZrChar *code = diagnostic->code != ZR_NULL
                                      ? ZrCore_String_GetNativeString(diagnostic->code)
                                      : ZR_NULL;
        if (code != ZR_NULL && strcmp(code, "invalid_variance") == 0) {
            count++;
            TEST_ASSERT_EQUAL_UINT32(2013U, diagnostic->descriptorId);
            TEST_ASSERT_EQUAL_INT(
                    ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
                    diagnostic->noFixReason);
            TEST_ASSERT_TRUE(diagnostic->relatedInformation.isValid);
            TEST_ASSERT_EQUAL_UINT32(
                    1U,
                    (TZrUInt32)diagnostic->relatedInformation.length);
            TEST_ASSERT_FALSE(diagnostic->fixes.isValid);
        }
    }
    return count;
}

/** @brief 直接验证 LSP 所用发布接口会将接口中的多处 variance 违规写入模块查询。
 * @note 常规接口编译另走首项校验路径；本用例不代表该路径会发布全部违规。
 */
static void test_parser_publishes_all_interface_variance_diagnostics(void) {
    static TZrChar source[] =
            "interface Mixed<out T> {\n"
            "    fn accept(value: T): void;\n"
            "    pub var value: T;\n"
            "    pub property item: T { set; }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_variance_query_diagnostics_test.zr",
            strlen("compiler_variance_query_diagnostics_test.zr"));
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *interfaceNode;
    SZrCompilerState compiler;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, ast->data.script.statements->count);
    interfaceNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(interfaceNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, interfaceNode->type);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = ast;

    /* LSP 直接调用该发布接口；常规编译只通过另一入口报告首个违规。 */
    TEST_ASSERT_TRUE(ZrParser_Variance_PublishInterfaceDiagnostics(
            &compiler, interfaceNode));
    TEST_ASSERT_FALSE(compiler.hasError);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(3U, count_variance_diagnostics(&diagnostics));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

/* TODO: 此目标已在 tests/CMakeLists.txt 建立，但尚无 add_test 注册，
 * 当前不会作为 CTest 用例运行；需确认 CI 是否直接运行该目标，或补注册。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_publishes_all_interface_variance_diagnostics);
    return UNITY_END();
}
