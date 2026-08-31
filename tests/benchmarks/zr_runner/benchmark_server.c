#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_lib_system/module.h"

#define ZR_BENCHMARK_PROTOCOL_LINE_CAPACITY 256
#define ZR_BENCHMARK_MAX_REPETITIONS UINT32_C(1048576)

typedef struct ZrBenchmarkOptions {
    const char *projectPath;
    const char *caseName;
    const char *tier;
} ZrBenchmarkOptions;

typedef struct ZrBenchmarkExecuteRequest {
    SZrFunction *function;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrBenchmarkExecuteRequest;

static FILE *zr_benchmark_protocol_output_open(void) {
    int savedStdout;
    int nullOutput;
    FILE *protocolOutput;

#if defined(_WIN32)
    savedStdout = _dup(_fileno(stdout));
    if (savedStdout < 0) {
        return ZR_NULL;
    }
    protocolOutput = _fdopen(savedStdout, "w");
    nullOutput = _open("NUL", _O_WRONLY);
#else
    savedStdout = dup(STDOUT_FILENO);
    if (savedStdout < 0) {
        return ZR_NULL;
    }
    protocolOutput = fdopen(savedStdout, "w");
    nullOutput = open("/dev/null", O_WRONLY);
#endif
    if (protocolOutput == ZR_NULL || nullOutput < 0) {
        if (protocolOutput != ZR_NULL) {
            fclose(protocolOutput);
        }
#if defined(_WIN32)
        else {
            _close(savedStdout);
        }
        if (nullOutput >= 0) {
            _close(nullOutput);
        }
#else
        else {
            close(savedStdout);
        }
        if (nullOutput >= 0) {
            close(nullOutput);
        }
#endif
        return ZR_NULL;
    }

    fflush(stdout);
#if defined(_WIN32)
    if (_dup2(nullOutput, _fileno(stdout)) != 0) {
        _close(nullOutput);
        fclose(protocolOutput);
        return ZR_NULL;
    }
    _close(nullOutput);
#else
    if (dup2(nullOutput, STDOUT_FILENO) < 0) {
        close(nullOutput);
        fclose(protocolOutput);
        return ZR_NULL;
    }
    close(nullOutput);
#endif
    setvbuf(protocolOutput, ZR_NULL, _IONBF, 0);
    return protocolOutput;
}

static void zr_benchmark_protocol_output_close(FILE *protocolOutput) {
    if (protocolOutput == ZR_NULL) {
        return;
    }
    fflush(stdout);
    fflush(protocolOutput);
    fclose(protocolOutput);
}

static TZrBool zr_benchmark_parse_options(int argc, char **argv, ZrBenchmarkOptions *options) {
    int index;
    TZrBool serverMode = ZR_FALSE;

    if (options == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(options, 0, sizeof(*options));
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--benchmark-server") == 0) {
            serverMode = ZR_TRUE;
        } else if (strcmp(argv[index], "--project") == 0 && index + 1 < argc) {
            options->projectPath = argv[++index];
        } else if (strcmp(argv[index], "--case") == 0 && index + 1 < argc) {
            options->caseName = argv[++index];
        } else if (strcmp(argv[index], "--tier") == 0 && index + 1 < argc) {
            options->tier = argv[++index];
        } else {
            return ZR_FALSE;
        }
    }
    return serverMode && options->projectPath != ZR_NULL && options->caseName != ZR_NULL &&
           options->tier != ZR_NULL &&
           (strcmp(options->caseName, "numeric_loops") == 0 ||
            strcmp(options->caseName, "dispatch_loops") == 0);
}

