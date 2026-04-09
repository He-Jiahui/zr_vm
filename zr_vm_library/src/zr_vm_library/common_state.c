//
// Created by HeJiahui on 2025/7/27.
//
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "zr_vm_common/zr_runtime_sentinel_conf.h"

static TZrUInt64 CZrLibrary_CommonState_MemoryCounter[ZR_MEMORY_NATIVE_TYPE_ENUM_MAX] = {0};

static TZrBool zr_library_common_state_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void zr_library_common_state_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!zr_library_common_state_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-common-state] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}

static TZrBool zr_library_common_state_allocator_can_release_pointer(TZrPtr pointer) {
    return pointer != ZR_NULL && (uintptr_t)pointer >= (uintptr_t)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND;
}

TZrPtr ZrLibrary_CommonState_BuiltinAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize,
                                              TZrInt64 flag) {
    TZrBool canReleasePointer;
    TZrBool trackCounter;

    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    canReleasePointer = zr_library_common_state_allocator_can_release_pointer(pointer);
    trackCounter = flag >= 0 && flag < ZR_MEMORY_NATIVE_TYPE_ENUM_MAX;
    if (newSize == 0) {
        if (trackCounter && canReleasePointer) {
            CZrLibrary_CommonState_MemoryCounter[flag]--;
        }
        if (canReleasePointer) {
            free(pointer);
        }
        return ZR_NULL;
    }
    if (pointer == ZR_NULL || !canReleasePointer) {
        if (trackCounter) {
            CZrLibrary_CommonState_MemoryCounter[flag]++;
        }
        return (TZrPtr) malloc(newSize);
    }
    return (TZrPtr) realloc(pointer, newSize);
}

SZrGlobalState *ZrLibrary_CommonState_CommonGlobalState_New(TZrNativeString configFilePath) {
    SZrCallbackGlobal callback = {
            .afterStateInitialized = ZR_NULL,
            .afterThreadInitialized = ZR_NULL,
            .beforeStateReleased = ZR_NULL,
            .beforeThreadReleased = ZR_NULL,
    };

    EZrLibrary_File_Exist exist = ZrLibrary_File_Exist(configFilePath);
    zr_library_common_state_trace("begin config='%s' exist=%d",
                                  configFilePath != ZR_NULL ? configFilePath : "<null>",
                                  (int)exist);
    if (exist != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_NULL;
    }


    SZrGlobalState *global =
            ZrCore_GlobalState_New(ZrLibrary_CommonState_BuiltinAllocator, ZR_NULL, 0x1234567890ABCDEF, &callback);
    zr_library_common_state_trace("global=%p mainThread=%p", (void *)global,
                                  global != ZR_NULL ? (void *)global->mainThreadState : ZR_NULL);

    TZrNativeString configContent = ZrLibrary_File_ReadAll(global, configFilePath);
    zr_library_common_state_trace("configContent=%p", (void *)configContent);
    if (configContent == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        return ZR_NULL;
    }
    SZrLibrary_Project *project = ZrLibrary_Project_New(global->mainThreadState, configContent, configFilePath);
    zr_library_common_state_trace("project=%p", (void *)project);
    TZrSize configLength = ZrCore_NativeString_Length(configContent);
    ZrCore_Memory_RawFreeWithType(global, configContent, configLength + 1,
                            ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    if (project == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        return ZR_NULL;
    }
    global->userData = project;
    global->sourceLoader = ZrLibrary_Project_SourceLoadImplementation;
    global->sourceLoaderUserData = project;
    zr_library_common_state_trace("attach native registry");
    ZrLibrary_NativeRegistry_Attach(global);
    zr_library_common_state_trace("complete");
    return global;
}

void ZrLibrary_CommonState_CommonGlobalState_Free(SZrGlobalState *globalState) {
    if (globalState == ZR_NULL) {
        return;
    }
    ZrLibrary_Project_Free(globalState->mainThreadState, ZR_CAST(SZrLibrary_Project *, globalState->userData));
    globalState->userData = ZR_NULL;
    globalState->sourceLoaderUserData = ZR_NULL;
    ZrLibrary_NativeRegistry_Free(globalState);

    ZrCore_GlobalState_Free(globalState);
}
