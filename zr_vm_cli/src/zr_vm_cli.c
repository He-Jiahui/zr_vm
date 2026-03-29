//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_cli.h"
#include <stdio.h>
#include "zr_vm_core.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/project.h"

static int cli_run(const int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: zr_vm_cli <project.zrp>\n");
        return 1;
    }

    SZrGlobalState *global = ZrLibrary_CommonState_CommonGlobalState_New(argv[1]);
    if (global == ZR_NULL) {
        fprintf(stderr, "failed to load project: %s\n", argv[1]);
        return 1;
    }

    ZrParser_ToGlobalState_Register(global->mainThreadState);
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);
    ZrVmLibFfi_Register(global);

    SZrTypeValue result;
    ZrCore_Value_ResetAsNull(&result);
    EZrThreadStatus status = ZrLibrary_Project_Run(global->mainThreadState, &result);

    if (status != ZR_THREAD_STATUS_FINE) {
        fprintf(stderr, "project execution failed with status %d\n", (int) status);
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    SZrString *resultString = ZrCore_Value_ConvertToString(global->mainThreadState, &result);
    if (resultString == ZR_NULL) {
        fprintf(stderr, "failed to stringify project result\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    printf("%s\n", ZrCore_String_GetNativeString(resultString));
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    return 0;
}

void ZrCli_Main(const int argc, char **argv) {
    (void) cli_run(argc, argv);
}

int main(const int argc, char **argv) {
    return cli_run(argc, argv);
}
