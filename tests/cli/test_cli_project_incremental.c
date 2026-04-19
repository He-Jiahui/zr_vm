#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command/command.h"
#include "compiler/compiler.h"
#include "harness/path_support.h"
#include "project/project.h"
#include "runtime/runtime.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/common_state.h"

void setUp(void) {}

void tearDown(void) {}

static TZrBool copy_file_bytes(const TZrChar *sourcePath, const TZrChar *destinationPath) {
    FILE *sourceFile;
    FILE *destinationFile;
    TZrByte buffer[4096];
    size_t readCount;

    if (sourcePath == ZR_NULL || destinationPath == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(destinationPath)) {
        return ZR_FALSE;
    }

    sourceFile = fopen(sourcePath, "rb");
    if (sourceFile == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationFile = fopen(destinationPath, "wb");
    if (destinationFile == ZR_NULL) {
        fclose(sourceFile);
        return ZR_FALSE;
    }

    while ((readCount = fread(buffer, 1, sizeof(buffer), sourceFile)) > 0) {
        if (fwrite(buffer, 1, readCount, destinationFile) != readCount) {
            fclose(destinationFile);
            fclose(sourceFile);
            return ZR_FALSE;
        }
    }

    fclose(destinationFile);
    fclose(sourceFile);
    return ZR_TRUE;
}

static TZrBool write_text_file(const TZrChar *destinationPath, const TZrChar *contents) {
    FILE *destinationFile;
    size_t contentLength;

    if (destinationPath == ZR_NULL || contents == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(destinationPath)) {
        return ZR_FALSE;
    }

    destinationFile = fopen(destinationPath, "wb");
    if (destinationFile == ZR_NULL) {
        return ZR_FALSE;
    }

    contentLength = strlen(contents);
    if (contentLength > 0 && fwrite(contents, 1, contentLength, destinationFile) != contentLength) {
        fclose(destinationFile);
        return ZR_FALSE;
    }

    fclose(destinationFile);
    return ZR_TRUE;
}

static TZrBool join_path_suffix(const TZrChar *basePath,
                                const TZrChar *suffix,
                                TZrChar *buffer,
                                TZrSize bufferSize) {
    size_t baseLength;
    size_t suffixLength;

    if (basePath == ZR_NULL || suffix == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    baseLength = strlen(basePath);
    suffixLength = strlen(suffix);
    if (baseLength + suffixLength + 1u > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, basePath, baseLength);
    memcpy(buffer + baseLength, suffix, suffixLength + 1u);
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

static TZrBool rewrite_text_file_replacing_once(const TZrChar *path,
                                                const TZrChar *needle,
                                                const TZrChar *replacement) {
    TZrChar *source = ZR_NULL;
    TZrSize sourceLength = 0;
    const TZrChar *match;
    size_t prefixLength;
    size_t needleLength;
    size_t replacementLength;
    size_t resultLength;
    TZrChar *result;
    TZrBool success;

    if (path == ZR_NULL || needle == ZR_NULL || replacement == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCli_Project_ReadTextFile(path, &source, &sourceLength)) {
        return ZR_FALSE;
    }

    match = strstr(source, needle);
    if (match == ZR_NULL) {
        free(source);
        return ZR_FALSE;
    }

    prefixLength = (size_t)(match - source);
    needleLength = strlen(needle);
    replacementLength = strlen(replacement);
    resultLength = sourceLength - needleLength + replacementLength;
    result = (TZrChar *)malloc(resultLength + 1u);
    if (result == ZR_NULL) {
        free(source);
        return ZR_FALSE;
    }

    memcpy(result, source, prefixLength);
    memcpy(result + prefixLength, replacement, replacementLength);
    memcpy(result + prefixLength + replacementLength,
           match + needleLength,
           sourceLength - prefixLength - needleLength + 1u);

    success = write_text_file(path, result);
    free(result);
    free(source);
    return success;
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

static void build_generated_project_root(const TZrChar *baseName, TZrChar *buffer, TZrSize bufferSize) {
    TEST_ASSERT_NOT_NULL(baseName);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli",
                                                       "project_incremental",
                                                       baseName,
                                                       "",
                                                       buffer,
                                                       bufferSize));
}

static TZrBool prepare_decorator_import_fixture_named(const TZrChar *baseName,
                                                      TZrChar *projectRoot,
                                                      TZrSize projectRootSize,
                                                      TZrChar *projectPath,
                                                      TZrSize projectPathSize) {
    TZrChar fixtureProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureMainPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureDecoratedUserPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureDecoratorsPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationMainPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationDecoratedUserPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationDecoratorsPath[ZR_TESTS_PATH_MAX];

    if (baseName == ZR_NULL || projectRoot == ZR_NULL || projectPath == ZR_NULL) {
        return ZR_FALSE;
    }

    build_generated_project_root(baseName, projectRoot, projectRootSize);
    clean_directory_tree(projectRoot);

    if (!ZrTests_Path_GetProjectFile("decorator_import",
                                     "decorator_import.zrp",
                                     fixtureProjectPath,
                                     sizeof(fixtureProjectPath)) ||
        !ZrTests_Path_GetProjectFile("decorator_import", "src/main.zr", fixtureMainPath, sizeof(fixtureMainPath)) ||
        !ZrTests_Path_GetProjectFile("decorator_import",
                                     "src/decorated_user.zr",
                                     fixtureDecoratedUserPath,
                                     sizeof(fixtureDecoratedUserPath)) ||
        !ZrTests_Path_GetProjectFile("decorator_import",
                                     "src/decorators.zr",
                                     fixtureDecoratorsPath,
                                     sizeof(fixtureDecoratorsPath))) {
        return ZR_FALSE;
    }

    snprintf(destinationProjectPath, sizeof(destinationProjectPath), "%s/decorator_import.zrp", projectRoot);
    snprintf(destinationMainPath, sizeof(destinationMainPath), "%s/src/main.zr", projectRoot);
    snprintf(destinationDecoratedUserPath,
             sizeof(destinationDecoratedUserPath),
             "%s/src/decorated_user.zr",
             projectRoot);
    snprintf(destinationDecoratorsPath, sizeof(destinationDecoratorsPath), "%s/src/decorators.zr", projectRoot);

    if (!copy_file_bytes(fixtureProjectPath, destinationProjectPath) ||
        !copy_file_bytes(fixtureMainPath, destinationMainPath) ||
        !copy_file_bytes(fixtureDecoratedUserPath, destinationDecoratedUserPath) ||
        !copy_file_bytes(fixtureDecoratorsPath, destinationDecoratorsPath)) {
        return ZR_FALSE;
    }

    snprintf(projectPath, projectPathSize, "%s", destinationProjectPath);
    return ZR_TRUE;
}

static TZrBool prepare_decorator_import_fixture(TZrChar *projectRoot,
                                                TZrSize projectRootSize,
                                                TZrChar *projectPath,
                                                TZrSize projectPathSize) {
    return prepare_decorator_import_fixture_named("decorator_import",
                                                  projectRoot,
                                                  projectRootSize,
                                                  projectPath,
                                                  projectPathSize);
}

static TZrBool prepare_cli_args_fixture(TZrChar *projectRoot,
                                        TZrSize projectRootSize,
                                        TZrChar *projectPath,
                                        TZrSize projectPathSize) {
    TZrChar fixtureProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureMainPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureModulePath[ZR_TESTS_PATH_MAX];
    TZrChar destinationProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationMainPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationModulePath[ZR_TESTS_PATH_MAX];

    if (projectRoot == ZR_NULL || projectPath == ZR_NULL) {
        return ZR_FALSE;
    }

    build_generated_project_root("cli_args", projectRoot, projectRootSize);
    clean_directory_tree(projectRoot);

    if (!ZrTests_Path_GetProjectFile("cli_args", "cli_args.zrp", fixtureProjectPath, sizeof(fixtureProjectPath)) ||
        !ZrTests_Path_GetProjectFile("cli_args", "src/main.zr", fixtureMainPath, sizeof(fixtureMainPath)) ||
        !ZrTests_Path_GetProjectFile("cli_args",
                                     "src/tools/seed.zr",
                                     fixtureModulePath,
                                     sizeof(fixtureModulePath))) {
        return ZR_FALSE;
    }

    snprintf(destinationProjectPath, sizeof(destinationProjectPath), "%s/cli_args.zrp", projectRoot);
    snprintf(destinationMainPath, sizeof(destinationMainPath), "%s/src/main.zr", projectRoot);
    snprintf(destinationModulePath, sizeof(destinationModulePath), "%s/src/tools/seed.zr", projectRoot);

    if (!copy_file_bytes(fixtureProjectPath, destinationProjectPath) ||
        !copy_file_bytes(fixtureMainPath, destinationMainPath) ||
        !copy_file_bytes(fixtureModulePath, destinationModulePath)) {
        return ZR_FALSE;
    }

    snprintf(projectPath, projectPathSize, "%s", destinationProjectPath);
    return ZR_TRUE;
}

static TZrBool load_manifest_for_project(const TZrChar *projectPath,
                                         SZrCliProjectContext *projectContext,
                                         SZrCliIncrementalManifest *manifest) {
    SZrGlobalState *global;
    TZrBool success = ZR_FALSE;

    if (projectPath == ZR_NULL || projectContext == ZR_NULL || manifest == ZR_NULL) {
        return ZR_FALSE;
    }

    global = ZrCli_Project_CreateProjectGlobal(projectPath);
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    success = ZrCli_ProjectContext_FromGlobal(projectContext, global, projectPath) &&
              ZrCli_Project_LoadManifest(projectContext, manifest);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    return success;
}

static void init_incremental_compile_command(SZrCliCommand *command, const TZrChar *projectPath) {
    TEST_ASSERT_NOT_NULL(command);
    TEST_ASSERT_NOT_NULL(projectPath);

    memset(command, 0, sizeof(*command));
    command->mode = ZR_CLI_MODE_COMPILE_PROJECT;
    command->projectPath = projectPath;
    command->emitIntermediate = ZR_TRUE;
    command->incremental = ZR_TRUE;
    command->executionMode = ZR_CLI_EXECUTION_MODE_BINARY;
}

static void init_binary_run_command(SZrCliCommand *command, const TZrChar *projectPath) {
    TEST_ASSERT_NOT_NULL(command);
    TEST_ASSERT_NOT_NULL(projectPath);

    memset(command, 0, sizeof(*command));
    command->mode = ZR_CLI_MODE_RUN_PROJECT;
    command->projectPath = projectPath;
    command->executionMode = ZR_CLI_EXECUTION_MODE_BINARY;
}

static void assert_capture_returns_expected_int(const SZrCliRunCapture *capture,
                                                TZrInt64 expectedValue,
                                                const TZrChar *expectedExecutedVia) {
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, capture->result.type);
    TEST_ASSERT_EQUAL_INT64(expectedValue, capture->result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_STRING(expectedExecutedVia, capture->executedVia);
}

static void assert_manifest_entry_has_single_import(const SZrCliManifestEntry *entry, const TZrChar *expectedImport) {
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(expectedImport);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)entry->imports.count);
    TEST_ASSERT_NOT_NULL(entry->imports.items);
    TEST_ASSERT_EQUAL_STRING(expectedImport, entry->imports.items[0]);
}

