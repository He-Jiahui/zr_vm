#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "zr_vm_rust_binding.h"

void setUp(void) {}

void tearDown(void) {}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *text) {
    FILE *file;

    if (path == ZR_NULL || text == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    fwrite(text, 1, strlen(text), file);
    fclose(file);
    return ZR_TRUE;
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

static void build_workspace_root(const TZrChar *baseName, TZrChar *buffer, TZrSize bufferSize) {
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(baseName);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("rust_binding",
                                                       "workspace",
                                                       baseName,
                                                       "",
                                                       buffer,
                                                       bufferSize));
}

static void clean_directory_tree(const TZrChar *path) {
    TZrChar command[ZR_TESTS_PATH_MAX * 2];
    TZrChar shellPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(path);
    snprintf(shellPath, sizeof(shellPath), "%s", path);
#ifdef _MSC_VER
    for (TZrChar *cursor = shellPath; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    snprintf(command,
             sizeof(command),
             "cmd /c if exist \"%s\" rd /s /q \"%s\"",
             shellPath,
             shellPath);
#else
    snprintf(command, sizeof(command), "rm -rf \"%s\"", shellPath);
#endif
    system(command);
}

static void test_rust_binding_scaffold_compile_and_run_round_trip(void) {
    static const TZrChar *projectName = "roundtrip_project";
    static const TZrChar *mainSource = "return \"rust binding roundtrip\";\n";
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar manifestPath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar zriPath[ZR_TESTS_PATH_MAX];
    TZrChar stringBuffer[256];
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingCompileOptions compileOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingCompileResult *compileResult = ZR_NULL;
    ZrRustBindingManifestSnapshot *manifestSnapshot = ZR_NULL;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    TZrSize compiledCount = 0;
    TZrSize skippedCount = 0;
    TZrSize removedCount = 0;
    TZrSize manifestEntryCount = 0;
    TZrSize manifestImportCount = 0;
    TZrSize mainEntryIndex = 0;
    TZrUInt32 manifestVersion = 0;

    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&compileOptions, 0, sizeof(compileOptions));
    memset(&runOptions, 0, sizeof(runOptions));
    memset(stringBuffer, 0, sizeof(stringBuffer));

    build_workspace_root("roundtrip", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(projectPath, sizeof(projectPath), "%s/%s.zrp", workspaceRoot, projectName);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);
    snprintf(manifestPath, sizeof(manifestPath), "%s/bin/.zr_cli_manifest", workspaceRoot);
    snprintf(zroPath, sizeof(zroPath), "%s/bin/main.zro", workspaceRoot);
    snprintf(zriPath, sizeof(zriPath), "%s/bin/main.zri", workspaceRoot);

    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(projectPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainPath));
    TEST_ASSERT_TRUE(write_text_file(mainPath, mainSource));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_Free(workspace));
    workspace = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Open(projectPath, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_GetProjectPath(workspace,
                                                                        stringBuffer,
                                                                        sizeof(stringBuffer)));
    normalize_path_text(stringBuffer);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(stringBuffer));
    TEST_ASSERT_TRUE(text_ends_with(stringBuffer, "roundtrip_project.zrp"));
    memset(stringBuffer, 0, sizeof(stringBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_GetProjectRoot(workspace,
                                                                        stringBuffer,
                                                                        sizeof(stringBuffer)));
    normalize_path_text(stringBuffer);
    TEST_ASSERT_TRUE(text_ends_with(stringBuffer, "/roundtrip"));
    memset(stringBuffer, 0, sizeof(stringBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_GetEntryModule(workspace,
                                                                        stringBuffer,
                                                                        sizeof(stringBuffer)));
    TEST_ASSERT_EQUAL_STRING("main", stringBuffer);
    memset(stringBuffer, 0, sizeof(stringBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_GetManifestPath(workspace,
                                                                         stringBuffer,
                                                                         sizeof(stringBuffer)));
    normalize_path_text(stringBuffer);
    TEST_ASSERT_TRUE(text_ends_with(stringBuffer, "/bin/.zr_cli_manifest"));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);

    compileOptions.emitIntermediate = ZR_TRUE;
    compileOptions.incremental = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &compileResult));
    TEST_ASSERT_NOT_NULL(compileResult);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(zroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(zriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(manifestPath));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_CompileResult_GetCounts(compileResult,
                                                                &compiledCount,
                                                                &skippedCount,
                                                                &removedCount));
    TEST_ASSERT_TRUE((compiledCount + skippedCount) > 0U);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)removedCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_LoadManifest(workspace, &manifestSnapshot));
    TEST_ASSERT_NOT_NULL(manifestSnapshot);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetVersion(manifestSnapshot, &manifestVersion));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)manifestVersion);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryCount(manifestSnapshot, &manifestEntryCount));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (unsigned int)manifestEntryCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_FindEntry(manifestSnapshot, "main", &mainEntryIndex));
    memset(stringBuffer, 0, sizeof(stringBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryModuleName(manifestSnapshot,
                                                                            mainEntryIndex,
                                                                            stringBuffer,
                                                                            sizeof(stringBuffer)));
    TEST_ASSERT_EQUAL_STRING("main", stringBuffer);
    memset(stringBuffer, 0, sizeof(stringBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryZroPath(manifestSnapshot,
                                                                         mainEntryIndex,
                                                                         stringBuffer,
                                                                         sizeof(stringBuffer)));
    normalize_path_text(stringBuffer);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(stringBuffer));
    TEST_ASSERT_TRUE(text_ends_with(stringBuffer, "/bin/main.zro"));
    memset(stringBuffer, 0, sizeof(stringBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryZriPath(manifestSnapshot,
                                                                         mainEntryIndex,
                                                                         stringBuffer,
                                                                         sizeof(stringBuffer)));
    normalize_path_text(stringBuffer);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(stringBuffer));
    TEST_ASSERT_TRUE(text_ends_with(stringBuffer, "/bin/main.zri"));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryImportCount(manifestSnapshot,
                                                                             mainEntryIndex,
                                                                             &manifestImportCount));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)manifestImportCount);

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_INTERP;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_STRING, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadString(result, stringBuffer, sizeof(stringBuffer)));
    TEST_ASSERT_EQUAL_STRING("rust binding roundtrip", stringBuffer);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    memset(stringBuffer, 0, sizeof(stringBuffer));
    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_BINARY;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_STRING, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadString(result, stringBuffer, sizeof(stringBuffer)));
    TEST_ASSERT_EQUAL_STRING("rust binding roundtrip", stringBuffer);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ManifestSnapshot_Free(manifestSnapshot));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_CompileResult_Free(compileResult));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

