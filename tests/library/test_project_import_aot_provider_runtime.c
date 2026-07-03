#include "unity.h"

#include "harness/path_support.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *content) {
    FILE *file;
    size_t contentLength;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    contentLength = strlen(content);
    written = fwrite(content, 1, contentLength, file);
    fclose(file);
    return written == contentLength;
}

static void normalize_path_text(TZrChar *path) {
    if (path == ZR_NULL) {
        return;
    }

    for (; *path != '\0'; path++) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

static TZrBool prepare_provider_aot_runtime_fixture(TZrChar *projectPath, TZrSize projectPathSize) {
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"3.4.5\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"mathLocal\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"version\": \"2.1.0\",\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\"\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"2.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    if (projectPath == ZR_NULL || projectPathSize == 0) {
        return ZR_FALSE;
    }
    if (!ZrTests_Path_GetGeneratedArtifact("library",
                                           "project_aot_provider_runtime",
                                           "root",
                                           ".zrp",
                                           projectPath,
                                           projectPathSize)) {
        return ZR_FALSE;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    lastSeparator = strrchr(rootPath, '/');
    if (lastSeparator == ZR_NULL) {
        lastSeparator = strrchr(rootPath, '\\');
    }
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathPath);

    return write_text_file(projectPath, projectContent) && write_text_file(mathPath, mathContent);
}

static void test_aot_runtime_reports_provider_library_path_for_canonical_import(void) {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    SZrGlobalState *global;
    SZrState *state;
    SZrString *moduleName;
    const TZrChar *diagnostic;
    const TZrChar *lastError;
    TZrChar normalizedDiagnostic[ZR_LIBRARY_MAX_PATH_LENGTH * 2];
    TZrChar normalizedLastError[ZR_LIBRARY_MAX_PATH_LENGTH * 2];

    TEST_ASSERT_TRUE(prepare_provider_aot_runtime_fixture(projectPath, sizeof(projectPath)));
    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    moduleName = ZrCore_String_CreateFromNative(state, "$mathLocal@2.1.0/ops/sum");
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NULL(ZrCore_Module_ImportByPath(state, moduleName));

    diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(global);
    lastError = ZrLibrary_AotRuntime_GetLastError(global);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_NOT_NULL(lastError);
    snprintf(normalizedDiagnostic, sizeof(normalizedDiagnostic), "%s", diagnostic);
    snprintf(normalizedLastError, sizeof(normalizedLastError), "%s", lastError);
    normalize_path_text(normalizedDiagnostic);
    normalize_path_text(normalizedLastError);

    TEST_ASSERT_NOT_NULL(strstr(normalizedDiagnostic, "loader=aot-runtime"));
    TEST_ASSERT_NOT_NULL(strstr(normalizedDiagnostic, "backend=aot-c"));
    TEST_ASSERT_NOT_NULL(strstr(normalizedDiagnostic, "descriptor-load-failed"));
    TEST_ASSERT_NOT_NULL(strstr(normalizedDiagnostic, "$mathLocal@2.1.0/ops/sum"));
    TEST_ASSERT_NOT_NULL(strstr(normalizedDiagnostic, "/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum."));
    TEST_ASSERT_NOT_NULL(strstr(normalizedLastError, "missing AOT provider library"));
    TEST_ASSERT_NOT_NULL(strstr(normalizedLastError, "/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum."));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_aot_runtime_reports_provider_library_path_for_canonical_import);

    return UNITY_END();
}