static void assert_manifest_entry_has_no_imports(const SZrCliManifestEntry *entry) {
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)entry->imports.count);
}

static void assert_manifest_entry_missing(const SZrCliIncrementalManifest *manifest, const TZrChar *moduleName) {
    TEST_ASSERT_NOT_NULL(manifest);
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NULL(ZrCli_Project_FindManifestEntryConst(manifest, moduleName));
}

static void test_cli_incremental_decorator_import_compile_skips_clean_rebuild_and_keeps_binary_run_stable(void) {
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZroPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZriPath[ZR_TESTS_PATH_MAX];
    TZrChar firstMainZroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar firstDecoratedUserZroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar firstDecoratorsZroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    SZrCliCommand compileCommand;
    SZrCliCommand runCommand;
    SZrCliCompileSummary firstSummary;
    SZrCliCompileSummary secondSummary;
    SZrCliProjectContext projectContext;
    SZrCliIncrementalManifest manifest;
    const SZrCliManifestEntry *mainEntry;
    const SZrCliManifestEntry *decoratedUserEntry;
    const SZrCliManifestEntry *decoratorsEntry;
    SZrCliRunCapture capture;

    memset(&firstSummary, 0, sizeof(firstSummary));
    memset(&secondSummary, 0, sizeof(secondSummary));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&manifest, 0, sizeof(manifest));
    memset(&capture, 0, sizeof(capture));
    memset(firstMainZroHash, 0, sizeof(firstMainZroHash));
    memset(firstDecoratedUserZroHash, 0, sizeof(firstDecoratedUserZroHash));
    memset(firstDecoratorsZroHash, 0, sizeof(firstDecoratorsZroHash));

    TEST_ASSERT_TRUE(prepare_decorator_import_fixture(projectRoot,
                                                      sizeof(projectRoot),
                                                      projectPath,
                                                      sizeof(projectPath)));

    init_incremental_compile_command(&compileCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &firstSummary));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)firstSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.removedCount);

    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)manifest.version);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)manifest.count);

    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext, "main", mainZroPath, sizeof(mainZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorated_user",
                                                     decoratedUserZroPath,
                                                     sizeof(decoratedUserZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorators",
                                                     decoratorsZroPath,
                                                     sizeof(decoratorsZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext, "main", mainZriPath, sizeof(mainZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorated_user",
                                                           decoratedUserZriPath,
                                                           sizeof(decoratedUserZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorators",
                                                           decoratorsZriPath,
                                                           sizeof(decoratorsZriPath)));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZriPath));

    mainEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "main");
    decoratedUserEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorated_user");
    decoratorsEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorators");
    TEST_ASSERT_NOT_NULL(mainEntry);
    TEST_ASSERT_NOT_NULL(decoratedUserEntry);
    TEST_ASSERT_NOT_NULL(decoratorsEntry);
    assert_manifest_entry_has_single_import(mainEntry, "decorated_user");
    assert_manifest_entry_has_single_import(decoratedUserEntry, "decorators");
    assert_manifest_entry_has_no_imports(decoratorsEntry);

    snprintf(firstMainZroHash, sizeof(firstMainZroHash), "%s", mainEntry->zroHash);
    snprintf(firstDecoratedUserZroHash, sizeof(firstDecoratedUserZroHash), "%s", decoratedUserEntry->zroHash);
    snprintf(firstDecoratorsZroHash, sizeof(firstDecoratorsZroHash), "%s", decoratorsEntry->zroHash);
    ZrCli_Project_Manifest_Free(&manifest);

    init_binary_run_command(&runCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 31, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
    memset(&capture, 0, sizeof(capture));

    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &secondSummary));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)secondSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)secondSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)secondSummary.removedCount);

    memset(&projectContext, 0, sizeof(projectContext));
    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)manifest.version);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)manifest.count);

    mainEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "main");
    decoratedUserEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorated_user");
    decoratorsEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorators");
    TEST_ASSERT_NOT_NULL(mainEntry);
    TEST_ASSERT_NOT_NULL(decoratedUserEntry);
    TEST_ASSERT_NOT_NULL(decoratorsEntry);
    assert_manifest_entry_has_single_import(mainEntry, "decorated_user");
    assert_manifest_entry_has_single_import(decoratedUserEntry, "decorators");
    assert_manifest_entry_has_no_imports(decoratorsEntry);
    TEST_ASSERT_EQUAL_STRING(firstMainZroHash, mainEntry->zroHash);
    TEST_ASSERT_EQUAL_STRING(firstDecoratedUserZroHash, decoratedUserEntry->zroHash);
    TEST_ASSERT_EQUAL_STRING(firstDecoratorsZroHash, decoratorsEntry->zroHash);
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 31, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
}

