/* 回归：pub/pro/pri async fn 的访问修饰符须保留到函数 AST，并进入模块导出预扫描。
 *
 * async fn 由 parse_reserved_async_function_declaration 接管；它先消费修饰符和 async，
 * 再把 fn 交给 parse_function_declaration。后者看不到修饰符而取缺省 private，
 * 因此这里按顶层、块内两个分派入口及导出预扫描这一消费者分别断言。 */

#include <string.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_common/zr_array_conf.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/parser.h"
#include "compiler/module_init_analysis.h"

/* 期望的函数声明形态；name 同时用作断言失败时的定位信息。 */
typedef struct {
    const char *name;
    EZrAccessModifier accessModifier;
    TZrBool isAsync;
} SZrExpectedFunction;

/* 四种 async 修饰符形态，外加一个同步 pub fn 作为不经过 async 入口的对照。 */
static const char kTopLevelSource[] =
        "pub async fn exported(): zr.task.Task<int> { return 1; }\n"
        "pro async fn shared(): zr.task.Task<int> { return 2; }\n"
        "pri async fn hidden(): zr.task.Task<int> { return 3; }\n"
        "async fn localOnly(): zr.task.Task<int> { return 4; }\n"
        "pub fn syncExported(): int { return 5; }\n";

static const SZrExpectedFunction kTopLevelFunctions[] = {
        {"exported", ZR_ACCESS_PUBLIC, ZR_TRUE},
        {"shared", ZR_ACCESS_PROTECTED, ZR_TRUE},
        {"hidden", ZR_ACCESS_PRIVATE, ZR_TRUE},
        {"localOnly", ZR_ACCESS_PRIVATE, ZR_TRUE},
        {"syncExported", ZR_ACCESS_PUBLIC, ZR_FALSE},
};

/* 函数体内的声明走 parse_statement，而不是 parse_top_level_statement。 */
static const char kNestedSource[] =
        "fn outer(): int {\n"
        "    pub async fn nestedExported(): zr.task.Task<int> { return 1; }\n"
        "    pro async fn nestedShared(): zr.task.Task<int> { return 2; }\n"
        "    async fn nestedLocal(): zr.task.Task<int> { return 3; }\n"
        "    return 0;\n"
        "}\n";

static const SZrExpectedFunction kNestedFunctions[] = {
        {"nestedExported", ZR_ACCESS_PUBLIC, ZR_TRUE},
        {"nestedShared", ZR_ACCESS_PROTECTED, ZR_TRUE},
        {"nestedLocal", ZR_ACCESS_PRIVATE, ZR_TRUE},
};

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_State_Destroy(g_state);
    g_state = ZR_NULL;
}

/* 解析整份脚本并要求没有任何诊断；返回的 AST 由调用方用 ZrParser_Ast_Free 释放。 */
static SZrAstNode *parse_script_without_diagnostics(const char *source, const char *sourceNameText) {
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *ast;
    TZrBool hasError;

    sourceName = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)sourceNameText);
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parserState);
    hasError = parserState.hasError;
    ZrParser_State_Free(&parserState);

    if (hasError && ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
        ast = ZR_NULL;
    }
    TEST_ASSERT_FALSE_MESSAGE(hasError, sourceNameText);
    TEST_ASSERT_NOT_NULL_MESSAGE(ast, sourceNameText);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    return ast;
}

/* 核对一个函数声明节点的名称、异步标记和访问级别。 */
static void assert_function_declaration(const SZrAstNode *node, const SZrExpectedFunction *expected) {
    const SZrFunctionDeclaration *declaration;

    TEST_ASSERT_NOT_NULL_MESSAGE(node, expected->name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_FUNCTION_DECLARATION, node->type, expected->name);

    declaration = &node->data.functionDeclaration;
    TEST_ASSERT_NOT_NULL_MESSAGE(declaration->name, expected->name);
    TEST_ASSERT_NOT_NULL_MESSAGE(declaration->name->name, expected->name);
    TEST_ASSERT_EQUAL_STRING(expected->name, ZrCore_String_GetNativeString(declaration->name->name));
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected->isAsync, declaration->isAsync, expected->name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected->accessModifier, declaration->accessModifier, expected->name);
}

/* 在预扫描摘要里按名称查找导出项；未导出时返回空。 */
static const SZrModuleInitExportInfo *find_summary_export(const SZrParserModuleInitSummary *summary,
                                                          const char *name) {
    TZrSize index;

    for (index = 0; index < summary->exports.length; ++index) {
        const SZrModuleInitExportInfo *info =
                (const SZrModuleInitExportInfo *)ZrCore_Array_Get((SZrArray *)&summary->exports, index);

        if (info != ZR_NULL && info->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(info->name), name) == 0) {
            return info;
        }
    }

    return ZR_NULL;
}