static TZrBool zr_benchmark_binary_source_loader(SZrState *state,
                                                 TZrNativeString sourcePath,
                                                 TZrNativeString md5,
                                                 SZrIo *io) {
    const SZrLibrary_Project *project;
    SZrLibrary_File_Reader *reader;
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    ZR_UNUSED_PARAMETER(md5);
    if (state == ZR_NULL || state->global == ZR_NULL || sourcePath == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }
    project = ZrLibrary_Project_GetFromGlobal(state->global);
    if (project == ZR_NULL ||
        !ZrLibrary_Project_ResolveBinaryPath(project, sourcePath, resolvedPath, sizeof(resolvedPath))) {
        return ZR_FALSE;
    }
    reader = ZrLibrary_File_OpenRead(state->global, resolvedPath, ZR_TRUE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Io_Init(state,
                   io,
                   ZrLibrary_File_SourceReadImplementation,
                   ZrLibrary_File_SourceCloseImplementation,
                   reader);
    io->isBinary = ZR_TRUE;
    return ZR_TRUE;
}

static SZrFunction *zr_benchmark_load_entry(SZrState *state,
                                            const SZrLibrary_Project *project,
                                            TZrChar *loadedPath,
                                            TZrSize loadedPathSize) {
    SZrIoSource *source;
    SZrFunction *function;

    if (!ZrLibrary_Project_ResolveBinaryPath(project, "main", loadedPath, loadedPathSize)) {
        return ZR_NULL;
    }
    source = ZrCore_Io_LoadSource(state, "main", ZR_NULL);
    if (source == ZR_NULL) {
        return ZR_NULL;
    }
    function = ZrCore_Io_LoadEntryFunctionToRuntime(state, source);
    ZrCore_Io_ReadSourceFree(state->global, source);
    return function;
}

static SZrObjectModule *zr_benchmark_prepare_entry_module(SZrState *state,
                                                          SZrFunction *entryFunction,
                                                          const TZrChar *loadedPath) {
    SZrObjectModule *module;
    SZrString *modulePath;
    TZrUInt64 pathHash;

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        return ZR_NULL;
    }
    modulePath = ZrCore_String_CreateFromNative(state, (TZrNativeString)loadedPath);
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }
    pathHash = ZrCore_Module_CalculatePathHash(state, modulePath);
    ZrCore_Module_SetInfo(state, module, ZR_NULL, pathHash, modulePath);
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, module, entryFunction);
    ZrCore_Module_CreatePrototypesFromConstants(state, module, entryFunction);
    return module;
}