static void test_rust_binding_open_missing_project_reports_not_found_error_info(void) {
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar missingProjectPath[ZR_TESTS_PATH_MAX];
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingErrorInfo errorInfo;

    memset(&errorInfo, 0, sizeof(errorInfo));
    build_workspace_root("missing_project", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(missingProjectPath, sizeof(missingProjectPath), "%s/missing_project.zrp", workspaceRoot);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                          ZrRustBinding_Project_Open(missingProjectPath, &workspace));
    TEST_ASSERT_NULL(workspace);
    ZrRustBinding_GetLastErrorInfo(&errorInfo);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_NOT_FOUND, errorInfo.status);
    TEST_ASSERT_NOT_NULL(strstr(errorInfo.message, "failed to load project"));
}

static void test_rust_binding_bare_runtime_run_reports_unsupported_error_info(void) {
    static const TZrChar *projectName = "bare_runtime_project";
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    ZrRustBindingErrorInfo errorInfo;

    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&runOptions, 0, sizeof(runOptions));
    memset(&errorInfo, 0, sizeof(errorInfo));

    build_workspace_root("bare_runtime", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);

    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_TRUE(write_text_file(mainPath, "return 99;\n"));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewBare(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_UNSUPPORTED,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NULL(result);
    ZrRustBinding_GetLastErrorInfo(&errorInfo);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_UNSUPPORTED, errorInfo.status);
    TEST_ASSERT_NOT_NULL(strstr(errorInfo.message, "bare runtime execution is not implemented yet"));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