/* 顶层入口：显式修饰符逐一保留，未写修饰符时仍是 private，且都带异步标记。 */
static void test_top_level_async_function_keeps_access_modifier(void) {
    SZrAstNode *ast = parse_script_without_diagnostics(kTopLevelSource, "async_access_top_level.zr");
    TZrSize index;

    TEST_ASSERT_EQUAL_size_t(ZR_ARRAY_COUNT(kTopLevelFunctions), ast->data.script.statements->count);
    for (index = 0; index < ZR_ARRAY_COUNT(kTopLevelFunctions); ++index) {
        assert_function_declaration(ast->data.script.statements->nodes[index], &kTopLevelFunctions[index]);
    }

    ZrParser_Ast_Free(g_state, ast);
}

/* 块内入口：函数体中的 async fn 与顶层保持同样的修饰符结果。 */
static void test_block_level_async_function_keeps_access_modifier(void) {
    SZrAstNode *ast = parse_script_without_diagnostics(kNestedSource, "async_access_block_level.zr");
    const SZrAstNode *outer;
    const SZrAstNodeArray *body;
    TZrSize index;

    TEST_ASSERT_EQUAL_size_t(1U, ast->data.script.statements->count);
    outer = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(outer);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, outer->type);
    TEST_ASSERT_FALSE(outer->data.functionDeclaration.isAsync);
    TEST_ASSERT_NOT_NULL(outer->data.functionDeclaration.body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, outer->data.functionDeclaration.body->type);

    body = outer->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_size_t(ZR_ARRAY_COUNT(kNestedFunctions) + 1U, body->count);
    for (index = 0; index < ZR_ARRAY_COUNT(kNestedFunctions); ++index) {
        assert_function_declaration(body->nodes[index], &kNestedFunctions[index]);
    }

    ZrParser_Ast_Free(g_state, ast);
}

/* 消费者：导出预扫描只登记 pub/pro 函数，pub/pro async fn 须与同步 pub fn 一样出现。 */
static void test_module_export_prescan_lists_public_and_protected_async_functions(void) {
    SZrAstNode *ast = parse_script_without_diagnostics(kTopLevelSource, "async_access_exports.zr");
    SZrString *moduleName = ZrCore_String_CreateFromNative(g_state, "async.access.exports");
    const SZrParserModuleInitSummary *summary;
    const SZrModuleInitExportInfo *exported;
    const SZrModuleInitExportInfo *shared;
    const SZrModuleInitExportInfo *syncExported;

    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(g_state, moduleName, ast));
    summary = ZrParser_ModuleInitAnalysis_FindSummaryByAst(g_state->global, ast);
    TEST_ASSERT_NOT_NULL(summary);

    syncExported = find_summary_export(summary, "syncExported");
    TEST_ASSERT_NOT_NULL(syncExported);
    TEST_ASSERT_EQUAL_UINT8(ZR_MODULE_EXPORT_KIND_FUNCTION, syncExported->exportKind);
    TEST_ASSERT_EQUAL_UINT8(ZR_ACCESS_PUBLIC, syncExported->accessModifier);

    exported = find_summary_export(summary, "exported");
    TEST_ASSERT_NOT_NULL(exported);
    TEST_ASSERT_EQUAL_UINT8(ZR_MODULE_EXPORT_KIND_FUNCTION, exported->exportKind);
    TEST_ASSERT_EQUAL_UINT8(ZR_ACCESS_PUBLIC, exported->accessModifier);

    shared = find_summary_export(summary, "shared");
    TEST_ASSERT_NOT_NULL(shared);
    TEST_ASSERT_EQUAL_UINT8(ZR_MODULE_EXPORT_KIND_FUNCTION, shared->exportKind);
    TEST_ASSERT_EQUAL_UINT8(ZR_ACCESS_PROTECTED, shared->accessModifier);

    TEST_ASSERT_NULL(find_summary_export(summary, "hidden"));
    TEST_ASSERT_NULL(find_summary_export(summary, "localOnly"));

    ZrParser_ModuleInitAnalysis_ClearAstIdentity(g_state->global, ast);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_top_level_async_function_keeps_access_modifier);
    RUN_TEST(test_block_level_async_function_keeps_access_modifier);
    RUN_TEST(test_module_export_prescan_lists_public_and_protected_async_functions);
    return UNITY_END();
}
