#include "unity.h"
#include "runtime_support.h"
#include "path_support.h"
#include "interface/lsp_interface_internal.h"
#include "zr_vm_language_server/lsp_uri.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/writer.h"

#include <string.h>

/* Unity 每例创建的 VM；LSP 上下文和返回位置均须在它销毁前释放。 */
static SZrState *g_state;
/* 与当前 g_state 配对的文档和项目索引上下文，由 tearDown 释放。 */
static SZrLspContext *g_context;
/* 普通调用定义查询返回的位置指针及数组缓冲区，均由本测试持有。 */
static SZrArray g_plainLocations;
/* cast 操作数定义查询单独收集，避免与普通调用结果混用。 */
static SZrArray g_castLocations;
/* 项目清单路径兼作本例是否创建磁盘夹具的清理哨兵。 */
static char g_projectPath[ZR_TESTS_PATH_MAX];
/* 编译得到的 provider 二进制路径，供项目导入查询。 */
static char g_binaryPath[ZR_TESTS_PATH_MAX];
/* 可选中间产物路径；二进制用例先尝试删除旧文件再决定是否重建。 */
static char g_intermediatePath[ZR_TESTS_PATH_MAX];
/* 项目入口源码路径；LSP 按其 file URI 打开本例文档。 */
static char g_mainPath[ZR_TESTS_PATH_MAX];

/* GetDefinition 的位置对象归调用方，先逐项释放，再归还数组缓冲区。 */
static void free_locations(SZrArray *locations) {
    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **location = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (location != ZR_NULL && *location != ZR_NULL) {
            ZrCore_Memory_RawFree(g_state->global, *location, sizeof(**location));
        }
    }
    if (locations->isValid) {
        ZrCore_Array_Free(g_state, locations);
    }
}

/* Unity 每例先建立 VM 和 LSP 上下文，再重置查询结果及磁盘夹具路径。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_context = ZrLanguageServer_LspContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
    ZrCore_Array_Construct(&g_plainLocations);
    ZrCore_Array_Construct(&g_castLocations);
    g_projectPath[0] = g_binaryPath[0] = g_intermediatePath[0] = g_mainPath[0] = '\0';
}

/* Unity 即使断言中止也执行此回调；按位置、上下文、VM 的依赖顺序释放。 */
void tearDown(void) {
    free_locations(&g_plainLocations);
    free_locations(&g_castLocations);
    ZrLanguageServer_LspContext_Free(g_state, g_context);
    ZrTests_Runtime_State_Destroy(g_state);
    /* 项目路径是磁盘夹具哨兵；清理仅尝试删除本例记录的四类产物。 */
    if (g_projectPath[0] != '\0') {
        remove(g_projectPath);
        remove(g_binaryPath);
        remove(g_intermediatePath);
        remove(g_mainPath);
    }
}

/* 用真实磁盘文本驱动项目发现；写入和关闭失败都使当前 Unity 用例失败。 */
static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    size_t written;
    int closed;
    TEST_ASSERT_NOT_NULL(file);
    written = fwrite(text, 1, strlen(text), file);
    closed = fclose(file);
    TEST_ASSERT_EQUAL_UINT64(strlen(text), written);
    TEST_ASSERT_EQUAL_INT(0, closed);
}

/* 按从零开始的出现次数定位调用名；本组源码是 ASCII，字节列数等于 LSP 的 UTF-16 列数。 */
static SZrLspPosition find_position(const char *source, const char *needle, TZrSize occurrence) {
    const char *match = source;
    SZrLspPosition position = {0};
    for (TZrSize index = 0; index <= occurrence; index++) {
        match = strstr(match, needle);
        TEST_ASSERT_NOT_NULL(match);
        if (index != occurrence) {
            match += strlen(needle);
        }
    }
    for (const char *cursor = source; cursor < match; cursor++) {
        if (*cursor == '\n') {
            position.line++;
            position.character = 0;
        } else {
            position.character++;
        }
    }
    return position;
}

