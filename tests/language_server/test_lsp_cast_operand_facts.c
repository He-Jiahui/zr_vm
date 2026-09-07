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

static SZrState *g_state;
static SZrLspContext *g_context;
static SZrArray g_plainLocations;
static SZrArray g_castLocations;
static char g_projectPath[ZR_TESTS_PATH_MAX];
static char g_binaryPath[ZR_TESTS_PATH_MAX];
static char g_mainPath[ZR_TESTS_PATH_MAX];

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

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_context = ZrLanguageServer_LspContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
    ZrCore_Array_Construct(&g_plainLocations);
    ZrCore_Array_Construct(&g_castLocations);
    g_projectPath[0] = g_binaryPath[0] = g_mainPath[0] = '\0';
}

void tearDown(void) {
    free_locations(&g_plainLocations);
    free_locations(&g_castLocations);
    ZrLanguageServer_LspContext_Free(g_state, g_context);
    ZrTests_Runtime_State_Destroy(g_state);
    if (g_projectPath[0] != '\0') {
        remove(g_projectPath);
        remove(g_binaryPath);
        remove(g_mainPath);
    }
}

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

static void test_binary_cast_operand_retains_external_identity_and_navigation(void) {
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
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("lsp_cast_operand", "project/src", "main", ".zr",
            g_mainPath, sizeof(g_mainPath)));
    write_text(g_projectPath, "{\"name\":\"cast\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}");
    write_text(g_mainPath, source);
    sourceName = ZrCore_String_CreateFromNative(g_state, g_binaryPath);
    function = ZrParser_Source_Compile(g_state, provider, strlen(provider), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    options.moduleName = "cast_provider";
    written = ZrParser_Writer_WriteBinaryFileWithOptions(g_state, function, g_binaryPath, &options);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_TRUE(written);
    assert_cast_preserves_external_call(ZrLanguageServer_LspUri_FromNativePath(g_state, g_mainPath), source, "measure");
}

static void test_native_cast_operand_retains_external_identity_and_navigation(void) {
    const char *source = "var math = import(\"zr.math\");\n"
                         "var plain = math.sqrt(4.0);\n"
                         "var cast = <int> math.sqrt(4.0);\n";
    SZrString *uri = ZrCore_String_CreateFromNative(g_state, "file:///native_cast_operand.zr");
    assert_cast_preserves_external_call(uri, source, "sqrt");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_cast_operand_retains_external_identity_and_navigation);
    RUN_TEST(test_native_cast_operand_retains_external_identity_and_navigation);
    return UNITY_END();
}
