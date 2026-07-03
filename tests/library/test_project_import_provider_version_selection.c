#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }
    return ZrCore_String_GetNativeString(value);
}

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

static TZrBool make_project_root_path(const TZrChar *baseName,
                                      TZrChar *projectPath,
                                      TZrSize projectPathSize,
                                      TZrChar *rootPath,
                                      TZrSize rootPathSize) {
    TZrChar *lastSeparator;

    if (projectPath == ZR_NULL || rootPath == ZR_NULL || projectPathSize == 0 || rootPathSize == 0 ||
        !ZrTests_Path_GetGeneratedArtifact("library",
                                           "project_import_provider_version_selection",
                                           baseName,
                                           ".zrp",
                                           projectPath,
                                           projectPathSize)) {
        return ZR_FALSE;
    }

    snprintf(rootPath, rootPathSize, "%s", projectPath);
    lastSeparator = strrchr(rootPath, '/');
    if (lastSeparator == ZR_NULL) {
        lastSeparator = strrchr(rootPath, '\\');
    }
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';
    return ZR_TRUE;
}

static void destroy_test_project(SZrState *state, SZrLibrary_Project *project) {
    if (state != ZR_NULL && project != ZR_NULL) {
        ZrLibrary_Project_Free(state, project);
    }
    if (state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
    }
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

static TZrBool text_ends_with(const TZrChar *text, const TZrChar *suffix) {
    TZrSize textLength;
    TZrSize suffixLength;

    if (text == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    textLength = strlen(text);
    suffixLength = strlen(suffix);
    return textLength >= suffixLength && strcmp(text + textLength - suffixLength, suffix) == 0;
}

static void assert_provider_load_request(const SZrLibrary_Project *project,
                                         const TZrChar *specifier,
                                         const TZrChar *expectedModuleKey,
                                         const TZrChar *expectedVersion,
                                         const TZrChar *expectedMinVersion,
                                         const TZrChar *expectedMaxVersion,
                                         const TZrChar *expectedSourceSuffix,
                                         const TZrChar *expectedBinarySuffix) {
    SZrLibrary_ProjectImportProviderAotLoadRequest request;
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(&request, 0, sizeof(request));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_Project_ResolveImportProviderAotLoadRequest(project,
                                                                                   "main",
                                                                                   specifier,
                                                                                   ZR_AOT_BACKEND_KIND_C,
                                                                                   &request,
                                                                                   error,
                                                                                   sizeof(error)),
                             error);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_BACKEND_KIND_C, request.backendKind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_PROJECT, request.artifactKind);
    TEST_ASSERT_EQUAL_STRING(expectedModuleKey, request.resolvedModuleKey);
    TEST_ASSERT_EQUAL_STRING("ops/sum", request.descriptorModuleName);
    TEST_ASSERT_EQUAL_STRING("zr.math", test_string_text(request.assemblyName));
    TEST_ASSERT_EQUAL_STRING(expectedVersion, test_string_text(request.requestedVersion));
    TEST_ASSERT_EQUAL_STRING(expectedMinVersion, test_string_text(request.minVersionInclusive));
    TEST_ASSERT_EQUAL_STRING(expectedMaxVersion, test_string_text(request.maxVersionExclusive));
    normalize_path_text(request.sourcePath);
    normalize_path_text(request.binaryPath);
    normalize_path_text(request.libraryPath);
    TEST_ASSERT_TRUE(text_ends_with(request.sourcePath, expectedSourceSuffix));
    TEST_ASSERT_TRUE(text_ends_with(request.binaryPath, expectedBinarySuffix));
    TEST_ASSERT_NOT_NULL(strstr(request.libraryPath, "/bin/aot_c/lib/zrvm_aot_ops_sum."));
}

