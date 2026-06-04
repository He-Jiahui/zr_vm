#include "unity.h"

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

#include <stdio.h>
#include <string.h>

extern TZrBool ZrLibrary_Project_ResolveImportModuleKey(const SZrLibrary_Project *project,
                                                        const TZrChar *currentModuleKey,
                                                        const TZrChar *rawSpecifier,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);
extern TZrBool ZrLibrary_Project_DeriveCurrentModuleKey(const SZrLibrary_Project *project,
                                                        const TZrChar *sourceName,
                                                        const TZrChar *explicitModuleKey,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);

void setUp(void) {}

void tearDown(void) {}

static SZrLibrary_Project *create_test_project(SZrState **outState) {
    static const TZrChar manifestText[] =
            "{"
            "\"name\":\"resolver-fixture\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"feature/app/main\","
            "\"pathAliases\":{"
            "\"@app\":\"feature/app\","
            "\"@shared\":\"common/shared\""
            "}"
            "}";
    SZrState *state;
    SZrLibrary_Project *project;

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, manifestText, "E:/repo/resolver-fixture/resolver-fixture.zrp");
    TEST_ASSERT_NOT_NULL(project);
    *outState = state;
    return project;
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

static SZrLibrary_Project *create_dependency_test_project(SZrState **outState, TZrChar *outProjectPath, TZrSize projectPathSize) {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathPath[ZR_TESTS_PATH_MAX];
    TZrChar mathDuplicatePath[ZR_TESTS_PATH_MAX];
    TZrChar mathV2Path[ZR_TESTS_PATH_MAX];
    TZrChar trigPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    SZrState *state;
    SZrLibrary_Project *project;

    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"root\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": {\n"
            "    \"$math\": { \"path\": \"deps/math/math.zrp\", \"version\": \"1.0.0\" },\n"
            "    \"$mathSame\": \"deps/math_copy/math.zrp\",\n"
            "    \"$mathV2\": \"deps/math_v2/math.zrp\"\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathContent =
            "{\n"
            "  \"name\": \"math\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"pathAliases\": { \"@core\": \"core\" },\n"
            "  \"dependencies\": { \"$trig\": \"../trig/trig.zrp\" }\n"
            "}\n";
    static const TZrChar *mathV2Content =
            "{\n"
            "  \"name\": \"math\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"index\",\n"
            "  \"version\": \"2.0.0\"\n"
            "}\n";
    static const TZrChar *trigContent =
            "{\n"
            "  \"name\": \"trig\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";

    if (outProjectPath != ZR_NULL && projectPathSize > 0) {
        outProjectPath[0] = '\0';
    }
    if (!ZrTests_Path_GetGeneratedArtifact("library",
                                           "project_dependencies",
                                           "root",
                                           ".zrp",
                                           projectPath,
                                           sizeof(projectPath))) {
        return ZR_NULL;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    lastSeparator = strrchr(rootPath, '/');
    if (lastSeparator == ZR_NULL) {
        lastSeparator = strrchr(rootPath, '\\');
    }
    if (lastSeparator == ZR_NULL) {
        return ZR_NULL;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math_copy/math.zrp", mathDuplicatePath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math_v2/math.zrp", mathV2Path);
    ZrLibrary_File_PathJoin(rootPath, "deps/trig/trig.zrp", trigPath);

    if (!write_text_file(projectPath, projectContent) ||
        !write_text_file(mathPath, mathContent) ||
        !write_text_file(mathDuplicatePath, mathContent) ||
        !write_text_file(mathV2Path, mathV2Content) ||
        !write_text_file(trigPath, trigContent)) {
        return ZR_NULL;
    }

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, projectContent, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    if (outProjectPath != ZR_NULL && projectPathSize > 0) {
        snprintf(outProjectPath, projectPathSize, "%s", projectPath);
    }
    *outState = state;
    return project;
}

static TZrBool prepare_manifest_validation_fixture(const TZrChar *baseName,
                                                   const TZrChar *dependencyContent,
                                                   const TZrChar *declaredVersion,
                                                   TZrChar *projectPath,
                                                   TZrSize projectPathSize,
                                                   TZrChar *projectContent,
                                                   TZrSize projectContentSize) {
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar dependencyPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (baseName == ZR_NULL || dependencyContent == ZR_NULL || projectPath == ZR_NULL || projectContent == ZR_NULL ||
        projectPathSize == 0 || projectContentSize == 0) {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_GetGeneratedArtifact("library",
                                           "project_dependency_validation",
                                           baseName,
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

    ZrLibrary_File_PathJoin(rootPath, "deps/bad/bad.zrp", dependencyPath);
    if (declaredVersion != ZR_NULL) {
        snprintf(projectContent,
                 projectContentSize,
                 "{\n"
                 "  \"source\": \"src\",\n"
                 "  \"binary\": \"bin\",\n"
                 "  \"entry\": \"main\",\n"
                 "  \"dependencies\": { \"$bad\": { \"path\": \"deps/bad/bad.zrp\", \"version\": \"%s\" } }\n"
                 "}\n",
                 declaredVersion);
    } else {
        snprintf(projectContent,
                 projectContentSize,
                 "{\n"
                 "  \"source\": \"src\",\n"
                 "  \"binary\": \"bin\",\n"
                 "  \"entry\": \"main\",\n"
                 "  \"dependencies\": { \"$bad\": \"deps/bad/bad.zrp\" }\n"
                 "}\n");
    }

    return write_text_file(projectPath, projectContent) && write_text_file(dependencyPath, dependencyContent);
}

static void assert_project_manifest_rejected(const TZrChar *baseName,
                                             const TZrChar *dependencyContent,
                                             const TZrChar *declaredVersion) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar projectContent[ZR_TESTS_PATH_MAX];

    memset(projectPath, 0, sizeof(projectPath));
    memset(projectContent, 0, sizeof(projectContent));
    TEST_ASSERT_TRUE(prepare_manifest_validation_fixture(baseName,
                                                         dependencyContent,
                                                         declaredVersion,
                                                         projectPath,
                                                         sizeof(projectPath),
                                                         projectContent,
                                                         sizeof(projectContent)));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, projectContent, projectPath);
    TEST_ASSERT_NULL(project);
    ZrTests_Runtime_State_Destroy(state);
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

static void assert_import_rejected(const SZrLibrary_Project *project,
                                   const TZrChar *currentModuleKey,
                                   const TZrChar *rawSpecifier) {
    TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Project_ResolveImportModuleKey(project,
                                                              currentModuleKey,
                                                              rawSpecifier,
                                                              resolved,
                                                              sizeof(resolved),
                                                              error,
                                                              sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);
}

static void test_project_import_resolver_accepts_relative_and_alias_specifiers(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_test_project(&state);

    assert_import_resolves(project, "feature/app/main", "feature/app/absolute", "feature/app/absolute");
    assert_import_resolves(project, "feature/app/main", ".helper.util", "feature/app/helper/util");
    assert_import_resolves(project, "feature/app/main", "..common.math", "feature/common/math");
    assert_import_resolves(project, "feature/app/main", "...root.entry", "root/entry");
    assert_import_resolves(project, "feature/app/main", "@app", "feature/app");
    assert_import_resolves(project, "feature/app/main", "@app.widgets.card", "feature/app/widgets/card");
    assert_import_resolves(project, "feature/app/main", "@shared.crypto.hash", "common/shared/crypto/hash");

    destroy_test_project(state, project);
}

static void test_project_import_resolver_rejects_invalid_relative_and_alias_forms(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_test_project(&state);

    assert_import_rejected(project, "feature/app/main", "./foo");
    assert_import_rejected(project, "feature/app/main", "../foo");
    assert_import_rejected(project, "feature/app/main", ".");
    assert_import_rejected(project, "feature/app/main", "..");
    assert_import_rejected(project, "feature/app/main", ".foo..bar");
    assert_import_rejected(project, "feature/app/main", "@missing");
    assert_import_rejected(project, "feature/app/main", "@app/foo");
    assert_import_rejected(project, "feature/app/main", "@app.");
    assert_import_rejected(project, "main", "..shared.tools");

    destroy_test_project(state, project);
}

static void test_project_import_resolver_derives_current_module_key_from_source_root_and_detects_mismatch(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_test_project(&state);
    TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                             "E:/repo/resolver-fixture/src/feature/app/main.zr",
                                                             ZR_NULL,
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("feature/app/main", resolved);

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                             "feature/app/helper",
                                                             ZR_NULL,
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("feature/app/helper", resolved);

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                             "E:/repo/resolver-fixture/src/feature/app/main.zr",
                                                             "feature/app/main",
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("feature/app/main", resolved);

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                              "E:/repo/resolver-fixture/src/feature/app/main.zr",
                                                              "feature/other/main",
                                                              resolved,
                                                              sizeof(resolved),
                                                              error,
                                                              sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);

    destroy_test_project(state, project);
}

static void test_project_import_resolver_resolves_dependency_imports_and_scopes(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_dependency_test_project(&state, ZR_NULL, 0);
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)project->dependencyRefCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)project->dependencyPackageCount);
    TEST_ASSERT_EQUAL_UINT32(project->dependencyRefs[0].packageIndex, project->dependencyRefs[1].packageIndex);
    TEST_ASSERT_NOT_EQUAL(project->dependencyRefs[0].packageIndex, project->dependencyRefs[2].packageIndex);

    assert_import_resolves(project, "main", "&math.aaa.bbb", "$math@1.0.0/aaa/bbb");
    assert_import_resolves(project, "main", "&math", "$math@1.0.0/index");
    assert_import_resolves(project, "main", "&mathSame", "$math@1.0.0/index");
    assert_import_resolves(project, "$math@1.0.0/feature/main", "@core.util", "$math@1.0.0/core/util");
    assert_import_resolves(project, "$math@1.0.0/feature/main", ".helper", "$math@1.0.0/feature/helper");
    assert_import_resolves(project, "$math@1.0.0/feature/main", "bare.local", "$math@1.0.0/bare/local");
    assert_import_resolves(project, "$math@1.0.0/feature/main", "&trig.wave", "$trig@1.0.0/wave");
    assert_import_resolves(project, "main", "&mathV2.aaa", "$math@2.0.0/aaa");
    assert_import_resolves(project, "$math@1.0.0/feature/main", "zr.system", "zr.system");
    assert_import_rejected(project, "main", "&missing.tool");

    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveSourcePath(project,
                                                         "$math@1.0.0/aaa/bbb",
                                                         sourcePath,
                                                         sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveBinaryPath(project,
                                                         "$math@1.0.0/aaa/bbb",
                                                         binaryPath,
                                                         sizeof(binaryPath)));
    normalize_path_text(sourcePath);
    normalize_path_text(binaryPath);
    TEST_ASSERT_TRUE(text_ends_with(sourcePath, "/deps/math/src/aaa/bbb.zr"));
    TEST_ASSERT_TRUE(text_ends_with(binaryPath, "/deps/math/bin/aaa/bbb.zro"));

    destroy_test_project(state, project);
}