/* 比较普通调用与 cast 操作数的 parser 外部身份，并要求 LSP 定义投影落在同一位置。 */
static void assert_cast_preserves_external_call(SZrString *uri, const char *source, const char *member) {
    SZrLspPosition plain = find_position(source, member, 0);
    SZrLspPosition cast = find_position(source, member, 1);
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition plainFilePosition;
    SZrFilePosition castFilePosition;
    SZrParserSemanticSymbolQuery plainSymbol;
    SZrParserSemanticSymbolQuery castSymbol;
    SZrLspLocation *plainLocation;
    SZrLspLocation *castLocation;

    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_state, g_context, uri, source, strlen(source), 1));
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_state, g_context, uri);
    TEST_ASSERT_NOT_NULL(analyzer);
    plainFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(g_context, uri, plain);
    castFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(g_context, uri, cast);
    /* 同一成员在 cast 内外须保留 provider、签名和所有者的规范语义身份。 */
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_SymbolAt(analyzer->semanticContext,
            ZrParser_FileRange_Create(plainFilePosition, plainFilePosition, uri), ZR_NULL, &plainSymbol));
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticQuery_SymbolAt(analyzer->semanticContext,
            ZrParser_FileRange_Create(castFilePosition, castFilePosition, uri), ZR_NULL, &castSymbol),
            "Cast operand must have the same canonical external target as the uncast call");
    TEST_ASSERT_TRUE(plainSymbol.hasExternalTarget);
    TEST_ASSERT_TRUE(castSymbol.hasExternalTarget);
    TEST_ASSERT_EQUAL_UINT32(plainSymbol.symbolId, castSymbol.symbolId);
    TEST_ASSERT_EQUAL_UINT32(plainSymbol.typeId, castSymbol.typeId);
    TEST_ASSERT_EQUAL_INT(plainSymbol.externalTargetKind, castSymbol.externalTargetKind);
    TEST_ASSERT_EQUAL_UINT64(plainSymbol.externalProviderGeneration, castSymbol.externalProviderGeneration);
    TEST_ASSERT_EQUAL_UINT32(plainSymbol.externalMetadataToken, castSymbol.externalMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(plainSymbol.externalSignatureToken, castSymbol.externalSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(plainSymbol.externalSignatureHash, castSymbol.externalSignatureHash);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(plainSymbol.externalOwnerIdentity, castSymbol.externalOwnerIdentity));
    /* 将 parser 身份约束落实到编辑器可见的定义跳转，结果由 tearDown 归还。 */
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(
            g_state, g_context, uri, plain, &g_plainLocations));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(
            g_state, g_context, uri, cast, &g_castLocations));
    TEST_ASSERT_EQUAL_UINT(1, g_plainLocations.length);
    TEST_ASSERT_EQUAL_UINT(1, g_castLocations.length);
    plainLocation = *(SZrLspLocation **)ZrCore_Array_Get(&g_plainLocations, 0);
    castLocation = *(SZrLspLocation **)ZrCore_Array_Get(&g_castLocations, 0);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(plainLocation->uri, castLocation->uri));
    TEST_ASSERT_EQUAL_INT(plainLocation->range.start.line, castLocation->range.start.line);
    TEST_ASSERT_EQUAL_INT(plainLocation->range.start.character, castLocation->range.start.character);
    TEST_ASSERT_EQUAL_INT(plainLocation->range.end.line, castLocation->range.end.line);
    TEST_ASSERT_EQUAL_INT(plainLocation->range.end.character, castLocation->range.end.character);
}