static void test_cli_incremental_decorator_import_prunes_removed_modules_and_keeps_binary_run_consistent(void) {
    static const TZrChar *replacementMainSource = "return 7;\n";
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar mainZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZroPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZriPath[ZR_TESTS_PATH_MAX];
    TZrChar prunedMainZroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    SZrCliCommand compileCommand;
    SZrCliCommand runCommand;
    SZrCliCompileSummary firstSummary;
    SZrCliCompileSummary pruneSummary;
    SZrCliCompileSummary stableSummary;
    SZrCliProjectContext projectContext;
    SZrCliIncrementalManifest manifest;
    const SZrCliManifestEntry *mainEntry;
    SZrCliRunCapture capture;

    memset(&firstSummary, 0, sizeof(firstSummary));
    memset(&pruneSummary, 0, sizeof(pruneSummary));
    memset(&stableSummary, 0, sizeof(stableSummary));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&manifest, 0, sizeof(manifest));
    memset(&capture, 0, sizeof(capture));
    memset(prunedMainZroHash, 0, sizeof(prunedMainZroHash));

    TEST_ASSERT_TRUE(prepare_decorator_import_fixture_named("decorator_import_prune",
                                                            projectRoot,
                                                            sizeof(projectRoot),
                                                            projectPath,
                                                            sizeof(projectPath)));

    TEST_ASSERT_TRUE(join_path_suffix(projectRoot, "/src/main.zr", mainSourcePath, sizeof(mainSourcePath)));
    TEST_ASSERT_TRUE(join_path_suffix(projectRoot,
                                      "/src/decorated_user.zr",
                                      decoratedUserSourcePath,
                                      sizeof(decoratedUserSourcePath)));
    TEST_ASSERT_TRUE(join_path_suffix(projectRoot,
                                      "/src/decorators.zr",
                                      decoratorsSourcePath,
                                      sizeof(decoratorsSourcePath)));

    init_incremental_compile_command(&compileCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &firstSummary));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)firstSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.removedCount);

    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext, "main", mainZroPath, sizeof(mainZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorated_user",
                                                     decoratedUserZroPath,
                                                     sizeof(decoratedUserZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorators",
                                                     decoratorsZroPath,
                                                     sizeof(decoratorsZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext, "main", mainZriPath, sizeof(mainZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorated_user",
                                                           decoratedUserZriPath,
                                                           sizeof(decoratedUserZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorators",
                                                           decoratorsZriPath,
                                                           sizeof(decoratorsZriPath)));
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_TRUE(write_text_file(mainSourcePath, replacementMainSource));
    TEST_ASSERT_EQUAL_INT(0, remove(decoratedUserSourcePath));
    TEST_ASSERT_EQUAL_INT(0, remove(decoratorsSourcePath));

    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &pruneSummary));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)pruneSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)pruneSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)pruneSummary.removedCount);

    memset(&projectContext, 0, sizeof(projectContext));
    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)manifest.version);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)manifest.count);

    mainEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "main");
    TEST_ASSERT_NOT_NULL(mainEntry);
    assert_manifest_entry_has_no_imports(mainEntry);
    assert_manifest_entry_missing(&manifest, "decorated_user");
    assert_manifest_entry_missing(&manifest, "decorators");
    snprintf(prunedMainZroHash, sizeof(prunedMainZroHash), "%s", mainEntry->zroHash);
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratedUserZroPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratorsZroPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratorsZriPath));

    init_binary_run_command(&runCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 7, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
    memset(&capture, 0, sizeof(capture));

    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &stableSummary));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)stableSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)stableSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)stableSummary.removedCount);

    memset(&projectContext, 0, sizeof(projectContext));
    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)manifest.count);
    mainEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "main");
    TEST_ASSERT_NOT_NULL(mainEntry);
    assert_manifest_entry_has_no_imports(mainEntry);
    TEST_ASSERT_EQUAL_STRING(prunedMainZroHash, mainEntry->zroHash);
    assert_manifest_entry_missing(&manifest, "decorated_user");
    assert_manifest_entry_missing(&manifest, "decorators");
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 7, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
}

