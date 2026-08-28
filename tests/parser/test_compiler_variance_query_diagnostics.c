#include <string.h>

#include "unity.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/variance.h"

#include "harness/runtime_support.h"

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

static TZrSize count_variance_diagnostics(
        const SZrParserSemanticQueryDiagnostics *diagnostics) {
    TZrSize count = 0U;

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_publishes_all_interface_variance_diagnostics);
    return UNITY_END();
}
