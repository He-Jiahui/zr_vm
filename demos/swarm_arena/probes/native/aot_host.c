/* GD-M6: strict AOT loader, separate from the default and session hosts. */
#include "aot_runner.h"
#include "modules.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_parser/compiler.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Probe {
    SZrGlobalState *global;
    TZrUInt64 checksum;
    TZrBool ok;
} Probe;

static void *open_module(const char *path, const char *name) {
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "AOT_LOAD_ERROR %s\n", dlerror());
        return NULL;
    }
    void *symbol = dlsym(handle, "ZrVm_GetAotCompiledModule");
    FZrVmGetAotCompiledModule get_module = NULL;
    memcpy(&get_module, &symbol, sizeof(get_module));
    const ZrAotCompiledModule *module = get_module ? get_module() : NULL;
    if (!module || module->abiVersion != ZR_VM_AOT_ABI_VERSION ||
        module->backendKind != ZR_AOT_BACKEND_KIND_C || !module->moduleName ||
        strcmp(module->moduleName, name) || !module->entryThunk ||
        !module->codeRegistration || !module->functionThunks || !module->functionThunkCount) {
        fprintf(stderr, "AOT_LOAD_ERROR invalid descriptor for %s\n", name);
        dlclose(handle);
        return NULL;
    }
    printf("AOT_MODULE module=%s abi=%u backend=c thunks=%u embedded_bytes=%zu\n",
           module->moduleName, (unsigned)module->abiVersion,
           (unsigned)module->functionThunkCount, (size_t)module->embeddedModuleBlobLength);
    return handle;
}

static void execute_entry(SZrState *state, TZrPtr data) {
    Probe *probe = data;
    SZrGlobalState *global = state->global;
    ZrCore_GlobalState_InitRegistry(state, global);
    ZrParser_ToGlobalState_Register(state);
    if (!ZrVmLibMath_Register(global) || !ZrVmLibSystem_Register(global) ||
        !ZrVmLibContainer_Register(global) || !ZrVmLibFfi_Register(global) ||
        !ZrCore_TaskRuntime_RegisterBuiltins(global) ||
        !ZrLibrary_NativeRegistry_RegisterModule(global, ZrGame_HostModule()) ||
        !ZrLibrary_NativeRegistry_RegisterModule(global, ZrGame_GfxModule()) ||
        !ZrLibrary_AotRuntime_ConfigureGlobal(global, ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C, ZR_TRUE)) return;
    SZrTypeValue value;
    ZrCore_Value_ResetAsNull(&value);
    if (!ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &value) ||
        state->hasCurrentException || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        !ZR_VALUE_IS_TYPE_INT(value.type) || value.value.nativeObject.nativeInt64 < 0 ||
        ZrLibrary_AotRuntime_GetExecutedVia(global) != ZR_LIBRARY_EXECUTED_VIA_AOT_C) return;
    probe->checksum = (TZrUInt64)value.value.nativeObject.nativeInt64;
    probe->ok = ZR_TRUE;
}

static TZrBool invoke(TZrPtr context, TZrUInt64 *checksum, SZrAotCoverageCounts *coverage) {
    Probe *probe = context;
    EZrThreadStatus status = ZrCore_Exception_TryRun(probe->global->mainThreadState, execute_entry, probe);
    /* No runtime semantic-site sampler is supplied. Leave coverage unavailable. */
    ZrTests_AotCoverage_Init(coverage);
    if (status != ZR_THREAD_STATUS_FINE || !probe->ok) {
        const char *error = ZrLibrary_AotRuntime_GetLastError(probe->global);
        fprintf(stderr, "AOT_EXEC_ERROR %s\n", error ? error : "entry failed");
        const char *diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(probe->global);
        if (diagnostic) fprintf(stderr, "AOT_MODULE_DIAGNOSTIC %s\n", diagnostic);
        if (probe->global->mainThreadState->hasCurrentException)
            ZrCore_Exception_LogUnhandled(probe->global->mainThreadState,
                                          &probe->global->mainThreadState->currentException);
        return ZR_FALSE;
    }
    *checksum = probe->checksum;
    return ZR_TRUE;
}

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "--inspect")) {
        void *handle = open_module(argv[2], argv[3]);
        if (!handle) return 1;
        dlclose(handle);
        return 0;
    }
    if (argc != 9 || strcmp(argv[1], "--project") || strcmp(argv[3], "--library") ||
        strcmp(argv[5], "--module") || strcmp(argv[7], "--checksum")) {
        fprintf(stderr, "usage: %s --inspect library module\n"
                "       %s --project manifest --library library --module NAME --checksum N\n", argv[0], argv[0]);
        return 2;
    }
    char *end = NULL;
    errno = 0;
    unsigned long long expected = strtoull(argv[8], &end, 10);
    if (errno || argv[8][0] < '0' || argv[8][0] > '9' || !end || *end) return 2;
    SZrAotRunner runner;
    SZrAotRunnerResult result;
    SZrAotRunnerRequest request = {
        .requestedBackend = ZR_AOT_BACKEND_C, .entryToken = 1u,
        .expectedChecksum = (TZrUInt64)expected, .checksumRequired = ZR_TRUE,
        .allowInterpreterFallback = ZR_FALSE
    };
    Probe probe = {0};
    ZrTests_AotRunner_Init(&runner);
    void *handle = open_module(argv[4], argv[6]);
    if (handle) probe.global = ZrLibrary_CommonState_CommonGlobalState_New(argv[2]);
    if (probe.global && !ZrTests_AotRunner_Register(&runner, ZR_AOT_BACKEND_C, 1u,
                                                    "swarm-entry", invoke, &probe)) {
        ZrLibrary_CommonState_CommonGlobalState_Free(probe.global);
        dlclose(handle);
        return 1;
    }
    TZrBool ok = ZrTests_AotRunner_Run(&runner, &request, &result);
    printf("AOT_RUNNER status=%s requested=%s actual=%s entry_invoked=%u checksum=%llu"
           " coverage=%s reason=%s\n",
           ZrTests_AotRunner_StatusName(result.status),
           ZrTests_AotRunner_BackendName(result.requestedBackend),
           ZrTests_AotRunner_BackendName(result.actualBackend), (unsigned)result.entryInvoked,
           (unsigned long long)result.checksum, ZrTests_AotCoverage_StatusName(result.coverage.status),
           ZrTests_AotRunner_FailureName(result.failure));
    if (probe.global) ZrLibrary_CommonState_CommonGlobalState_Free(probe.global);
    if (handle) dlclose(handle);
    return ok && result.status == ZR_AOT_RUNNER_STATUS_RAN &&
           ZrTests_AotRunner_ValidateResult(&result) ? 0 : 1;
}