static void zr_benchmark_execute_body(SZrState *state, TZrPtr arguments) {
    ZrBenchmarkExecuteRequest *request = (ZrBenchmarkExecuteRequest *)arguments;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &anchor);
    ZrCore_Closure_PushToStack(state, request->function, ZR_NULL, base, state->stackTop.valuePointer);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    if (closureValue == ZR_NULL || closureValue->value.object == ZR_NULL) {
        return;
    }
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;
    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    request->callCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static TZrBool zr_benchmark_execute(SZrState *state, SZrFunction *function, TZrInt64 *checksum) {
    ZrBenchmarkExecuteRequest request;
    SZrTypeValue result;
    EZrThreadStatus status;

    memset(&request, 0, sizeof(request));
    ZrCore_Value_ResetAsNull(&result);
    request.function = function;
    ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    status = ZrCore_Exception_TryRun(state, zr_benchmark_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE || !request.callCompleted || request.resultBase == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_Copy(state, &result, ZrCore_Stack_GetValue(request.resultBase));
    if (!ZR_VALUE_IS_TYPE_INT(result.type)) {
        return ZR_FALSE;
    }
    *checksum = result.value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool zr_benchmark_parse_positive_decimal(const char **cursor,
                                                   uint64_t maximum,
                                                   uint64_t *value) {
    uint64_t parsed = 0U;

    if (cursor == ZR_NULL || *cursor == ZR_NULL || value == ZR_NULL ||
        **cursor < '1' || **cursor > '9') {
        return ZR_FALSE;
    }
    while (**cursor >= '0' && **cursor <= '9') {
        const uint64_t digit = (uint64_t)(**cursor - '0');
        if (parsed > (maximum - digit) / 10U) {
            return ZR_FALSE;
        }
        parsed = parsed * 10U + digit;
        (*cursor)++;
    }
    *value = parsed;
    return ZR_TRUE;
}

static TZrBool zr_benchmark_parse_request(const char *line, int *index, uint32_t *repetitions) {
    const char *cursor;
    uint64_t parsedIndex;
    uint64_t parsedRepetitions;

    if (strncmp(line, "WARMUP ", 7) == 0) {
        cursor = line + 7;
    } else if (strncmp(line, "RUN ", 4) == 0) {
        cursor = line + 4;
    } else {
        return ZR_FALSE;
    }
    if (!zr_benchmark_parse_positive_decimal(&cursor, (uint64_t)INT_MAX, &parsedIndex) || *cursor != ' ') {
        return ZR_FALSE;
    }
    cursor++;
    if (!zr_benchmark_parse_positive_decimal(&cursor,
                                             (uint64_t)ZR_BENCHMARK_MAX_REPETITIONS,
                                             &parsedRepetitions) ||
        strcmp(cursor, "\n") != 0) {
        return ZR_FALSE;
    }
    *index = (int)parsedIndex;
    *repetitions = (uint32_t)parsedRepetitions;
    return ZR_TRUE;
}

static int zr_benchmark_run_protocol(FILE *protocolOutput,
                                     SZrState *state,
                                     SZrFunction *function,
                                     const ZrBenchmarkOptions *options) {
    char line[ZR_BENCHMARK_PROTOCOL_LINE_CAPACITY];

    fprintf(protocolOutput,
            "READY benchmark-checksum-v1:%s:%s\n",
            options->caseName,
            options->tier);
    while (fgets(line, sizeof(line), stdin) != ZR_NULL) {
        int index;
        TZrInt64 checksum;
        TZrInt64 repetitionChecksum;
        uint32_t repetitions;
        uint32_t repetition;
        size_t length = strlen(line);

        if (length == 0 || line[length - 1] != '\n') {
            fprintf(protocolOutput, "ERROR 0 malformed-request\n");
            return 1;
        }
        if (strcmp(line, "STOP\n") == 0) {
            return 0;
        }
        if (!zr_benchmark_parse_request(line, &index, &repetitions)) {
            fprintf(protocolOutput, "ERROR 0 malformed-request\n");
            return 1;
        }
        checksum = 0;
        for (repetition = 0U; repetition < repetitions; repetition++) {
            if (!zr_benchmark_execute(state, function, &repetitionChecksum)) {
                fprintf(protocolOutput, "ERROR %d zr-execution-failed\n", index);
                return 1;
            }
            if (repetition > 0U && repetitionChecksum != checksum) {
                fprintf(protocolOutput, "ERROR %d repetition-checksum-mismatch\n", index);
                return 1;
            }
            checksum = repetitionChecksum;
        }
        fprintf(protocolOutput, "DONE %d %lld\n", index, (long long)checksum);
    }
    return 1;
}

int main(int argc, char **argv) {
    ZrBenchmarkOptions options;
    FILE *protocolOutput;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state;
    const SZrLibrary_Project *project;
    SZrFunction *entryFunction = ZR_NULL;
    SZrObjectModule *entryModule = ZR_NULL;
    TZrBool ignoredFunction = ZR_FALSE;
    TZrBool ignoredModule = ZR_FALSE;
    TZrChar loadedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    int result = 1;

    if (!zr_benchmark_parse_options(argc, argv, &options)) {
        fprintf(stderr, "usage: zr_vm_zr_benchmark_server --benchmark-server --project <file.zrp> --case <case> --tier <tier>\n");
        return 2;
    }
    protocolOutput = zr_benchmark_protocol_output_open();
    if (protocolOutput == ZR_NULL) {
        return 1;
    }
    global = ZrLibrary_CommonState_CommonGlobalState_New((TZrNativeString)options.projectPath);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        goto cleanup;
    }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    if (!ZrVmLibSystem_Register(global)) {
        goto cleanup;
    }
    global->sourceLoader = zr_benchmark_binary_source_loader;
    project = ZrLibrary_Project_GetFromGlobal(global);
    if (project == ZR_NULL) {
        goto cleanup;
    }
    entryFunction = zr_benchmark_load_entry(state, project, loadedPath, sizeof(loadedPath));
    if (entryFunction == ZR_NULL) {
        goto cleanup;
    }
    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
    entryModule = zr_benchmark_prepare_entry_module(state, entryFunction, loadedPath);
    if (entryModule == ZR_NULL) {
        goto cleanup;
    }
    ignoredModule = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(entryModule));
    result = zr_benchmark_run_protocol(protocolOutput, state, entryFunction, &options);

cleanup:
    if (global != ZR_NULL) {
        if (ignoredModule) {
            ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(entryModule));
        }
        if (ignoredFunction) {
            ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
        }
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
    }
    zr_benchmark_protocol_output_close(protocolOutput);
    return result;
}