static void test_rust_binding_incremental_toggle_prunes_stale_intermediate_and_keeps_binary_run_stable(void) {
    static const TZrChar *projectName = "incremental_toggle_project";
    static const TZrChar *mainSource =
            "var decorated = %import(\"decorated_user\");\n"
            "\n"
            "return decorated.verifyDecorators() + decorated.decoratedBonus();\n";
    static const TZrChar *decoratedUserSource =
            "%module \"decorated_user\";\n"
            "\n"
            "var decorators = %import(\"decorators\");\n"
            "var markClass = decorators.markClass;\n"
            "var markField = decorators.markField;\n"
            "var markMethod = decorators.markMethod;\n"
            "var markProperty = decorators.markProperty;\n"
            "var markFunction = decorators.markFunction;\n"
            "\n"
            "#markClass#\n"
            "pub class User {\n"
            "    #markField#\n"
            "    pub var id: int = 1;\n"
            "\n"
            "    pri var _value: int = 2;\n"
            "\n"
            "    #markMethod#\n"
            "    pub load(v: int): int {\n"
            "        return v;\n"
            "    }\n"
            "\n"
            "    #markProperty#\n"
            "    pub get value: int {\n"
            "        return this._value;\n"
            "    }\n"
            "}\n"
            "\n"
            "#markFunction#\n"
            "pub decoratedBonus(): int {\n"
            "    var meta = %type(decoratedBonus).metadata;\n"
            "    return meta.instrumented ? 16 : 0;\n"
            "}\n"
            "\n"
            "pub var verifyDecorators = () => {\n"
            "    var seed = 0;\n"
            "    var typeMeta = %type(User).metadata;\n"
            "    var fieldMeta = %type(User).members.id[0].metadata;\n"
            "    var methodMeta = %type(User).members.load[0].metadata;\n"
            "    var propertyMeta = %type(User).members.value[0].metadata;\n"
            "\n"
            "    if (typeMeta.runtimeSerializable) {\n"
            "        seed = seed + 1;\n"
            "    }\n"
            "    if (fieldMeta.isRuntimeField) {\n"
            "        seed = seed + 2;\n"
            "    }\n"
            "    if (methodMeta.isRuntimeMethod) {\n"
            "        seed = seed + 4;\n"
            "    }\n"
            "    if (propertyMeta.isRuntimeProperty) {\n"
            "        seed = seed + 8;\n"
            "    }\n"
            "\n"
            "    return seed;\n"
            "};\n";
    static const TZrChar *decoratorsSource =
            "%module \"decorators\";\n"
            "\n"
            "pub markClass(target: %type Class): void {\n"
            "    target.metadata.runtimeSerializable = true;\n"
            "}\n"
            "\n"
            "pub markFunction(target: %type Function): void {\n"
            "    target.metadata.instrumented = true;\n"
            "}\n"
            "\n"
            "pub markField(target: %type Field): void {\n"
            "    target.metadata.isRuntimeField = true;\n"
            "}\n"
            "\n"
            "pub markMethod(target: %type Method): void {\n"
            "    target.metadata.isRuntimeMethod = true;\n"
            "}\n"
            "\n"
            "pub markProperty(target: %type Property): void {\n"
            "    target.metadata.isRuntimeProperty = true;\n"
            "}\n";
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZroPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZriPath[ZR_TESTS_PATH_MAX];
    TZrChar pathBuffer[ZR_TESTS_PATH_MAX];
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingCompileOptions compileOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingCompileResult *compileResult = ZR_NULL;
    ZrRustBindingManifestSnapshot *manifestSnapshot = ZR_NULL;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    TZrSize compiledCount = 0;
    TZrSize skippedCount = 0;
    TZrSize removedCount = 0;
    TZrSize manifestEntryCount = 0;
    TZrSize mainEntryIndex = 0;
    TZrSize decoratedUserEntryIndex = 0;
    TZrSize decoratorsEntryIndex = 0;
    TZrSize importCount = 0;
    TZrInt64 intValue = 0;

    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&compileOptions, 0, sizeof(compileOptions));
    memset(&runOptions, 0, sizeof(runOptions));
    memset(pathBuffer, 0, sizeof(pathBuffer));

    build_workspace_root("incremental_toggle", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);
    snprintf(decoratedUserPath, sizeof(decoratedUserPath), "%s/src/decorated_user.zr", workspaceRoot);
    snprintf(decoratorsPath, sizeof(decoratorsPath), "%s/src/decorators.zr", workspaceRoot);
    snprintf(mainZroPath, sizeof(mainZroPath), "%s/bin/main.zro", workspaceRoot);
    snprintf(decoratedUserZroPath, sizeof(decoratedUserZroPath), "%s/bin/decorated_user.zro", workspaceRoot);
    snprintf(decoratorsZroPath, sizeof(decoratorsZroPath), "%s/bin/decorators.zro", workspaceRoot);
    snprintf(mainZriPath, sizeof(mainZriPath), "%s/bin/main.zri", workspaceRoot);
    snprintf(decoratedUserZriPath, sizeof(decoratedUserZriPath), "%s/bin/decorated_user.zri", workspaceRoot);
    snprintf(decoratorsZriPath, sizeof(decoratorsZriPath), "%s/bin/decorators.zri", workspaceRoot);

    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_TRUE(write_text_file(mainPath, mainSource));
    TEST_ASSERT_TRUE(write_text_file(decoratedUserPath, decoratedUserSource));
    TEST_ASSERT_TRUE(write_text_file(decoratorsPath, decoratorsSource));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);

    compileOptions.emitIntermediate = ZR_TRUE;
    compileOptions.incremental = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &compileResult));
    TEST_ASSERT_NOT_NULL(compileResult);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_CompileResult_GetCounts(compileResult,
                                                                &compiledCount,
                                                                &skippedCount,
                                                                &removedCount));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)removedCount);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZriPath));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_LoadManifest(workspace, &manifestSnapshot));
    TEST_ASSERT_NOT_NULL(manifestSnapshot);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryCount(manifestSnapshot, &manifestEntryCount));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)manifestEntryCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_FindEntry(manifestSnapshot, "main", &mainEntryIndex));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_FindEntry(manifestSnapshot,
                                                                   "decorated_user",
                                                                   &decoratedUserEntryIndex));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_FindEntry(manifestSnapshot,
                                                                   "decorators",
                                                                   &decoratorsEntryIndex));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryImportCount(manifestSnapshot,
                                                                             mainEntryIndex,
                                                                             &importCount));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)importCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryImportCount(manifestSnapshot,
                                                                             decoratedUserEntryIndex,
                                                                             &importCount));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)importCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryImportCount(manifestSnapshot,
                                                                             decoratorsEntryIndex,
                                                                             &importCount));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)importCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ManifestSnapshot_Free(manifestSnapshot));
    manifestSnapshot = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_CompileResult_Free(compileResult));
    compileResult = ZR_NULL;

    compileOptions.emitIntermediate = ZR_FALSE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &compileResult));
    TEST_ASSERT_NOT_NULL(compileResult);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_CompileResult_GetCounts(compileResult,
                                                                &compiledCount,
                                                                &skippedCount,
                                                                &removedCount));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)compiledCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)removedCount);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZroPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratorsZriPath));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_LoadManifest(workspace, &manifestSnapshot));
    TEST_ASSERT_NOT_NULL(manifestSnapshot);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryCount(manifestSnapshot, &manifestEntryCount));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)manifestEntryCount);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryZriPath(manifestSnapshot,
                                                                          mainEntryIndex,
                                                                          pathBuffer,
                                                                          sizeof(pathBuffer)));
    normalize_path_text(pathBuffer);
    if (pathBuffer[0] != '\0') {
        TEST_ASSERT_FALSE(ZrTests_File_Exists(pathBuffer));
    }
    memset(pathBuffer, 0, sizeof(pathBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryZriPath(manifestSnapshot,
                                                                          decoratedUserEntryIndex,
                                                                          pathBuffer,
                                                                          sizeof(pathBuffer)));
    normalize_path_text(pathBuffer);
    if (pathBuffer[0] != '\0') {
        TEST_ASSERT_FALSE(ZrTests_File_Exists(pathBuffer));
    }
    memset(pathBuffer, 0, sizeof(pathBuffer));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ManifestSnapshot_GetEntryZriPath(manifestSnapshot,
                                                                          decoratorsEntryIndex,
                                                                          pathBuffer,
                                                                          sizeof(pathBuffer)));
    normalize_path_text(pathBuffer);
    if (pathBuffer[0] != '\0') {
        TEST_ASSERT_FALSE(ZrTests_File_Exists(pathBuffer));
    }
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ManifestSnapshot_Free(manifestSnapshot));
    manifestSnapshot = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_CompileResult_Free(compileResult));
    compileResult = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_BINARY;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(31, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &compileResult));
    TEST_ASSERT_NOT_NULL(compileResult);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_CompileResult_GetCounts(compileResult,
                                                                &compiledCount,
                                                                &skippedCount,
                                                                &removedCount));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)compiledCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)removedCount);
    TEST_ASSERT_FALSE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratorsZriPath));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_CompileResult_Free(compileResult));
    compileResult = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(31, intValue);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

