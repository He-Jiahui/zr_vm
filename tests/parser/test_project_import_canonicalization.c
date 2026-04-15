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
    RUN_TEST(test_project_derive_current_module_key_accepts_mixed_windows_separators);
    RUN_TEST(test_project_compile_rejects_explicit_module_key_path_mismatch);

    return UNITY_END();
}