static void test_cli_incremental_decorator_import_rename_reuses_clean_dependencies_and_prunes_old_artifacts(void) {
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar oldModuleSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar newModuleSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar oldModuleZroPath[ZR_TESTS_PATH_MAX];
    TZrChar oldModuleZriPath[ZR_TESTS_PATH_MAX];
    TZrChar newModuleZroPath[ZR_TESTS_PATH_MAX];
    TZrChar newModuleZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    SZrCliCommand compileCommand;
    SZrCliCommand runCommand;
    SZrCliCompileSummary firstSummary;
    SZrCliCompileSummary renameSummary;
    SZrCliCompileSummary stableSummary;
    SZrCliProjectContext projectContext;
    SZrCliIncrementalManifest manifest;
    const SZrCliManifestEntry *mainEntry;
    const SZrCliManifestEntry *renamedEntry;
    const SZrCliManifestEntry *decoratorsEntry;
    SZrCliRunCapture capture;

    memset(&firstSummary, 0, sizeof(firstSummary));
    memset(&renameSummary, 0, sizeof(renameSummary));
    memset(&stableSummary, 0, sizeof(stableSummary));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&manifest, 0, sizeof(manifest));
    memset(&capture, 0, sizeof(capture));
    memset(decoratorsZroHash, 0, sizeof(decoratorsZroHash));

    TEST_ASSERT_TRUE(prepare_decorator_import_fixture_named("decorator_import_rename",
                                                            projectRoot,
                                                            sizeof(projectRoot),
                                                            projectPath,
                                                            sizeof(projectPath)));
    TEST_ASSERT_TRUE(join_path_suffix(projectRoot, "/src/main.zr", mainSourcePath, sizeof(mainSourcePath)));
    TEST_ASSERT_TRUE(join_path_suffix(projectRoot,
                                      "/src/decorated_user.zr",
                                      oldModuleSourcePath,
                                      sizeof(oldModuleSourcePath)));
    TEST_ASSERT_TRUE(join_path_suffix(projectRoot,
                                      "/src/decorated_user_v2.zr",
                                      newModuleSourcePath,
                                      sizeof(newModuleSourcePath)));

    init_incremental_compile_command(&compileCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &firstSummary));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)firstSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.removedCount);

    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorated_user",
                                                     oldModuleZroPath,
                                                     sizeof(oldModuleZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorated_user",
                                                           oldModuleZriPath,
                                                           sizeof(oldModuleZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorators",
                                                     newModuleZroPath,
                                                     sizeof(newModuleZroPath)));
    decoratorsEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorators");
    TEST_ASSERT_NOT_NULL(decoratorsEntry);
    snprintf(decoratorsZroHash, sizeof(decoratorsZroHash), "%s", decoratorsEntry->zroHash);
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_EQUAL_INT(0, rename(oldModuleSourcePath, newModuleSourcePath));
    TEST_ASSERT_TRUE(rewrite_text_file_replacing_once(newModuleSourcePath,
                                                      "%module \"decorated_user\";",
                                                      "%module \"decorated_user_v2\";"));
    TEST_ASSERT_TRUE(rewrite_text_file_replacing_once(mainSourcePath,
                                                      "%import(\"decorated_user\")",
                                                      "%import(\"decorated_user_v2\")"));

    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &renameSummary));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)renameSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)renameSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (unsigned int)renameSummary.removedCount);

    memset(&projectContext, 0, sizeof(projectContext));
    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)manifest.version);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)manifest.count);

    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorated_user_v2",
                                                     newModuleZroPath,
                                                     sizeof(newModuleZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorated_user_v2",
                                                           newModuleZriPath,
                                                           sizeof(newModuleZriPath)));
    mainEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "main");
    renamedEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorated_user_v2");
    decoratorsEntry = ZrCli_Project_FindManifestEntryConst(&manifest, "decorators");
    TEST_ASSERT_NOT_NULL(mainEntry);
    TEST_ASSERT_NOT_NULL(renamedEntry);
    TEST_ASSERT_NOT_NULL(decoratorsEntry);
    assert_manifest_entry_has_single_import(mainEntry, "decorated_user_v2");
    assert_manifest_entry_has_single_import(renamedEntry, "decorators");
    assert_manifest_entry_has_no_imports(decoratorsEntry);
    TEST_ASSERT_EQUAL_STRING(decoratorsZroHash, decoratorsEntry->zroHash);
    assert_manifest_entry_missing(&manifest, "decorated_user");
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_FALSE(ZrTests_File_Exists(oldModuleZroPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(oldModuleZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(newModuleZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(newModuleZriPath));

    init_binary_run_command(&runCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 31, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
    memset(&capture, 0, sizeof(capture));

    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &stableSummary));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)stableSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)stableSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)stableSummary.removedCount);

    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 31, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
}