static void test_provider_import_selects_declared_alias_version_and_paths(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathV2Path[ZR_TESTS_PATH_MAX];
    TZrChar mathV3Path[ZR_TESTS_PATH_MAX];
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"mathV2\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"version\": \"2.1.0\",\n"
            "      \"path\": \"deps/math_v2/math.zrp\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\"\n"
            "    },\n"
            "    \"mathV3\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"version\": \"3.1.0\",\n"
            "      \"path\": \"deps/math_v3/math.zrp\",\n"
            "      \"minVersionInclusive\": \"3.0.0\",\n"
            "      \"maxVersionExclusive\": \"4.0.0\"\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathV2Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"2.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";
    static const TZrChar *mathV3Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"3.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("multi_version_root",
                                            projectPath,
                                            sizeof(projectPath),
                                            rootPath,
                                            sizeof(rootPath)));
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v2/math.zrp", mathV2Path);
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v3/math.zrp", mathV3Path);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(mathV2Path, mathV2Content));
    TEST_ASSERT_TRUE(write_text_file(mathV3Path, mathV3Content));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->dependencyRefCount);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->dependencyPackageCount);

    assert_provider_load_request(project,
                                 "&mathV2.ops.sum",
                                 "$mathV2@2.1.0/ops/sum",
                                 "2.1.0",
                                 "2.0.0",
                                 "3.0.0",
                                 "/deps/math_v2/src/ops/sum.zr",
                                 "/deps/math_v2/bin/ops/sum.zro");
    assert_provider_load_request(project,
                                 "&mathV3.ops.sum",
                                 "$mathV3@3.1.0/ops/sum",
                                 "3.1.0",
                                 "3.0.0",
                                 "4.0.0",
                                 "/deps/math_v3/src/ops/sum.zr",
                                 "/deps/math_v3/bin/ops/sum.zro");

    destroy_test_project(state, project);
}

static void test_provider_import_selects_highest_candidate_within_declared_range(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathV1Path[ZR_TESTS_PATH_MAX];
    TZrChar mathV2Path[ZR_TESTS_PATH_MAX];
    TZrChar mathV3Path[ZR_TESTS_PATH_MAX];
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"math\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\",\n"
            "      \"candidates\": [\n"
            "        { \"path\": \"deps/math_v1/math.zrp\" },\n"
            "        { \"path\": \"deps/math_v2/math.zrp\" },\n"
            "        { \"path\": \"deps/math_v3/math.zrp\" }\n"
            "      ]\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathV1Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"2.0.5\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";
    static const TZrChar *mathV2Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"2.2.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";
    static const TZrChar *mathV3Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"3.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("range_candidate_root",
                                            projectPath,
                                            sizeof(projectPath),
                                            rootPath,
                                            sizeof(rootPath)));
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v1/math.zrp", mathV1Path);
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v2/math.zrp", mathV2Path);
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v3/math.zrp", mathV3Path);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(mathV1Path, mathV1Content));
    TEST_ASSERT_TRUE(write_text_file(mathV2Path, mathV2Content));
    TEST_ASSERT_TRUE(write_text_file(mathV3Path, mathV3Content));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyPackageCount);

    assert_provider_load_request(project,
                                 "&math.ops.sum",
                                 "$math@2.2.0/ops/sum",
                                 "2.2.0",
                                 "2.0.0",
                                 "3.0.0",
                                 "/deps/math_v2/src/ops/sum.zr",
                                 "/deps/math_v2/bin/ops/sum.zro");

    destroy_test_project(state, project);
}

static void test_provider_import_rejects_manifest_version_outside_declared_range(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathPath[ZR_TESTS_PATH_MAX];
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"mathV3\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"version\": \"3.1.0\",\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\"\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"3.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("out_of_range_root",
                                            projectPath,
                                            sizeof(projectPath),
                                            rootPath,
                                            sizeof(rootPath)));
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathPath);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(mathPath, mathContent));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, projectPath);
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_provider_import_rejects_candidate_set_without_range_match(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathPath[ZR_TESTS_PATH_MAX];
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"math\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\",\n"
            "      \"candidates\": [\n"
            "        { \"path\": \"deps/math/math.zrp\" }\n"
            "      ]\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"3.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("candidate_out_of_range_root",
                                            projectPath,
                                            sizeof(projectPath),
                                            rootPath,
                                            sizeof(rootPath)));
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathPath);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(mathPath, mathContent));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, projectPath);
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_provider_import_selects_declared_alias_version_and_paths);
    RUN_TEST(test_provider_import_selects_highest_candidate_within_declared_range);
    RUN_TEST(test_provider_import_rejects_manifest_version_outside_declared_range);
    RUN_TEST(test_provider_import_rejects_candidate_set_without_range_match);

    return UNITY_END();
}