static void test_rust_binding_run_named_module_preserves_module_name_and_program_args(void) {
    static const TZrChar *projectName = "module_run_project";
    static const TZrChar *mainSource = "return 17;\n";
    static const TZrChar *moduleSource =
            "var system = %import(\"zr.system\");\n"
            "\n"
            "fingerprint(): int {\n"
            "    var count = 0;\n"
            "    var score = 0;\n"
            "    for (var item in system.process.arguments) {\n"
            "        if (count == 0 && item == \"tools.seed\") {\n"
            "            score = score + 100;\n"
            "        } else if (count == 1 && item == \"foo\") {\n"
            "            score = score + 10;\n"
            "        } else if (count == 2 && item == \"bar\") {\n"
            "            score = score + 1;\n"
            "        }\n"
            "        count = count + 1;\n"
            "    }\n"
            "    return count * 1000 + score;\n"
            "}\n"
            "\n"
            "return fingerprint();\n";
    static const TZrChar *programArgs[] = {"foo", "bar"};
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar zriPath[ZR_TESTS_PATH_MAX];
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    TZrInt64 intValue = 0;

    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&runOptions, 0, sizeof(runOptions));

    build_workspace_root("module_run", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);
    snprintf(modulePath, sizeof(modulePath), "%s/src/tools/seed.zr", workspaceRoot);

    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_TRUE(write_text_file(mainPath, mainSource));
    TEST_ASSERT_TRUE(write_text_file(modulePath, moduleSource));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectWorkspace_ResolveArtifacts(workspace,
                                                                          "tools.seed",
                                                                          zroPath,
                                                                          sizeof(zroPath),
                                                                          zriPath,
                                                                          sizeof(zriPath)));
    normalize_path_text(zroPath);
    normalize_path_text(zriPath);
    TEST_ASSERT_TRUE(text_ends_with(zroPath, "/bin/tools/seed.zro"));
    TEST_ASSERT_TRUE(text_ends_with(zriPath, "/bin/tools/seed.zri"));

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_INTERP;
    runOptions.moduleName = "tools.seed";
    runOptions.programArgs = programArgs;
    runOptions.programArgCount = 2;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(3111, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_BINARY;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(3111, intValue);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

static void test_rust_binding_owned_value_array_and_object_accessors(void) {
    ZrRustBindingValue *arrayValue = ZR_NULL;
    ZrRustBindingValue *objectValue = ZR_NULL;
    ZrRustBindingValue *firstInt = ZR_NULL;
    ZrRustBindingValue *secondInt = ZR_NULL;
    ZrRustBindingValue *fieldString = ZR_NULL;
    ZrRustBindingValue *fetchedElement = ZR_NULL;
    ZrRustBindingValue *fetchedField = ZR_NULL;
    TZrSize arrayLength = 0;
    TZrInt64 intValue = 0;
    TZrChar buffer[128];

    memset(buffer, 0, sizeof(buffer));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_NewArray(&arrayValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_NewInt(7, &firstInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_NewInt(11, &secondInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_ARRAY, ZrRustBinding_Value_GetKind(arrayValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(arrayValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(firstInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(firstInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(secondInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(secondInt));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Array_Push(arrayValue, firstInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Array_Push(arrayValue, secondInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_Array_Length(arrayValue, &arrayLength));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)arrayLength);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_Array_Get(arrayValue, 1, &fetchedElement));
    TEST_ASSERT_NOT_NULL(fetchedElement);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_INT, ZrRustBinding_Value_GetKind(fetchedElement));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(fetchedElement));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadInt(fetchedElement, &intValue));
    TEST_ASSERT_EQUAL_INT64(11, intValue);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_NewObject(&objectValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_NewString("binding-object", &fieldString));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_OBJECT, ZrRustBinding_Value_GetKind(objectValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(objectValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_STRING, ZrRustBinding_Value_GetKind(fieldString));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(fieldString));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_Object_Set(objectValue, "label", fieldString));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_Object_Get(objectValue, "label", &fetchedField));
    TEST_ASSERT_NOT_NULL(fetchedField);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_STRING, ZrRustBinding_Value_GetKind(fetchedField));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(fetchedField));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadString(fetchedField, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("binding-object", buffer);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(fetchedField));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(fieldString));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(objectValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(fetchedElement));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(secondInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(firstInt));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(arrayValue));
}