static void test_project_dependency_manifest_validation_rejects_invalid_declarations(void) {
    assert_project_manifest_rejected("version_mismatch",
                                     "{\n"
                                     "  \"name\": \"bad\",\n"
                                     "  \"source\": \"src\",\n"
                                     "  \"binary\": \"bin\",\n"
                                     "  \"entry\": \"main\",\n"
                                     "  \"version\": \"2.0.0\"\n"
                                     "}\n",
                                     "1.0.0");
    assert_project_manifest_rejected("invalid_name",
                                     "{\n"
                                     "  \"name\": \"bad.name\",\n"
                                     "  \"source\": \"src\",\n"
                                     "  \"binary\": \"bin\",\n"
                                     "  \"entry\": \"main\",\n"
                                     "  \"version\": \"1.0.0\"\n"
                                     "}\n",
                                     ZR_NULL);
    assert_project_manifest_rejected("invalid_version",
                                     "{\n"
                                     "  \"name\": \"bad\",\n"
                                     "  \"source\": \"src\",\n"
                                     "  \"binary\": \"bin\",\n"
                                     "  \"entry\": \"main\",\n"
                                     "  \"version\": \"1.0/0\"\n"
                                     "}\n",
                                     ZR_NULL);
    assert_project_manifest_rejected("invalid_entry",
                                     "{\n"
                                     "  \"name\": \"bad\",\n"
                                     "  \"source\": \"src\",\n"
                                     "  \"binary\": \"bin\",\n"
                                     "  \"entry\": \"\",\n"
                                     "  \"version\": \"1.0.0\"\n"
                                     "}\n",
                                     ZR_NULL);
}

