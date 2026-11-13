#include "zr_vm_core/exception.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_testing/module.h"
#include "zr_vm_parser/compiler.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GridTest {
    const char *path;
    const char *source;
    size_t length;
    TZrBool passed;
} GridTest;

static void execute(SZrState *state, TZrPtr data) {
    GridTest *test = data;
    SZrString *name = ZrCore_String_CreateFromNative(state, (TZrNativeString)test->path);
    SZrFunction *function = name ? ZrParser_Source_CompileTest(
            state, test->source, test->length, name) : NULL;
    SZrTypeValue callable, result;
    if (!function) return;
    ZrCore_Value_InitAsRawObject(state, &callable, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callable.type = ZR_VALUE_TYPE_FUNCTION;
    callable.isGarbageCollectable = ZR_TRUE;
    callable.isNative = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&result);
    test->passed = ZrLib_CallValue(state, &callable, NULL, NULL, 0, &result);
}

static int run_test(TZrNativeString projectPath, const char *source, size_t length) {
    SZrGlobalState *global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    int status = 1;
    if (global) {
        SZrState *state = global->mainThreadState;
        ZrCore_GlobalState_InitRegistry(state, global);
        ZrParser_ToGlobalState_Register(state);
        if (ZrVmLibContainer_Register(global) && ZrVmLibMath_Register(global) &&
            ZrVmLibSystem_Register(global) && ZrVmLibTesting_Register(global)) {
            SZrLibrary_Project *project = global->userData;
            char root[ZR_LIBRARY_MAX_PATH_LENGTH], logicalPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            ZrLibrary_File_PathJoin(ZrCore_String_GetNativeString(project->directory),
                                   ZrCore_String_GetNativeString(project->source), root);
            /* Local imports need a source identity inside the project. This
             * logical module exists only in memory; no source is staged. */
            ZrLibrary_File_PathJoin(root, "__swarm_grid_tests.zr", logicalPath);
            GridTest test = {logicalPath, source, length, ZR_FALSE};
            ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
            ZrVmLibTesting_ClearLastFailure();
            EZrThreadStatus execution = ZrCore_Exception_TryRun(state, execute, &test);
            if (execution == ZR_THREAD_STATUS_FINE && test.passed && !state->hasCurrentException) {
                status = 0;
            } else if (state->hasCurrentException) {
                ZrCore_Exception_LogUnhandled(state, &state->currentException);
            }
        }
        ZrCore_GlobalState_Free(global);
    }
    return status;
}

int main(int argc, char **argv) {
    FILE *input;
    char *source;
    long length;
    size_t suffixLength = 0;
    if (argc < 4) {
        fprintf(stderr, "usage: zr_swarm_grid_tests PROJECT.zrp TESTS.zr TEST_NAME...\n");
        return 2;
    }
    for (int i = 3; i < argc; ++i) {
        const unsigned char *name = (const unsigned char *)argv[i];
        size_t size = strlen(argv[i]);
        if (!size || !(isalpha(*name) || *name == '_') || size > SIZE_MAX - 6) return 2;
        while (*name) { if (!(isalnum(*name) || *name == '_')) return 2; ++name; }
        if (size + 6 > suffixLength) suffixLength = size + 6;
    }
    input = fopen(argv[2], "rb");
    if (!input) return 2;
    if (fseek(input, 0, SEEK_END) || (length = ftell(input)) < 0 ||
        (size_t)length > SIZE_MAX - suffixLength || fseek(input, 0, SEEK_SET)) {
        fclose(input);
        return 2;
    }
    source = malloc((size_t)length + suffixLength);
    if (!source) { fclose(input); return 2; }
    if (fread(source, 1, (size_t)length, input) != (size_t)length) {
        fclose(input);
        free(source);
        return 2;
    }
    fclose(input);
    for (int i = 3; i < argc; ++i) {
        char *tail = source + length;
        size_t size = strlen(argv[i]);
        *tail++ = '\n';
        memcpy(tail, argv[i], size); tail += size;
        memcpy(tail, "();\n", 4); tail += 4;
        *tail = '\0';
        /* Keep case-local VM/module state independent, matching the stock
         * runner's isolated test states. */
        if (run_test(argv[1], source, (size_t)(tail - source))) {
            fprintf(stderr, "SWARM_GRID_FAIL case=%s\n", argv[i]);
            free(source);
            return 1;
        }
    }
    free(source);
    printf("SWARM_GRID_OK cases=%d\n", argc - 3);
    return 0;
}
