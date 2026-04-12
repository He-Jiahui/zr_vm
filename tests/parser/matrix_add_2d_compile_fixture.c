#include "matrix_add_2d_compile_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#include <process.h>
#define zr_tests_getpid _getpid
#else
#include <unistd.h>
#define zr_tests_getpid getpid
#endif

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_parser.h"

static unsigned int g_matrix_add_2d_fixture_sequence = 0;

static TZrBool write_text_file(const TZrChar *path, const TZrChar *content, TZrSize length) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(content, 1, (size_t)length, file);
    fclose(file);
    return written == (size_t)length;
}

static TZrBool copy_fixture_file(const TZrChar *sourcePath, const TZrChar *targetPath) {
    TZrChar *content;
    TZrSize contentLength = 0;
    TZrBool success;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        return ZR_FALSE;
    }

    content = ZrTests_ReadTextFile(sourcePath, &contentLength);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    success = write_text_file(targetPath, content, contentLength);
    free(content);
    return success;
}

static TZrBool build_matrix_add_2d_fixture_source_path(const TZrChar *relativePath,
                                                       TZrChar *outPath,
                                                       TZrSize maxLen) {
    int written;

    if (relativePath == ZR_NULL || outPath == ZR_NULL || maxLen == 0) {
        return ZR_FALSE;
    }

    written = snprintf(outPath,
                       maxLen,
                       "%s/benchmarks/cases/matrix_add_2d/zr/%s",
                       ZR_VM_TESTS_SOURCE_DIR,
                       relativePath);
    if (written < 0 || (TZrSize)written >= maxLen) {
        outPath[0] = '\0';
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool prepare_fresh_matrix_add_2d_project_files(ZrMatrixAdd2dCompileFixture *fixture,
                                                         const TZrChar *artifactName) {
    TZrChar fixtureProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureMainPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureBenchConfigPath[ZR_TESTS_PATH_MAX];
    TZrChar targetBenchConfigPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureRootPath[ZR_TESTS_PATH_MAX];
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int processId;
    int written;

    if (fixture == ZR_NULL || artifactName == ZR_NULL || artifactName[0] == '\0') {
        return ZR_FALSE;
    }

    if (!build_matrix_add_2d_fixture_source_path("benchmark_matrix_add_2d.zrp",
                                                 fixtureProjectPath,
                                                 sizeof(fixtureProjectPath)) ||
        !build_matrix_add_2d_fixture_source_path("src/main.zr",
                                                 fixtureMainPath,
                                                 sizeof(fixtureMainPath)) ||
        !build_matrix_add_2d_fixture_source_path("src/bench_config.zr",
                                                 fixtureBenchConfigPath,
                                                 sizeof(fixtureBenchConfigPath))) {
        return ZR_FALSE;
    }

    timestamp = (unsigned long long)time(NULL);
    sequence = ++g_matrix_add_2d_fixture_sequence;
    processId = (unsigned int)zr_tests_getpid();
    written = snprintf(fixtureRootPath,
                       sizeof(fixtureRootPath),
                       "%s/compiler_regressions/matrix_add_2d/%s_%llu_%u_%u",
                       ZR_VM_TESTS_BINARY_DIR,
                       artifactName,
                       timestamp,
                       processId,
                       sequence);
    if (written < 0 || (TZrSize)written >= sizeof(fixtureRootPath)) {
        return ZR_FALSE;
    }

    written = snprintf(fixture->projectPath,
                       sizeof(fixture->projectPath),
                       "%s/benchmark_matrix_add_2d.zrp",
                       fixtureRootPath);
    if (written < 0 || (TZrSize)written >= sizeof(fixture->projectPath)) {
        fixture->projectPath[0] = '\0';
        return ZR_FALSE;
    }

    written = snprintf(fixture->sourceRootPath,
                       sizeof(fixture->sourceRootPath),
                       "%s/src",
                       fixtureRootPath);
    if (written < 0 || (TZrSize)written >= sizeof(fixture->sourceRootPath)) {
        fixture->sourceRootPath[0] = '\0';
        return ZR_FALSE;
    }

    written = snprintf(fixture->sourcePath,
                       sizeof(fixture->sourcePath),
                       "%s/main.zr",
                       fixture->sourceRootPath);
    if (written < 0 || (TZrSize)written >= sizeof(fixture->sourcePath)) {
        fixture->sourcePath[0] = '\0';
        return ZR_FALSE;
    }

    written = snprintf(targetBenchConfigPath,
                       sizeof(targetBenchConfigPath),
                       "%s/bench_config.zr",
                       fixture->sourceRootPath);
    if (written < 0 || (TZrSize)written >= sizeof(targetBenchConfigPath)) {
        return ZR_FALSE;
    }

    return copy_fixture_file(fixtureProjectPath, fixture->projectPath) &&
           copy_fixture_file(fixtureMainPath, fixture->sourcePath) &&
           copy_fixture_file(fixtureBenchConfigPath, targetBenchConfigPath);
}

TZrBool ZrTests_PrepareMatrixAdd2dCompileFixture(ZrMatrixAdd2dCompileFixture *fixture, const TZrChar *artifactName) {
    SZrString *sourceName;

    if (fixture == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(fixture, 0, sizeof(*fixture));
    if (!prepare_fresh_matrix_add_2d_project_files(fixture, artifactName)) {
        return ZR_FALSE;
    }

    fixture->global = ZrLibrary_CommonState_CommonGlobalState_New(fixture->projectPath);
    if (fixture->global == ZR_NULL) {
        ZrTests_FreeMatrixAdd2dCompileFixture(fixture);
        return ZR_FALSE;
    }

    fixture->state = fixture->global->mainThreadState;
    if (fixture->state == ZR_NULL) {
        ZrTests_FreeMatrixAdd2dCompileFixture(fixture);
        return ZR_FALSE;
    }

    ZrParser_ToGlobalState_Register(fixture->state);
    if (!ZrVmLibContainer_Register(fixture->global) || !ZrVmLibSystem_Register(fixture->global)) {
        ZrTests_FreeMatrixAdd2dCompileFixture(fixture);
        return ZR_FALSE;
    }

    fixture->source = ZrTests_ReadTextFile(fixture->sourcePath, ZR_NULL);
    if (fixture->source == ZR_NULL) {
        ZrTests_FreeMatrixAdd2dCompileFixture(fixture);
        return ZR_FALSE;
    }

    sourceName = ZrCore_String_CreateFromNative(fixture->state, fixture->sourcePath);
    if (sourceName == ZR_NULL) {
        ZrTests_FreeMatrixAdd2dCompileFixture(fixture);
        return ZR_FALSE;
    }

    fixture->function = ZrParser_Source_Compile(fixture->state,
                                                fixture->source,
                                                strlen(fixture->source),
                                                sourceName);
    if (fixture->function == ZR_NULL) {
        ZrTests_FreeMatrixAdd2dCompileFixture(fixture);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

void ZrTests_FreeMatrixAdd2dCompileFixture(ZrMatrixAdd2dCompileFixture *fixture) {
    if (fixture == ZR_NULL) {
        return;
    }

    if (fixture->function != ZR_NULL && fixture->state != ZR_NULL) {
        ZrCore_Function_Free(fixture->state, fixture->function);
        fixture->function = ZR_NULL;
    }

    if (fixture->source != ZR_NULL) {
        free(fixture->source);
        fixture->source = ZR_NULL;
    }

    if (fixture->global != ZR_NULL) {
        ZrLibrary_CommonState_CommonGlobalState_Free(fixture->global);
        fixture->global = ZR_NULL;
    }

    fixture->state = ZR_NULL;
    fixture->projectPath[0] = '\0';
    fixture->sourcePath[0] = '\0';
    fixture->sourceRootPath[0] = '\0';
}