static void test_cli_incremental_disabling_intermediate_prunes_stale_zri_for_reachable_modules(void) {
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZroPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZroPath[ZR_TESTS_PATH_MAX];
    TZrChar mainZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratedUserZriPath[ZR_TESTS_PATH_MAX];
    TZrChar decoratorsZriPath[ZR_TESTS_PATH_MAX];
    SZrCliCommand compileCommand;
    SZrCliCommand runCommand;
    SZrCliCompileSummary firstSummary;
    SZrCliCompileSummary pruneIntermediateSummary;
    SZrCliCompileSummary stableSummary;
    SZrCliProjectContext projectContext;
    SZrCliIncrementalManifest manifest;
    SZrCliRunCapture capture;

    memset(&firstSummary, 0, sizeof(firstSummary));
    memset(&pruneIntermediateSummary, 0, sizeof(pruneIntermediateSummary));
    memset(&stableSummary, 0, sizeof(stableSummary));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&manifest, 0, sizeof(manifest));
    memset(&capture, 0, sizeof(capture));

    TEST_ASSERT_TRUE(prepare_decorator_import_fixture_named("decorator_import_intermediate_toggle",
                                                            projectRoot,
                                                            sizeof(projectRoot),
                                                            projectPath,
                                                            sizeof(projectPath)));

    init_incremental_compile_command(&compileCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &firstSummary));
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)firstSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)firstSummary.removedCount);

    TEST_ASSERT_TRUE(load_manifest_for_project(projectPath, &projectContext, &manifest));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext, "main", mainZroPath, sizeof(mainZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorated_user",
                                                     decoratedUserZroPath,
                                                     sizeof(decoratedUserZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext,
                                                     "decorators",
                                                     decoratorsZroPath,
                                                     sizeof(decoratorsZroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext, "main", mainZriPath, sizeof(mainZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorated_user",
                                                           decoratedUserZriPath,
                                                           sizeof(decoratedUserZriPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext,
                                                           "decorators",
                                                           decoratorsZriPath,
                                                           sizeof(decoratorsZriPath)));
    ZrCli_Project_Manifest_Free(&manifest);

    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZriPath));

    compileCommand.emitIntermediate = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &pruneIntermediateSummary));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)pruneIntermediateSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)pruneIntermediateSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)pruneIntermediateSummary.removedCount);

    TEST_ASSERT_TRUE(ZrTests_File_Exists(mainZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratedUserZroPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(decoratorsZroPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratorsZriPath));

    init_binary_run_command(&runCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 31, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
    memset(&capture, 0, sizeof(capture));

    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &stableSummary));
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)stableSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(3u, (unsigned int)stableSummary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)stableSummary.removedCount);
    TEST_ASSERT_FALSE(ZrTests_File_Exists(mainZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratedUserZriPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(decoratorsZriPath));

    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 31, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
}