static void test_project_dependency_parser_handles_manifest_cycles(void) {
    SZrState *state;
    SZrLibrary_Project *project;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar aPath[ZR_TESTS_PATH_MAX];
    TZrChar bPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    static const TZrChar *projectContent =
            "{\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": { \"$a\": \"deps/a/a.zrp\" }\n"
            "}\n";
    static const TZrChar *aContent =
            "{\n"
            "  \"name\": \"a\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"dependencies\": { \"$b\": \"../b/b.zrp\" }\n"
            "}\n";
    static const TZrChar *bContent =
            "{\n"
            "  \"name\": \"b\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"dependencies\": { \"$a\": \"../a/a.zrp\" }\n"
            "}\n";

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library",
                                                       "project_dependency_cycles",
                                                       "root",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    lastSeparator = strrchr(rootPath, '/');
    if (lastSeparator == ZR_NULL) {
        lastSeparator = strrchr(rootPath, '\\');
    }
    TEST_ASSERT_NOT_NULL(lastSeparator);
    *lastSeparator = '\0';
    ZrLibrary_File_PathJoin(rootPath, "deps/a/a.zrp", aPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/b/b.zrp", bPath);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(aPath, aContent));
    TEST_ASSERT_TRUE(write_text_file(bPath, bContent));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, projectContent, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)project->dependencyPackageCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyPackages[0].dependencyRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)project->dependencyPackages[1].dependencyRefCount);
    assert_import_resolves(project, "main", "&a", "$a@1.0.0/main");
    assert_import_resolves(project, "$a@1.0.0/main", "&b", "$b@1.0.0/main");
    assert_import_resolves(project, "$b@1.0.0/main", "&a", "$a@1.0.0/main");

    destroy_test_project(state, project);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_import_resolver_accepts_relative_and_alias_specifiers);
    RUN_TEST(test_project_import_resolver_rejects_invalid_relative_and_alias_forms);
    RUN_TEST(test_project_import_resolver_derives_current_module_key_from_source_root_and_detects_mismatch);
    RUN_TEST(test_project_import_resolver_resolves_dependency_imports_and_scopes);
    RUN_TEST(test_project_dependency_manifest_validation_rejects_invalid_declarations);
    RUN_TEST(test_project_dependency_parser_handles_manifest_cycles);

    return UNITY_END();
}