/* 从真实项目清单及编译产物导入 provider，比较预期无、同步及过时 .zri 布局。 */
static void assert_binary_cast_operand(const char *intermediateSource) {
    const char *source = "var binary = import(\"cast_provider\");\n"
                         "var plain = binary.measure();\n"
                         "var cast = <int> binary.measure();\n";
    const char *provider = "pub var measure = fn(): float => 3.5;\n";
    SZrBinaryWriterOptions options = {0};
    SZrFunction *function;
    SZrString *sourceName;
    TZrBool written;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("lsp_cast_operand", "project", "cast", ".zrp",
            g_projectPath, sizeof(g_projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("lsp_cast_operand", "project/bin", "cast_provider", ".zro",
            g_binaryPath, sizeof(g_binaryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("lsp_cast_operand", "project/bin", "cast_provider", ".zri",
            g_intermediatePath, sizeof(g_intermediatePath)));
    /* 每个二进制用例先尝试删旧 .zri，避免无中间产物分支被前一例污染。
     * TODO: remove 失败结果未核验；若旧文件仍在，无 .zri 场景的夹具前提失效，需在测试前确认文件不存在。 */
    remove(g_intermediatePath);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("lsp_cast_operand", "project/src", "main", ".zr",
            g_mainPath, sizeof(g_mainPath)));
    write_text(g_projectPath, "{\"name\":\"cast\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}");
    write_text(g_mainPath, source);
    sourceName = ZrCore_String_CreateFromNative(g_state, g_binaryPath);
    function = ZrParser_Source_Compile(g_state, provider, strlen(provider), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    /* 二进制模块名与 import 一致；写入后即释放编译结果，LSP 后续从磁盘导入 provider。 */
    options.moduleName = "cast_provider";
    written = ZrParser_Writer_WriteBinaryFileWithOptions(g_state, function, g_binaryPath, &options);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_TRUE(written);
    /* 另造同步或过时 .zri，验证可选中间产物不会改变 cast 前后的外部身份。 */
    if (intermediateSource != ZR_NULL) {
        function = ZrParser_Source_Compile(g_state, intermediateSource, strlen(intermediateSource), sourceName);
        TEST_ASSERT_NOT_NULL(function);
        written = ZrParser_Writer_WriteIntermediateFile(g_state, function, g_intermediatePath);
        ZrCore_Function_Free(g_state, function);
        TEST_ASSERT_TRUE(written);
    }
    assert_cast_preserves_external_call(ZrLanguageServer_LspUri_FromNativePath(g_state, g_mainPath), source, "measure");
}

/* 预期仅有 .zro 时，cast 不能抹掉二进制 provider 的调用身份和定义位置。 */
static void test_binary_cast_operand_retains_external_identity_and_navigation(void) {
    assert_binary_cast_operand(ZR_NULL);
}

/* 与 .zro 同步的 .zri 存在时，cast 前后的外部身份仍应一致。 */
static void test_binary_cast_operand_with_current_intermediate(void) {
    assert_binary_cast_operand("pub var measure = fn(): float => 3.5;\n");
}

/* .zri 声明已过时也不得覆盖 .zro 对 measure 的规范身份。 */
static void test_binary_cast_operand_with_stale_intermediate(void) {
    assert_binary_cast_operand("pub var obsolete = fn(): int => 7;\n");
}

/* 原生 zr.math 提供者也走同一身份与定义投影检查。 */
static void test_native_cast_operand_retains_external_identity_and_navigation(void) {
    const char *source = "var math = import(\"zr.math\");\n"
                         "var plain = math.sqrt(4.0);\n"
                         "var cast = <int> math.sqrt(4.0);\n";
    SZrString *uri = ZrCore_String_CreateFromNative(g_state, "file:///native_cast_operand.zr");
    assert_cast_preserves_external_call(uri, source, "sqrt");
}

/* CTest language_server_cast_operand_facts 通过 Unity 逐例执行四种 provider 场景。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_cast_operand_retains_external_identity_and_navigation);
    RUN_TEST(test_binary_cast_operand_with_current_intermediate);
    RUN_TEST(test_binary_cast_operand_with_stale_intermediate);
    RUN_TEST(test_native_cast_operand_retains_external_identity_and_navigation);
    return UNITY_END();
}