static void test_rust_binding_scalar_value_kind_and_ownership_metadata(void) {
    ZrRustBindingValue *nullValue = ZR_NULL;
    ZrRustBindingValue *boolValueHandle = ZR_NULL;
    ZrRustBindingValue *floatValueHandle = ZR_NULL;
    ZrRustBindingValue *stringValue = ZR_NULL;
    TZrBool boolValue = ZR_FALSE;
    TZrFloat64 floatValue = 0.0;
    TZrChar buffer[128];

    memset(buffer, 0, sizeof(buffer));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_NewNull(&nullValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_NewBool(ZR_TRUE, &boolValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_NewFloat(3.5, &floatValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_NewString("owned-metadata", &stringValue));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_NULL, ZrRustBinding_Value_GetKind(nullValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(nullValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_BOOL, ZrRustBinding_Value_GetKind(boolValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(boolValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadBool(boolValueHandle, &boolValue));
    TEST_ASSERT_TRUE(boolValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_FLOAT, ZrRustBinding_Value_GetKind(floatValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(floatValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadFloat(floatValueHandle, &floatValue));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.5, floatValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_STRING, ZrRustBinding_Value_GetKind(stringValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(stringValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadString(stringValue, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("owned-metadata", buffer);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(stringValue));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(floatValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(boolValueHandle));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(nullValue));
}

static void test_rust_binding_call_module_export_with_owned_arguments(void) {
    static const TZrChar *projectName = "call_export_project";
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingValue *argument = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    TZrFloat64 floatValue = 0.0;

    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&runOptions, 0, sizeof(runOptions));

    build_workspace_root("call_export", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_NewFloat(9.0, &argument));
    TEST_ASSERT_NOT_NULL(argument);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_CallModuleExport(runtime,
                                                                 workspace,
                                                                 &runOptions,
                                                                 "zr.math",
                                                                 "sqrt",
                                                                 &argument,
                                                                 1,
                                                                 &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_VALUE_KIND_FLOAT, ZrRustBinding_Value_GetKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_OWNERSHIP_KIND_NONE,
                          ZrRustBinding_Value_GetOwnershipKind(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadFloat(result, &floatValue));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, floatValue);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(argument));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

typedef struct NativeCallbackCapture {
    TZrSize callCount;
    TZrSize destroyCount;
    TZrSize argumentCount;
    TZrInt64 firstValue;
    TZrInt64 secondValue;
    ZrRustBindingStatus callbackStatus;
    TZrUInt32 failureStep;
    TZrChar moduleName[64];
    TZrChar typeName[64];
    TZrChar callableName[64];
} NativeCallbackCapture;

static void native_callback_capture_reset(NativeCallbackCapture *capture) {
    TEST_ASSERT_NOT_NULL(capture);
    memset(capture, 0, sizeof(*capture));
    capture->callbackStatus = ZR_RUST_BINDING_STATUS_OK;
}

static void native_callback_capture_destroy(TZrPtr userData) {
    NativeCallbackCapture *capture = (NativeCallbackCapture *)userData;

    if (capture != ZR_NULL) {
        capture->destroyCount++;
    }
}

static ZrRustBindingStatus native_callback_capture_fail(NativeCallbackCapture *capture,
                                                        TZrUInt32 failureStep,
                                                        ZrRustBindingStatus status) {
    if (capture != ZR_NULL && capture->callbackStatus == ZR_RUST_BINDING_STATUS_OK) {
        capture->callbackStatus = status;
        capture->failureStep = failureStep;
    }
    return status;
}

static ZrRustBindingStatus native_host_sum_callback(ZrRustBindingNativeCallContext *context,
                                                    TZrPtr userData,
                                                    ZrRustBindingValue **outResult) {
    NativeCallbackCapture *capture = (NativeCallbackCapture *)userData;
    ZrRustBindingValue *first = ZR_NULL;
    ZrRustBindingValue *second = ZR_NULL;
    ZrRustBindingStatus status;

    if (capture == ZR_NULL || outResult == ZR_NULL) {
        return ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT;
    }
    capture->callCount++;
    status = ZrRustBinding_NativeCallContext_GetModuleName(context,
                                                           capture->moduleName,
                                                           sizeof(capture->moduleName));
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 1, status);
    }
    status = ZrRustBinding_NativeCallContext_GetCallableName(context,
                                                             capture->callableName,
                                                             sizeof(capture->callableName));
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 2, status);
    }
    status = ZrRustBinding_NativeCallContext_GetArgumentCount(context, &capture->argumentCount);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 3, status);
    }
    status = ZrRustBinding_NativeCallContext_CheckArity(context, 2, 2);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 4, status);
    }
    status = ZrRustBinding_NativeCallContext_GetTypeName(context,
                                                         capture->typeName,
                                                         sizeof(capture->typeName));
    if (status != ZR_RUST_BINDING_STATUS_NOT_FOUND) {
        return native_callback_capture_fail(capture, 5, status);
    }
    capture->typeName[0] = '\0';
    status = ZrRustBinding_NativeCallContext_GetArgument(context, 0, &first);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 6, status);
    }
    status = ZrRustBinding_NativeCallContext_GetArgument(context, 1, &second);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 7, status);
    }
    status = ZrRustBinding_Value_ReadInt(first, &capture->firstValue);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(second);
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 8, status);
    }
    status = ZrRustBinding_Value_ReadInt(second, &capture->secondValue);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(second);
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 9, status);
    }
    status = ZrRustBinding_Value_NewInt(capture->firstValue + capture->secondValue, outResult);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(second);
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 10, status);
    }
    ZrRustBinding_Value_Free(second);
    ZrRustBinding_Value_Free(first);
    return ZR_RUST_BINDING_STATUS_OK;
}

