#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "zr_vm_core/exception.h"
#include "zr_vm_core/state.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_parser/compiler.h"

static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000.0 + t.tv_nsec / 1000.0;
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
    SZrState *state;
    SZrTypeValue arg, result;
    double start, elapsed;
    int exit_code = 1;
    if ((argc != 3 && argc != 4) || (strcmp(argv[2], "interp") && strcmp(argv[2], "binary"))) { return 2; }
    global = ZrLibrary_CommonState_CommonGlobalState_New(argv[1]);
    if (!global) { return 1; }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    if (!strcmp(argv[2], "interp")) { ZrParser_ToGlobalState_Register(state); }
    if (!ZrVmLibSystem_Register(global) || !ZrVmLibContainer_Register(global)) { goto done; }
    ZrCore_Value_ResetAsNull(&arg);
    ZrCore_Value_ResetAsNull(&result);
    if (argc == 4 && !strcmp(argv[3], "p01")) {
        static const char *names[] = {"direct", "copy", "soa"};
        /* Debug/forced-GC runs check the same arithmetic without timing 900k
         * iterations. Only Release produces access-cost observations. */
#if defined(ZR_DEBUG)
        const int iterations = 1000;
#else
        const int iterations = 100000;
#endif
        for (unsigned k = 0; k < 3; ++k) {
            ZrLib_Value_SetInt(state, &arg, 1000);
            if (!ZrLib_CallModuleExport(state, "bench", names[k], &arg, 1, &result) ||
                !ZR_VALUE_IS_TYPE_FLOAT(result.type) || result.value.nativeObject.nativeDouble != 2003.0) {
                fprintf(stderr, "P01 cost warmup failed: %s\n", names[k]); goto done;
            }
            for (int repeat = 0; repeat < 3; ++repeat) {
                ZrLib_Value_SetInt(state, &arg, iterations);
                start = now_us();
                if (!ZrLib_CallModuleExport(state, "bench", names[k], &arg, 1, &result) ||
                    !ZR_VALUE_IS_TYPE_FLOAT(result.type) || result.value.nativeObject.nativeDouble != 3.0 + iterations * 2.0) {
                    fprintf(stderr, "P01 cost result failed: %s\n", names[k]); goto done;
                }
                elapsed = now_us() - start;
                printf("P01_COST path=%s repeat=%d iterations=%d ns_per_pair_update=%.3f\n", names[k], repeat, iterations, elapsed * 1000.0 / iterations);
            }
        }
        printf("PROBE_OK P01 host cost-checks\nexecuted_via=%s\n", argv[2]);
        exit_code = 0; goto done;
    }
    if (argc == 4 && !strcmp(argv[3], "p08")) {
        if (!ZrLib_CallModuleExport(state, "end", "finished", ZR_NULL, 0, &result) ||
            !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != 1) {
            fprintf(stderr, "P08 program-end destructor count is not one\n"); goto done;
        }
        if (!ZrLib_CallModuleExport(state, "end", "afterGC", ZR_NULL, 0, &result) ||
            !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != 1) {
            fprintf(stderr, "P08 destructor repeated after GC\n"); goto done;
        }
        printf("PROBE_OK P08 host program-end-destructor=1 after-full-GC=1\n");
        printf("executed_via=%s\n", argv[2]);
        exit_code = 0; goto done;
    }
    /* Import executes main's top-level once, including the imported game control. */
    if (!ZrLib_CallModuleExport(state, "main", "boot", ZR_NULL, 0, &result) ||
        !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != 1) {
        fprintf(stderr, "P21 entry/boot failed\n"); goto done;
    }
    start = now_us();
    for (int i = 0; i < 10000; ++i) {
        ZrLib_Value_SetInt(state, &arg, i);
        if (!ZrLib_CallModuleExport(state, "game", "tick", &arg, 1, &result) ||
            !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != i + 1) {
            fprintf(stderr, "P21 tick failed at %d\n", i); goto done;
        }
    }
    elapsed = now_us() - start;
    if (ZrLib_CallModuleExport(state, "game", "fail", ZR_NULL, 0, &result) || !state->hasCurrentException) {
        fprintf(stderr, "P21 expected exception absent\n"); goto done;
    }
    ZrCore_State_ResetThread(state, state->currentExceptionStatus);
    if (state->hasCurrentException) { fprintf(stderr, "P21 exception reset failed\n"); goto done; }
    ZrLib_Value_SetInt(state, &arg, 41);
    if (!ZrLib_CallModuleExport(state, "game", "tick", &arg, 1, &result) ||
        !ZR_VALUE_IS_TYPE_INT(result.type) || result.value.nativeObject.nativeInt64 != 42 ||
        state->threadStatus != ZR_THREAD_STATUS_FINE) {
        fprintf(stderr, "P21 call after exception reset failed\n"); goto done;
    }
    printf("PROBE_OK P21 host module=game calls=10000 us_per_call=%.6f exception_reset=OK\n", elapsed / 10000.0);
    printf("executed_via=%s\n", argv[2]);
    exit_code = 0;
done:
    if (exit_code && state->hasCurrentException) { ZrCore_Exception_LogUnhandled(state, &state->currentException); }
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    return exit_code;
}
