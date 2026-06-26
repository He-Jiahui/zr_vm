#include "unity.h"

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

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

static TZrBool make_project_root_path(const TZrChar *suiteName,
                                      const TZrChar *baseName,
                                      TZrChar *projectPath,
                                      TZrSize projectPathSize,
                                      TZrChar *rootPath,
                                      TZrSize rootPathSize) {
    TZrChar *lastSeparator;

    if (projectPath == ZR_NULL || rootPath == ZR_NULL || projectPathSize == 0 || rootPathSize == 0 ||
        !ZrTests_Path_GetGeneratedArtifact("library",
                                           suiteName,
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

static void assert_import_resolves(const SZrLibrary_Project *project,
                                   const TZrChar *currentModuleKey,
                                   const TZrChar *rawSpecifier,
                                   const TZrChar *expectedModuleKey) {
    TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveImportModuleKey(project,
                                                             currentModuleKey,
                                                             rawSpecifier,
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING(expectedModuleKey, resolved);
}

static void test_project_manifest_normalization_keeps_identical_old_and_new_reference_once(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathPath[ZR_TESTS_PATH_MAX];
    SZrString *assemblyName = ZR_NULL;
    SZrString *requestedVersion = ZR_NULL;
    SZrString *minVersionInclusive = ZR_NULL;
    SZrString *maxVersionExclusive = ZR_NULL;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": {\n"
            "    \"$math\": {\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"version\": \"2.1.0\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\"\n"
            "    }\n"
            "  },\n"
            "  \"references\": {\n"
            "    \"math\": {\n"
            "      \"assembly\": \"math\",\n"
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
            "  \"assembly\": { \"name\": \"math\", \"version\": \"2.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("project_manifest_normalization",
                                            "identical_old_new",
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
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyPackageCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyRefCount);

    assert_import_resolves(project, "main", "&math.ops.sum", "$math@2.1.0/ops/sum");
    TEST_ASSERT_TRUE(ZrLibrary_Project_GetDependencyImportVersionRange(project,
                                                                       "main",
                                                                       "$math@2.1.0/ops/sum",
                                                                       &assemblyName,
                                                                       &requestedVersion,
                                                                       &minVersionInclusive,
                                                                       &maxVersionExclusive));
    TEST_ASSERT_EQUAL_STRING("math", test_string_text(assemblyName));
    TEST_ASSERT_EQUAL_STRING("2.1.0", test_string_text(requestedVersion));
    TEST_ASSERT_EQUAL_STRING("2.0.0", test_string_text(minVersionInclusive));
    TEST_ASSERT_EQUAL_STRING("3.0.0", test_string_text(maxVersionExclusive));

    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_conflicting_old_and_new_reference(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathV1Path[ZR_TESTS_PATH_MAX];
    TZrChar mathV2Path[ZR_TESTS_PATH_MAX];
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": { \"$math\": { \"path\": \"deps/math_v1/math.zrp\", \"version\": \"2.1.0\" } },\n"
            "  \"references\": { \"math\": { \"assembly\": \"math\", \"version\": \"2.2.0\", \"path\": \"deps/math_v2/math.zrp\" } }\n"
            "}\n";
    static const TZrChar *mathV1Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"math\", \"version\": \"2.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";
    static const TZrChar *mathV2Content =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"math\", \"version\": \"2.2.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("project_manifest_normalization",
                                            "conflicting_old_new",
                                            projectPath,
                                            sizeof(projectPath),
                                            rootPath,
                                            sizeof(rootPath)));
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v1/math.zrp", mathV1Path);
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v2/math.zrp", mathV2Path);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(mathV1Path, mathV1Content));
    TEST_ASSERT_TRUE(write_text_file(mathV2Path, mathV2Content));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, projectPath);
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_accepts_legacy_dependency_declared_assembly(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathPath[ZR_TESTS_PATH_MAX];
    SZrString *assemblyName = ZR_NULL;
    SZrString *requestedVersion = ZR_NULL;
    SZrString *minVersionInclusive = ZR_NULL;
    SZrString *maxVersionExclusive = ZR_NULL;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": {\n"
            "    \"$math\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"version\": \"2.1.0\"\n"
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

    TEST_ASSERT_TRUE(make_project_root_path("project_manifest_normalization",
                                            "legacy_declared_assembly",
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
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyPackageCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyRefCount);
    TEST_ASSERT_EQUAL_STRING("math", test_string_text(project->dependencyPackages[0].name));
    TEST_ASSERT_EQUAL_STRING("zr.math", test_string_text(project->dependencyPackages[0].assemblyName));
    TEST_ASSERT_EQUAL_STRING("zr.math", test_string_text(project->dependencyRefs[0].assemblyName));

    assert_import_resolves(project, "main", "&math.ops.sum", "$math@2.1.0/ops/sum");
    TEST_ASSERT_TRUE(ZrLibrary_Project_GetDependencyImportVersionRange(project,
                                                                       "main",
                                                                       "$math@2.1.0/ops/sum",
                                                                       &assemblyName,
                                                                       &requestedVersion,
                                                                       &minVersionInclusive,
                                                                       &maxVersionExclusive));
    TEST_ASSERT_EQUAL_STRING("zr.math", test_string_text(assemblyName));
    TEST_ASSERT_EQUAL_STRING("2.1.0", test_string_text(requestedVersion));
    TEST_ASSERT_NULL(minVersionInclusive);
    TEST_ASSERT_NULL(maxVersionExclusive);

    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_legacy_dependency_declared_assembly_mismatch(void) {
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
            "  \"dependencies\": {\n"
            "    \"$math\": {\n"
            "      \"name\": \"math\",\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"version\": \"1.0.0\"\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"physics\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\"\n"
            "}\n";

    TEST_ASSERT_TRUE(make_project_root_path("project_manifest_normalization",
                                            "legacy_declared_assembly_mismatch",
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

static void test_project_manifest_normalization_rejects_unsupported_manifest_version(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 2,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_lowercases_public_key_token(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": {\n"
            "    \"name\": \"app.render\",\n"
            "    \"version\": \"1.0.0\",\n"
            "    \"publicKeyToken\": \"A1B2C3D4E5F60708\"\n"
            "  },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d4e5f60708", test_string_text(project->assemblyPublicKeyToken));
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_public_key_token(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": {\n"
            "    \"name\": \"app.render\",\n"
            "    \"version\": \"1.0.0\",\n"
            "    \"publicKeyToken\": \"not-hex\"\n"
            "  },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_legacy_assembly_name(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"app render\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_legacy_version(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"app.render\",\n"
            "  \"version\": \"1/0/0\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_defaults_assembly_identity_fields(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_STRING("app.render", test_string_text(project->assemblyName));
    TEST_ASSERT_EQUAL_STRING("0.0.0", test_string_text(project->version));
    TEST_ASSERT_EQUAL_STRING("neutral", test_string_text(project->assemblyCulture));
    TEST_ASSERT_NULL(project->assemblyPublicKeyToken);
    TEST_ASSERT_EQUAL_STRING("library", test_string_text(project->assemblyKind));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID, project->aotMode);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_parses_full_aot_mode(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"aotMode\": \"full-aot\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/full-aot.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT, project->aotMode);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_aot_mode(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"aotMode\": \"full\"\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/invalid-aot-mode.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_parses_preserve_rules(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"type\", \"target\": \"Foo\", \"members\": \"all\" },\n"
            "    { \"kind\": \"method\", \"target\": \"Baz.run\" }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->preserveRuleCount);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE, project->preserveRules[0].kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_ALL, project->preserveRules[0].members);
    TEST_ASSERT_EQUAL_STRING("Foo", test_string_text(project->preserveRules[0].target));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD, project->preserveRules[1].kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_DEFAULT, project->preserveRules[1].members);
    TEST_ASSERT_EQUAL_STRING("Baz.run", test_string_text(project->preserveRules[1].target));
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_parses_feature_conditioned_preserve_rule(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    {\n"
            "      \"kind\": \"method\",\n"
            "      \"target\": \"Baz.run\",\n"
            "      \"feature\": \"EnableFastAot\",\n"
            "      \"featureValue\": true\n"
            "    },\n"
            "    {\n"
            "      \"kind\": \"type\",\n"
            "      \"target\": \"Widget\",\n"
            "      \"members\": \"methods\",\n"
            "      \"feature\": \"EnableFastAot\",\n"
            "      \"featureValue\": false\n"
            "    }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->preserveRuleCount);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD, project->preserveRules[0].kind);
    TEST_ASSERT_EQUAL_STRING("Baz.run", test_string_text(project->preserveRules[0].target));
    TEST_ASSERT_EQUAL_STRING("EnableFastAot", test_string_text(project->preserveRules[0].feature));
    TEST_ASSERT_TRUE(project->preserveRules[0].hasFeatureValue);
    TEST_ASSERT_TRUE(project->preserveRules[0].featureValue);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE, project->preserveRules[1].kind);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_METHODS, project->preserveRules[1].members);
    TEST_ASSERT_EQUAL_STRING("Widget", test_string_text(project->preserveRules[1].target));
    TEST_ASSERT_EQUAL_STRING("EnableFastAot", test_string_text(project->preserveRules[1].feature));
    TEST_ASSERT_TRUE(project->preserveRules[1].hasFeatureValue);
    TEST_ASSERT_FALSE(project->preserveRules[1].featureValue);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_parses_generic_preserve_arguments(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    {\n"
            "      \"kind\": \"generic\",\n"
            "      \"target\": \"List\",\n"
            "      \"arguments\": [\"Foo\", \"Bar.Baz\"],\n"
            "      \"feature\": \"EnableFastAot\",\n"
            "      \"featureValue\": true\n"
            "    }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->preserveRuleCount);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC, project->preserveRules[0].kind);
    TEST_ASSERT_EQUAL_STRING("List", test_string_text(project->preserveRules[0].target));
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->preserveRules[0].genericArgumentCount);
    TEST_ASSERT_EQUAL_STRING("Foo", test_string_text(project->preserveRules[0].genericArguments[0]));
    TEST_ASSERT_EQUAL_STRING("Bar.Baz", test_string_text(project->preserveRules[0].genericArguments[1]));
    TEST_ASSERT_EQUAL_STRING("EnableFastAot", test_string_text(project->preserveRules[0].feature));
    TEST_ASSERT_TRUE(project->preserveRules[0].hasFeatureValue);
    TEST_ASSERT_TRUE(project->preserveRules[0].featureValue);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_generic_preserve_arguments(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"generic\", \"target\": \"List\", \"arguments\": [\"Foo Bar\"] }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_non_array_generic_preserve_arguments(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"generic\", \"target\": \"List\", \"arguments\": \"Foo\" }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_generic_preserve_without_arguments(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"generic\", \"target\": \"List\" }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_empty_generic_preserve_arguments(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"generic\", \"target\": \"List\", \"arguments\": [] }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_arguments_on_non_generic_preserve_rule(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"method\", \"target\": \"List.run\", \"arguments\": [\"Foo\"] }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_parses_feature_switches(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"features\": {\n"
            "    \"EnableFastAot\": true,\n"
            "    \"KeepDiagnostics\": false\n"
            "  }\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->featureSwitchCount);
    TEST_ASSERT_EQUAL_STRING("EnableFastAot", test_string_text(project->featureSwitches[0].name));
    TEST_ASSERT_TRUE(project->featureSwitches[0].value);
    TEST_ASSERT_EQUAL_STRING("KeepDiagnostics", test_string_text(project->featureSwitches[1].name));
    TEST_ASSERT_FALSE(project->featureSwitches[1].value);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_feature_switch(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"features\": {\n"
            "    \"Enable Fast Aot\": true\n"
            "  }\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_preserve_feature_value_without_feature(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"method\", \"target\": \"Baz.run\", \"featureValue\": true }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_preserve_feature_without_value(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"method\", \"target\": \"Baz.run\", \"feature\": \"EnableFastAot\" }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

static void test_project_manifest_normalization_rejects_invalid_preserve_rule(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"1.0.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"preserve\": [\n"
            "    { \"kind\": \"type\", \"target\": \"Foo Bar\", \"members\": \"all\" }\n"
            "  ]\n"
            "}\n";

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectContent, "E:/repo/app/app.zrp");
    TEST_ASSERT_NULL(project);
    destroy_test_project(state, project);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_manifest_normalization_keeps_identical_old_and_new_reference_once);
    RUN_TEST(test_project_manifest_normalization_rejects_conflicting_old_and_new_reference);
    RUN_TEST(test_project_manifest_normalization_accepts_legacy_dependency_declared_assembly);
    RUN_TEST(test_project_manifest_normalization_rejects_legacy_dependency_declared_assembly_mismatch);
    RUN_TEST(test_project_manifest_normalization_rejects_unsupported_manifest_version);
    RUN_TEST(test_project_manifest_normalization_lowercases_public_key_token);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_public_key_token);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_legacy_assembly_name);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_legacy_version);
    RUN_TEST(test_project_manifest_normalization_defaults_assembly_identity_fields);
    RUN_TEST(test_project_manifest_normalization_parses_full_aot_mode);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_aot_mode);
    RUN_TEST(test_project_manifest_normalization_parses_preserve_rules);
    RUN_TEST(test_project_manifest_normalization_parses_feature_conditioned_preserve_rule);
    RUN_TEST(test_project_manifest_normalization_parses_generic_preserve_arguments);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_generic_preserve_arguments);
    RUN_TEST(test_project_manifest_normalization_rejects_non_array_generic_preserve_arguments);
    RUN_TEST(test_project_manifest_normalization_rejects_generic_preserve_without_arguments);
    RUN_TEST(test_project_manifest_normalization_rejects_empty_generic_preserve_arguments);
    RUN_TEST(test_project_manifest_normalization_rejects_arguments_on_non_generic_preserve_rule);
    RUN_TEST(test_project_manifest_normalization_parses_feature_switches);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_feature_switch);
    RUN_TEST(test_project_manifest_normalization_rejects_preserve_feature_value_without_feature);
    RUN_TEST(test_project_manifest_normalization_rejects_preserve_feature_without_value);
    RUN_TEST(test_project_manifest_normalization_rejects_invalid_preserve_rule);

    return UNITY_END();
}