static ZrRustBindingStatus native_host_mul_callback(ZrRustBindingNativeCallContext *context,
                                                    TZrPtr userData,
                                                    ZrRustBindingValue **outResult) {
    NativeCallbackCapture *capture = (NativeCallbackCapture *)userData;
    ZrRustBindingValue *first = ZR_NULL;
    ZrRustBindingValue *second = ZR_NULL;
    ZrRustBindingStatus status;

    if (capture == ZR_NULL || outResult == ZR_NULL) {
        return ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT;
    }
    capture->callCount++;
    status = ZrRustBinding_NativeCallContext_GetModuleName(context,
                                                           capture->moduleName,
                                                           sizeof(capture->moduleName));
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 101, status);
    }
    status = ZrRustBinding_NativeCallContext_GetTypeName(context,
                                                         capture->typeName,
                                                         sizeof(capture->typeName));
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 102, status);
    }
    status = ZrRustBinding_NativeCallContext_GetCallableName(context,
                                                             capture->callableName,
                                                             sizeof(capture->callableName));
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 103, status);
    }
    status = ZrRustBinding_NativeCallContext_GetArgumentCount(context, &capture->argumentCount);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 104, status);
    }
    status = ZrRustBinding_NativeCallContext_CheckArity(context, 2, 2);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 105, status);
    }
    status = ZrRustBinding_NativeCallContext_GetArgument(context, 0, &first);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        return native_callback_capture_fail(capture, 106, status);
    }
    status = ZrRustBinding_NativeCallContext_GetArgument(context, 1, &second);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 107, status);
    }
    status = ZrRustBinding_Value_ReadInt(first, &capture->firstValue);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(second);
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 108, status);
    }
    status = ZrRustBinding_Value_ReadInt(second, &capture->secondValue);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(second);
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 109, status);
    }
    status = ZrRustBinding_Value_NewInt(capture->firstValue * capture->secondValue, outResult);
    if (status != ZR_RUST_BINDING_STATUS_OK) {
        ZrRustBinding_Value_Free(second);
        ZrRustBinding_Value_Free(first);
        return native_callback_capture_fail(capture, 110, status);
    }
    ZrRustBinding_Value_Free(second);
    ZrRustBinding_Value_Free(first);
    return ZR_RUST_BINDING_STATUS_OK;
}

