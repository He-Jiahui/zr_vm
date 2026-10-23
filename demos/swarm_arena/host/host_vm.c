#include "host.h"
#include "modules.h"
#include "runtime.h"
#include "csim.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_parser/compiler.h"
#if defined(ZRG_HAS_NETWORK)
#include "zr_vm_lib_network/module.h"
#endif
#if defined(ZRG_HAS_THREAD)
#include "zr_vm_lib_thread/module.h"
#endif

#include <stdio.h>
#include <string.h>

static bool failure(ZrgHostVm *vm, const char *operation) {
    fprintf(stderr, "SWARM_ERROR game.%s failed\n", operation);
    if (vm->state->hasCurrentException)
        ZrCore_Exception_LogUnhandled(vm->state, &vm->state->currentException);
    const char *diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(vm->global);
    if (diagnostic) fprintf(stderr, "%s\n", diagnostic);
    return false;
}

bool zrg_host_vm_call_int(ZrgHostVm *vm, const char *name,
                          const int64_t *arguments, size_t count, int64_t *out) {
    if (vm->csim) {
        int64_t value;
        if (!strcmp(name, "tick") && count == 1) value = zrg_csim_tick(vm->csim, arguments[0]);
        else if (!strcmp(name, "render") && count == 1) value = zrg_csim_render(vm->csim, arguments[0]);
        else if (!strcmp(name, "fullHash") && count == 0) value = zrg_csim_full_hash(vm->csim);
        else if (!strcmp(name, "entityMax") && count == 1) value = zrg_csim_entity_max(vm->csim, arguments[0]);
        else {
            fprintf(stderr, "SWARM_ERROR c lane unknown operation %s\n", name);
            return false;
        }
        if (value < 0) return false;
        if (out) *out = value;
        return true;
    }
    SZrTypeValue args[4], result;
    if (count > 4) return false;
    for (size_t i = 0; i < count; ++i) {
        ZrCore_Value_ResetAsNull(&args[i]);
        ZrLib_Value_SetInt(vm->state, &args[i], arguments[i]);
    }
    ZrCore_Value_ResetAsNull(&result);
    if (!ZrLib_CallModuleExport(vm->state, "game", name, args, count, &result) ||
        vm->state->hasCurrentException || vm->state->threadStatus != ZR_THREAD_STATUS_FINE ||
        !ZR_VALUE_IS_TYPE_INT(result.type)) return failure(vm, name);
    if (out) *out = result.value.nativeObject.nativeInt64;
    return true;
}

typedef struct Bootstrap {
    ZrgHostVm *vm;
    const ZrgOptions *options;
    ZrLibTempValueRoot roots[2];
    bool ok;
} Bootstrap;

static void bootstrap(SZrState *state, TZrPtr data) {
    Bootstrap *boot = data;
    SZrGlobalState *global = state->global;
    ZrCore_GlobalState_InitRegistry(state, global);
    ZrParser_ToGlobalState_Register(state);
    /* Same runtime providers/order as CLI project.c's RegisterStandardModules.
     * Container also registers the canonical zr.iteration descriptor. */
    if (!ZrVmLibMath_Register(global) || !ZrVmLibSystem_Register(global) ||
#if defined(ZRG_HAS_NETWORK)
        !ZrVmLibNetwork_Register(global) ||
#endif
        !ZrVmLibContainer_Register(global) || !ZrVmLibFfi_Register(global) ||
#if defined(ZRG_HAS_THREAD)
        !ZrVmThread_Register(global) ||
#endif
        !ZrCore_TaskRuntime_RegisterBuiltins(global) ||
        !ZrLibrary_NativeRegistry_RegisterModule(global, ZrGame_HostModule()) ||
        !ZrLibrary_NativeRegistry_RegisterModule(global, ZrGame_GfxModule())) return;

    SZrTypeValue result, args[4];
    ZrCore_Value_ResetAsNull(&result);
    /* Import main once; its host branch skips the script-owned CLI loop. */
    if (!ZrLib_CallModuleExport(state, "main", "hostEntry", NULL, 0, &result) ||
        !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != 1) return;
    for (unsigned i = 0; i < 2; ++i) {
        if (!ZrLib_TempValueRoot_Begin(state, &boot->roots[i])) return;
        ZrLib_Value_SetString(state, ZrLib_TempValueRoot_Value(&boot->roots[i]), i ? "dod" : boot->options->scenario);
    }
    for (unsigned i = 0; i < 4; ++i) ZrCore_Value_ResetAsNull(&args[i]);
    /* Resolve the roots only after both string allocations, which can move GC objects. */
    args[0] = *ZrLib_TempValueRoot_Value(&boot->roots[0]);
    args[1] = *ZrLib_TempValueRoot_Value(&boot->roots[1]);
    ZrLib_Value_SetInt(state, &args[2], boot->options->seed);
    ZrLib_Value_SetInt(state, &args[3], boot->options->frames);
    if (!ZrLib_CallModuleExport(state, "game", "boot", args, 4, &result) ||
        !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != 0) return;
    boot->vm->booted = true;
    ZrCore_GarbageCollector_SetPauseBudgetUs(global, boot->options->gc_budget_us, boot->options->gc_budget_us);
    if (boot->options->gc_policy == ZRG_GC_MANUAL &&
        !zrg_host_vm_call_int(boot->vm, "gcDisable", NULL, 0, NULL)) return;
    boot->ok = true;
}