static void test_cli_project_path_resolution_maps_dotted_module_name_to_nested_artifacts(void) {
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar zriPath[ZR_TESTS_PATH_MAX];
    SZrGlobalState *global = ZR_NULL;
    SZrCliProjectContext projectContext;

    memset(&projectContext, 0, sizeof(projectContext));
    memset(sourcePath, 0, sizeof(sourcePath));
    memset(zroPath, 0, sizeof(zroPath));
    memset(zriPath, 0, sizeof(zriPath));

    TEST_ASSERT_TRUE(prepare_cli_args_fixture(projectRoot,
                                              sizeof(projectRoot),
                                              projectPath,
                                              sizeof(projectPath)));

    global = ZrCli_Project_CreateProjectGlobal(projectPath);
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(ZrCli_ProjectContext_FromGlobal(&projectContext, global, projectPath));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveSourcePath(&projectContext, "tools.seed", sourcePath, sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(&projectContext, "tools.seed", zroPath, sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveIntermediatePath(&projectContext, "tools.seed", zriPath, sizeof(zriPath)));
    normalize_path_text(sourcePath);
    normalize_path_text(zroPath);
    normalize_path_text(zriPath);
    TEST_ASSERT_TRUE(text_ends_with(sourcePath, "/src/tools/seed.zr"));
    TEST_ASSERT_TRUE(text_ends_with(zroPath, "/bin/tools/seed.zro"));
    TEST_ASSERT_TRUE(text_ends_with(zriPath, "/bin/tools/seed.zri"));
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cli_incremental_decorator_import_compile_skips_clean_rebuild_and_keeps_binary_run_stable);
    RUN_TEST(test_cli_incremental_decorator_import_prunes_removed_modules_and_keeps_binary_run_consistent);
    RUN_TEST(test_cli_incremental_decorator_import_rename_reuses_clean_dependencies_and_prunes_old_artifacts);
    RUN_TEST(test_cli_incremental_disabling_intermediate_prunes_stale_zri_for_reachable_modules);
    RUN_TEST(test_cli_project_path_resolution_maps_dotted_module_name_to_nested_artifacts);

    return UNITY_END();
}