static void test_rust_binding_native_module_registration_roundtrip(void) {
    static const TZrChar *projectName = "native_module_project";
    static const TZrChar *mainSource =
            "var host = %import(\"host_demo\");\n"
            "return host.answer + host.bump(2, 3) + host.Counter.mul(4, 5);\n";
    static const TZrChar *parameterTypeNames[] = {"int", "int"};
    static const TZrChar *parameterNames[] = {"left", "right"};
    static const TZrChar *parameterDocs[] = {"left operand", "right operand"};
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    NativeCallbackCapture functionCapture;
    NativeCallbackCapture methodCapture;
    ZrRustBindingNativeParameterDescriptor parameters[2];
    ZrRustBindingNativeFunctionDescriptor functionDescriptor;
    ZrRustBindingNativeMethodDescriptor methodDescriptor;
    ZrRustBindingNativeTypeDescriptor typeDescriptor;
    ZrRustBindingNativeConstantDescriptor constantDescriptor;
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingCompileOptions compileOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingNativeModuleBuilder *builder = ZR_NULL;
    ZrRustBindingNativeModule *module = ZR_NULL;
    ZrRustBindingRuntimeNativeModuleRegistration *registration = ZR_NULL;
    ZrRustBindingCompileResult *compileResult = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    ZrRustBindingStatus runStatus;
    TZrInt64 intValue = 0;

    memset(parameters, 0, sizeof(parameters));
    memset(&functionDescriptor, 0, sizeof(functionDescriptor));
    memset(&methodDescriptor, 0, sizeof(methodDescriptor));
    memset(&typeDescriptor, 0, sizeof(typeDescriptor));
    memset(&constantDescriptor, 0, sizeof(constantDescriptor));
    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&compileOptions, 0, sizeof(compileOptions));
    memset(&runOptions, 0, sizeof(runOptions));
    native_callback_capture_reset(&functionCapture);
    native_callback_capture_reset(&methodCapture);

    parameters[0].name = parameterNames[0];
    parameters[0].typeName = parameterTypeNames[0];
    parameters[0].documentation = parameterDocs[0];
    parameters[1].name = parameterNames[1];
    parameters[1].typeName = parameterTypeNames[1];
    parameters[1].documentation = parameterDocs[1];

    functionDescriptor.name = "bump";
    functionDescriptor.minArgumentCount = 2;
    functionDescriptor.maxArgumentCount = 2;
    functionDescriptor.callback = native_host_sum_callback;
    functionDescriptor.userData = &functionCapture;
    functionDescriptor.destroyUserData = native_callback_capture_destroy;
    functionDescriptor.returnTypeName = "int";
    functionDescriptor.documentation = "Adds two values.";
    functionDescriptor.parameters = parameters;
    functionDescriptor.parameterCount = 2;

    methodDescriptor.name = "mul";
    methodDescriptor.minArgumentCount = 2;
    methodDescriptor.maxArgumentCount = 2;
    methodDescriptor.callback = native_host_mul_callback;
    methodDescriptor.userData = &methodCapture;
    methodDescriptor.destroyUserData = native_callback_capture_destroy;
    methodDescriptor.returnTypeName = "int";
    methodDescriptor.documentation = "Multiplies two values.";
    methodDescriptor.isStatic = ZR_TRUE;
    methodDescriptor.parameters = parameters;
    methodDescriptor.parameterCount = 2;

    typeDescriptor.name = "Counter";
    typeDescriptor.prototypeType = ZR_RUST_BINDING_PROTOTYPE_TYPE_CLASS;
    typeDescriptor.methods = &methodDescriptor;
    typeDescriptor.methodCount = 1;
    typeDescriptor.documentation = "Host-side native counter helpers.";

    constantDescriptor.name = "answer";
    constantDescriptor.kind = ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT;
    constantDescriptor.intValue = 100;
    constantDescriptor.documentation = "Answer constant.";
    constantDescriptor.typeName = "int";

    build_workspace_root("native_module", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);

    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_TRUE(write_text_file(mainPath, mainSource));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_New("host_demo", &builder));
    TEST_ASSERT_NOT_NULL(builder);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetDocumentation(builder, "Host demo module."));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetModuleVersion(builder, "1.0.0"));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddConstant(builder, &constantDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddFunction(builder, &functionDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddType(builder, &typeDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Build(builder, &module));
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Free(builder));
    builder = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_RegisterNativeModule(runtime, module, &registration));
    TEST_ASSERT_NOT_NULL(registration);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &compileResult));
    TEST_ASSERT_NOT_NULL(compileResult);

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_INTERP;
    runStatus = ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result);
    if (runStatus != ZR_RUST_BINDING_STATUS_OK) {
        TZrChar message[256];
        snprintf(message,
                 sizeof(message),
                 "native interp run failed status=%d function_status=%d function_step=%u method_status=%d method_step=%u",
                 (int)runStatus,
                 (int)functionCapture.callbackStatus,
                 (unsigned int)functionCapture.failureStep,
                 (int)methodCapture.callbackStatus,
                 (unsigned int)methodCapture.failureStep);
        TEST_FAIL_MESSAGE(message);
    }
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(125, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_BINARY;
    runStatus = ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result);
    if (runStatus != ZR_RUST_BINDING_STATUS_OK) {
        TZrChar message[256];
        snprintf(message,
                 sizeof(message),
                 "native run failed status=%d function_status=%d function_step=%u method_status=%d method_step=%u",
                 (int)runStatus,
                 (int)functionCapture.callbackStatus,
                 (unsigned int)functionCapture.failureStep,
                 (int)methodCapture.callbackStatus,
                 (unsigned int)methodCapture.failureStep);
        TEST_FAIL_MESSAGE(message);
    }
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(125, intValue);

    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)functionCapture.callCount);
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)functionCapture.argumentCount);
    TEST_ASSERT_EQUAL_INT64(2, functionCapture.firstValue);
    TEST_ASSERT_EQUAL_INT64(3, functionCapture.secondValue);
    TEST_ASSERT_EQUAL_STRING("host_demo", functionCapture.moduleName);
    TEST_ASSERT_EQUAL_STRING("bump", functionCapture.callableName);
    TEST_ASSERT_EQUAL_STRING("", functionCapture.typeName);
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)methodCapture.callCount);
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)methodCapture.argumentCount);
    TEST_ASSERT_EQUAL_INT64(4, methodCapture.firstValue);
    TEST_ASSERT_EQUAL_INT64(5, methodCapture.secondValue);
    TEST_ASSERT_EQUAL_STRING("host_demo", methodCapture.moduleName);
    TEST_ASSERT_EQUAL_STRING("mul", methodCapture.callableName);
    TEST_ASSERT_EQUAL_STRING("Counter", methodCapture.typeName);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_CompileResult_Free(compileResult));
    compileResult = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_NativeModule_Free(module));
    module = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_RuntimeNativeModuleRegistration_Free(registration));
    registration = ZR_NULL;

    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)functionCapture.destroyCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)methodCapture.destroyCount);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