bool zrg_host_vm_open(ZrgHostVm *vm, const ZrgOptions *options) {
    zrg_set_host_driven(true);
    zrg_zones_reset();
    if (!strcmp(options->lane, "c")) {
        /* Retain the host's --project path contract. Native gameplay does not
         * import or execute the project and does not create a managed heap. */
        FILE *project = fopen(options->project, "r");
        if (!project) {
            fprintf(stderr, "SWARM_ERROR --project could not load %s\n", options->project);
            return false;
        }
        fclose(project);
        vm->csim = zrg_csim_new(options->scenario, options->seed, options->frames);
        vm->booted = vm->csim != NULL;
        return vm->booted;
    }
    vm->global = ZrLibrary_CommonState_CommonGlobalState_New((TZrNativeString)options->project);
    if (!vm->global) {
        fprintf(stderr, "SWARM_ERROR --project could not load %s\n", options->project);
        return false;
    }
    vm->state = vm->global->mainThreadState;
    Bootstrap boot = {.vm = vm, .options = options};
    EZrThreadStatus status = ZrCore_Exception_TryRun(vm->state, bootstrap, &boot);
    for (unsigned i = 2; i > 0; --i) ZrLib_TempValueRoot_End(&boot.roots[i - 1]);
    if (!boot.ok || status != ZR_THREAD_STATUS_FINE || vm->state->hasCurrentException ||
        vm->state->threadStatus != ZR_THREAD_STATUS_FINE) return failure(vm, "boot");
    return true;
}

static void gc_step(SZrState *state, TZrPtr unused) {
    (void)unused;
    ZrCore_GarbageCollector_GcStep(state);
}

bool zrg_host_vm_gc_step(ZrgHostVm *vm) {
    EZrThreadStatus status = ZrCore_Exception_TryRun(vm->state, gc_step, NULL);
    if (status != ZR_THREAD_STATUS_FINE || vm->state->hasCurrentException ||
        vm->state->threadStatus != ZR_THREAD_STATUS_FINE) return failure(vm, "gcStep");
    return true;
}

bool zrg_host_vm_shutdown(ZrgHostVm *vm, char *summary, size_t capacity) {
    if (vm->csim) {
        bool ok = zrg_csim_summary(vm->csim, summary, capacity);
        vm->booted = false;
        return ok;
    }
    SZrTypeValue result;
    ZrCore_Value_ResetAsNull(&result);
    if (!ZrLib_CallModuleExport(vm->state, "game", "shutdown", NULL, 0, &result) ||
        vm->state->hasCurrentException || vm->state->threadStatus != ZR_THREAD_STATUS_FINE ||
        !ZR_VALUE_IS_TYPE_STRING(result.type)) return failure(vm, "shutdown");
    vm->booted = false;
    if (summary && capacity) {
        const SZrString *text = (const SZrString *)result.value.object;
        size_t length = ZrCore_String_GetByteLength(text);
        if (length >= capacity) return failure(vm, "shutdown summary capacity");
        memcpy(summary, ZrCore_String_GetNativeString(text), length);
        summary[length] = '\0';
    }
    return true;
}

void zrg_host_vm_close(ZrgHostVm *vm) {
    zrg_csim_free(vm->csim);
    if (vm->state) {
        if (vm->state->hasCurrentException || vm->state->threadStatus != ZR_THREAD_STATUS_FINE)
            ZrCore_State_ResetThread(vm->state, vm->state->currentExceptionStatus);
        if (vm->booted) (void)zrg_host_vm_shutdown(vm, NULL, 0);
        /* No tasks, callbacks, external handles or live C roots remain. */
        ZrLibrary_NativeRegistry_Free(vm->global);
        ZrCore_State_Exit(vm->state);
    }
    ZrLibrary_CommonState_CommonGlobalState_Free(vm->global);
    *vm = (ZrgHostVm){0};
    zrg_set_host_driven(false);
}
