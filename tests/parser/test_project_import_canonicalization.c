#include "unity.h"

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SZrProjectImportFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar helperPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedPath[ZR_TESTS_PATH_MAX];
} SZrProjectImportFixture;

typedef struct SZrProjectDependencyImportFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootMainPath[ZR_TESTS_PATH_MAX];
    TZrChar mathMainPath[ZR_TESTS_PATH_MAX];
} SZrProjectDependencyImportFixture;

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

static TZrChar *find_last_path_separator(TZrChar *path) {
    TZrChar *forwardSlash;
    TZrChar *backSlash;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    forwardSlash = strrchr(path, '/');
    backSlash = strrchr(path, '\\');
    if (forwardSlash == ZR_NULL) {
        return backSlash;
    }
    if (backSlash == ZR_NULL) {
        return forwardSlash;
    }

    return forwardSlash > backSlash ? forwardSlash : backSlash;
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

static void normalize_test_path_to_forward_slashes(TZrChar *path) {
    if (path == ZR_NULL) {
        return;
    }

    for (; *path != '\0'; path++) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

static void inject_windows_mixed_separator(TZrChar *path) {
    normalize_test_path_to_forward_slashes(path);

#ifdef ZR_VM_PLATFORM_IS_WIN
    TZrChar *featureSeparator;

    if (path == ZR_NULL) {
        return;
    }

    featureSeparator = strstr(path, "/feature/");
    if (featureSeparator != ZR_NULL) {
        *featureSeparator = '\\';
    }
#endif
}

static TZrBool prepare_project_import_fixture(SZrProjectImportFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"parser_project_import_canonicalization\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"feature/app/main\",\n"
            "  \"pathAliases\": {\n"
            "    \"@shared\": \"common/shared\"\n"
            "  }\n"
            "}\n";
    static const TZrChar *mainContent =
            "var localMath = %import(\".helper.math\");\n"
            "var localMathAgain = %import(\"feature/app/helper/math\");\n"
            "var sharedHash = %import(\"@shared.crypto.hash\");\n"
            "\n"
            "pub func run(): i32 {\n"
            "    return localMath.answer + localMathAgain.answer + sharedHash.seed;\n"
            "}\n";
    static const TZrChar *helperContent =
            "pub var answer = 40;\n";
    static const TZrChar *sharedContent =
            "pub var seed = 2;\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("parser",
                                           "project_import_canonicalization",
                                           "import_relative_alias",
                                           ".zrp",
                                           fixture->projectPath,
                                           sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }

    memset(fixture->mainPath, 0, sizeof(fixture->mainPath));
    memset(fixture->helperPath, 0, sizeof(fixture->helperPath));
    memset(fixture->sharedPath, 0, sizeof(fixture->sharedPath));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "feature/app/main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(sourceRoot, "feature/app/helper/math.zr", fixture->helperPath);
    ZrLibrary_File_PathJoin(sourceRoot, "common/shared/crypto/hash.zr", fixture->sharedPath);

    return write_text_file(fixture->projectPath, projectContent) &&
           write_text_file(fixture->mainPath, mainContent) &&
           write_text_file(fixture->helperPath, helperContent) &&
           write_text_file(fixture->sharedPath, sharedContent);
}

static TZrBool prepare_project_dependency_import_fixture(SZrProjectDependencyImportFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"dependency_import_root\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": {\n"
            "    \"$math\": { \"path\": \"deps/math/math.zrp\", \"version\": \"1.0.0\" }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathProjectContent =
            "{\n"
            "  \"name\": \"math\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"feature/main\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"pathAliases\": { \"@core\": \"core\" },\n"
            "  \"dependencies\": { \"$trig\": \"../trig/trig.zrp\" }\n"
            "}\n";
    static const TZrChar *trigProjectContent =
            "{\n"
            "  \"name\": \"trig\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
    static const TZrChar *rootMainContent =
            "var math = %import(\"&math.ops.sum\");\n"
            "pub func run(): i32 { return 1; }\n";
    static const TZrChar *mathMainContent =
            "var core = %import(\"@core.util\");\n"
            "var helper = %import(\".helper\");\n"
            "var bare = %import(\"bare.local\");\n"
            "var trig = %import(\"&trig.wave\");\n"
            "pub func run(): i32 { return 2; }\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar trigProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar mathSourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar trigSourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathOpsPath[ZR_TESTS_PATH_MAX];
    TZrChar mathCorePath[ZR_TESTS_PATH_MAX];
    TZrChar mathHelperPath[ZR_TESTS_PATH_MAX];
    TZrChar mathBarePath[ZR_TESTS_PATH_MAX];
    TZrChar trigWavePath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("parser",
                                           "project_import_canonicalization",
                                           "dependency_imports",
                                           ".zrp",
                                           fixture->projectPath,
                                           sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }

    memset(fixture->rootMainPath, 0, sizeof(fixture->rootMainPath));
    memset(fixture->mathMainPath, 0, sizeof(fixture->mathMainPath));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "main.zr", fixture->rootMainPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/trig/trig.zrp", trigProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/src", mathSourceRoot);
    ZrLibrary_File_PathJoin(rootPath, "deps/trig/src", trigSourceRoot);
    ZrLibrary_File_PathJoin(mathSourceRoot, "feature/main.zr", fixture->mathMainPath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "ops/sum.zr", mathOpsPath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "core/util.zr", mathCorePath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "feature/helper.zr", mathHelperPath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "bare/local.zr", mathBarePath);
    ZrLibrary_File_PathJoin(trigSourceRoot, "wave.zr", trigWavePath);

    return write_text_file(fixture->projectPath, projectContent) &&
           write_text_file(fixture->rootMainPath, rootMainContent) &&
           write_text_file(mathProjectPath, mathProjectContent) &&
           write_text_file(trigProjectPath, trigProjectContent) &&
           write_text_file(fixture->mathMainPath, mathMainContent) &&
           write_text_file(mathOpsPath, "pub var value = 1;\n") &&
           write_text_file(mathCorePath, "pub var value = 2;\n") &&
           write_text_file(mathHelperPath, "pub var value = 3;\n") &&
           write_text_file(mathBarePath, "pub var value = 4;\n") &&
           write_text_file(trigWavePath, "pub var value = 5;\n");
}

static TZrSize count_static_imports_named(const SZrFunction *function, const TZrChar *moduleName) {
    TZrSize count = 0;

    if (function == ZR_NULL || moduleName == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->staticImportLength; index++) {
        const TZrChar *text = test_string_text(function->staticImports[index]);
        if (text != ZR_NULL && strcmp(text, moduleName) == 0) {
            count++;
        }
    }

    return count;
}

static TZrBool function_contains_module_effect_named(const SZrFunction *function, const TZrChar *moduleName) {
    if (function == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        const TZrChar *text = test_string_text(function->moduleEntryEffects[index].moduleName);
        if (text != ZR_NULL && strcmp(text, moduleName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrFunctionCallableSummary *find_exported_callable_summary_named(const SZrFunction *function,
                                                                              const TZrChar *callableName) {
    if (function == ZR_NULL || callableName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
        const SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[index];
        const TZrChar *text = test_string_text(summary->name);
        if (text != ZR_NULL && strcmp(text, callableName) == 0) {
            return summary;
        }
    }

    return ZR_NULL;
}

static TZrBool callable_summary_contains_module_effect_named(const SZrFunctionCallableSummary *summary,
                                                             const TZrChar *moduleName) {
    if (summary == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < summary->effectCount; index++) {
        const TZrChar *text = test_string_text(summary->effects[index].moduleName);
        if (text != ZR_NULL && strcmp(text, moduleName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_project_compile_canonicalizes_relative_and_alias_imports(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    const SZrFunctionCallableSummary *runSummary;
    SZrString *sourceName;
    TZrSize mainLength = 0;
    TZrChar *mainContent;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    mainContent = ZrTests_ReadTextFile(fixture.mainPath, &mainLength);
    TEST_ASSERT_NOT_NULL(mainContent);
    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, mainContent, mainLength, sourceName);
    free(mainContent);
    TEST_ASSERT_NOT_NULL(function);
    runSummary = find_exported_callable_summary_named(function, "run");
    TEST_ASSERT_NOT_NULL(runSummary);

    TEST_ASSERT_EQUAL_UINT32(2u, function->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "feature/app/helper/math"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "common/shared/crypto/hash"));
    TEST_ASSERT_TRUE(callable_summary_contains_module_effect_named(runSummary, "feature/app/helper/math"));
    TEST_ASSERT_TRUE(callable_summary_contains_module_effect_named(runSummary, "common/shared/crypto/hash"));
    TEST_ASSERT_FALSE(function_contains_module_effect_named(function, ".helper.math"));
    TEST_ASSERT_FALSE(function_contains_module_effect_named(function, "@shared.crypto.hash"));
    TEST_ASSERT_FALSE(callable_summary_contains_module_effect_named(runSummary, ".helper.math"));
    TEST_ASSERT_FALSE(callable_summary_contains_module_effect_named(runSummary, "@shared.crypto.hash"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_canonicalizes_dependency_imports(void) {
    SZrProjectDependencyImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *rootFunction;
    SZrFunction *mathFunction;
    SZrString *rootSourceName;
    SZrString *mathSourceName;
    TZrSize rootLength = 0;
    TZrSize mathLength = 0;
    TZrChar *rootContent;
    TZrChar *mathContent;

    TEST_ASSERT_TRUE(prepare_project_dependency_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    rootContent = ZrTests_ReadTextFile(fixture.rootMainPath, &rootLength);
    TEST_ASSERT_NOT_NULL(rootContent);
    rootSourceName = ZrCore_String_CreateFromNative(state, fixture.rootMainPath);
    TEST_ASSERT_NOT_NULL(rootSourceName);
    rootFunction = ZrParser_Source_Compile(state, rootContent, rootLength, rootSourceName);
    free(rootContent);
    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, rootFunction->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(rootFunction, "$math@1.0.0/ops/sum"));

    mathContent = ZrTests_ReadTextFile(fixture.mathMainPath, &mathLength);
    TEST_ASSERT_NOT_NULL(mathContent);
    mathSourceName = ZrCore_String_CreateFromNative(state, fixture.mathMainPath);
    TEST_ASSERT_NOT_NULL(mathSourceName);
    mathFunction = ZrParser_Source_Compile(state, mathContent, mathLength, mathSourceName);
    free(mathContent);
    TEST_ASSERT_NOT_NULL(mathFunction);
    TEST_ASSERT_EQUAL_UINT32(4u, mathFunction->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$math@1.0.0/core/util"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$math@1.0.0/feature/helper"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$math@1.0.0/bare/local"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$trig@1.0.0/wave"));

    ZrCore_Function_Free(state, mathFunction);
    ZrCore_Function_Free(state, rootFunction);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_derive_current_module_key_accepts_mixed_windows_separators(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    const SZrLibrary_Project *project;
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar currentModuleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar errorBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_GetFromGlobal(global);
    TEST_ASSERT_NOT_NULL(project);

    snprintf(sourcePath, sizeof(sourcePath), "%s", fixture.mainPath);
    inject_windows_mixed_separator(sourcePath);
    memset(currentModuleKey, 0, sizeof(currentModuleKey));
    memset(errorBuffer, 0, sizeof(errorBuffer));

    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                              sourcePath,
                                                              ZR_NULL,
                                                              currentModuleKey,
                                                              sizeof(currentModuleKey),
                                                              errorBuffer,
                                                              sizeof(errorBuffer)));
    TEST_ASSERT_EQUAL_STRING("feature/app/main", currentModuleKey);

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_rejects_explicit_module_key_path_mismatch(void) {
    SZrProjectImportFixture fixture;
    static const TZrChar *mismatchSource =
            "%module(\"feature/other/main\");\n"
            "pub var value = 1;\n";
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, mismatchSource, strlen(mismatchSource), sourceName);
    TEST_ASSERT_NULL(function);

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_compile_canonicalizes_relative_and_alias_imports);
    RUN_TEST(test_project_compile_canonicalizes_dependency_imports);
    RUN_TEST(test_project_derive_current_module_key_accepts_mixed_windows_separators);
    RUN_TEST(test_project_compile_rejects_explicit_module_key_path_mismatch);

    return UNITY_END();
}