static void test_rust_binding_native_module_registration_release_allows_re_registration(void) {
    static const TZrChar *projectName = "native_module_reregister_project";
    static const TZrChar *mainSource =
            "var host = %import(\"host_demo\");\n"
            "return host.answer;\n";
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    ZrRustBindingNativeConstantDescriptor constantDescriptor;
    ZrRustBindingScaffoldOptions scaffoldOptions;
    ZrRustBindingRuntimeOptions runtimeOptions;
    ZrRustBindingCompileOptions compileOptions;
    ZrRustBindingRunOptions runOptions;
    ZrRustBindingErrorInfo errorInfo;
    ZrRustBindingProjectWorkspace *workspace = ZR_NULL;
    ZrRustBindingRuntime *runtime = ZR_NULL;
    ZrRustBindingNativeModuleBuilder *builder = ZR_NULL;
    ZrRustBindingNativeModule *module = ZR_NULL;
    ZrRustBindingRuntimeNativeModuleRegistration *registration = ZR_NULL;
    ZrRustBindingRuntimeNativeModuleRegistration *duplicateRegistration = ZR_NULL;
    ZrRustBindingCompileResult *compileResult = ZR_NULL;
    ZrRustBindingValue *result = ZR_NULL;
    TZrInt64 intValue = 0;

    memset(&constantDescriptor, 0, sizeof(constantDescriptor));
    memset(&scaffoldOptions, 0, sizeof(scaffoldOptions));
    memset(&runtimeOptions, 0, sizeof(runtimeOptions));
    memset(&compileOptions, 0, sizeof(compileOptions));
    memset(&runOptions, 0, sizeof(runOptions));
    memset(&errorInfo, 0, sizeof(errorInfo));

    constantDescriptor.name = "answer";
    constantDescriptor.kind = ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT;
    constantDescriptor.documentation = "Answer constant.";
    constantDescriptor.typeName = "int";

    build_workspace_root("native_module_reregister", workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);

    scaffoldOptions.rootPath = workspaceRoot;
    scaffoldOptions.projectName = projectName;
    scaffoldOptions.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffoldOptions, &workspace));
    TEST_ASSERT_NOT_NULL(workspace);
    TEST_ASSERT_TRUE(write_text_file(mainPath, mainSource));

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &runtime));
    TEST_ASSERT_NOT_NULL(runtime);

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_New("host_demo", &builder));
    TEST_ASSERT_NOT_NULL(builder);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetDocumentation(builder, "Host demo module."));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetModuleVersion(builder, "1.0.0"));
    constantDescriptor.intValue = 100;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddConstant(builder, &constantDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Build(builder, &module));
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Free(builder));
    builder = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_RegisterNativeModule(runtime, module, &registration));
    TEST_ASSERT_NOT_NULL(registration);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_NativeModule_Free(module));
    module = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_New("host_demo", &builder));
    TEST_ASSERT_NOT_NULL(builder);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetDocumentation(builder, "Host demo module."));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetModuleVersion(builder, "1.0.0"));
    constantDescriptor.intValue = 200;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddConstant(builder, &constantDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Build(builder, &module));
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Free(builder));
    builder = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_ALREADY_EXISTS,
                          ZrRustBinding_Runtime_RegisterNativeModule(runtime, module, &duplicateRegistration));
    TEST_ASSERT_NULL(duplicateRegistration);
    ZrRustBinding_GetLastErrorInfo(&errorInfo);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_ALREADY_EXISTS, errorInfo.status);
    TEST_ASSERT_NOT_NULL(strstr(errorInfo.message, "native module already registered on runtime"));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_NativeModule_Free(module));
    module = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &compileResult));
    TEST_ASSERT_NOT_NULL(compileResult);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_CompileResult_Free(compileResult));
    compileResult = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_INTERP;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(100, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_BINARY;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(100, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_RuntimeNativeModuleRegistration_Free(registration));
    registration = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_New("host_demo", &builder));
    TEST_ASSERT_NOT_NULL(builder);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetDocumentation(builder, "Host demo module."));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_SetModuleVersion(builder, "1.0.0"));
    constantDescriptor.intValue = 250;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddConstant(builder, &constantDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Build(builder, &module));
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Free(builder));
    builder = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_RegisterNativeModule(runtime, module, &registration));
    TEST_ASSERT_NOT_NULL(registration);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_NativeModule_Free(module));
    module = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_INTERP;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(250, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    runOptions.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_BINARY;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &result));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &intValue));
    TEST_ASSERT_EQUAL_INT64(250, intValue);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    result = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_RuntimeNativeModuleRegistration_Free(registration));
    registration = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(workspace));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(runtime));
}

static void test_rust_binding_native_builder_rejects_invalid_function_descriptor(void) {
    NativeCallbackCapture capture;
    ZrRustBindingNativeFunctionDescriptor functionDescriptor;
    ZrRustBindingNativeModuleBuilder *builder = ZR_NULL;

    memset(&functionDescriptor, 0, sizeof(functionDescriptor));
    native_callback_capture_reset(&capture);

    functionDescriptor.name = "bad";
    functionDescriptor.minArgumentCount = 2;
    functionDescriptor.maxArgumentCount = 1;
    functionDescriptor.callback = native_host_sum_callback;
    functionDescriptor.userData = &capture;
    functionDescriptor.destroyUserData = native_callback_capture_destroy;
    functionDescriptor.returnTypeName = "int";

    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_New("invalid_demo", &builder));
    TEST_ASSERT_NOT_NULL(builder);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                          ZrRustBinding_NativeModuleBuilder_AddFunction(builder, &functionDescriptor));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Free(builder));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)capture.destroyCount);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_rust_binding_scaffold_compile_and_run_round_trip);
    RUN_TEST(test_rust_binding_open_missing_project_reports_not_found_error_info);
    RUN_TEST(test_rust_binding_bare_runtime_run_reports_unsupported_error_info);
    RUN_TEST(test_rust_binding_incremental_toggle_prunes_stale_intermediate_and_keeps_binary_run_stable);
    RUN_TEST(test_rust_binding_run_named_module_preserves_module_name_and_program_args);
    RUN_TEST(test_rust_binding_owned_value_array_and_object_accessors);
    RUN_TEST(test_rust_binding_scalar_value_kind_and_ownership_metadata);
    RUN_TEST(test_rust_binding_call_module_export_with_owned_arguments);
    RUN_TEST(test_rust_binding_native_module_registration_roundtrip);
    RUN_TEST(test_rust_binding_native_module_registration_release_allows_re_registration);
    RUN_TEST(test_rust_binding_native_builder_rejects_invalid_function_descriptor);

    return UNITY_END();
}
