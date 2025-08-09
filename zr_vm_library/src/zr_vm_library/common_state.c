//
// Created by HeJiahui on 2025/7/27.
//
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

static TUInt64 CZrLibrary_CommonState_MemoryCounter[ZR_MEMORY_NATIVE_TYPE_ENUM_MAX] = {};
TZrPtr ZrLibrary_CommonState_BuiltinAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize,
                                              TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    if (newSize == 0) {
        CZrLibrary_CommonState_MemoryCounter[flag]--;
        free(pointer);
        return ZR_NULL;
    }
    if (pointer == ZR_NULL) {
        CZrLibrary_CommonState_MemoryCounter[flag]++;
        return (TZrPtr) malloc(newSize);
    }
    return (TZrPtr) realloc(pointer, newSize);
}

SZrGlobalState *ZrLibrary_CommonState_CommonGlobalState_New(TNativeString configFilePath) {
    SZrCallbackGlobal callback = {
            .afterStateInitialized = ZR_NULL,
            .afterThreadInitialized = ZR_NULL,
            .beforeStateReleased = ZR_NULL,
            .beforeThreadReleased = ZR_NULL,
    };

    EZrLibrary_File_Exist exist = ZrLibrary_File_Exist(configFilePath);
    if (exist != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_NULL;
    }


    SZrGlobalState *global =
            ZrGlobalStateNew(ZrLibrary_CommonState_BuiltinAllocator, ZR_NULL, 0x1234567890ABCDEF, &callback);

    TNativeString configContent = ZrLibrary_File_ReadAll(global, configFilePath);
    if (configContent == ZR_NULL) {
        ZrGlobalStateFree(global);
        return ZR_NULL;
    }
    SZrLibrary_Project *project = ZrLibrary_Project_New(global->mainThreadState, configContent, configFilePath);
    ZrMemoryRawFreeWithType(global, configContent, ZrNativeStringLength(configContent),
                            ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    global->userData = project;
    global->sourceLoader = ZrLibrary_Project_SourceLoadImplementation;
    return global;
}

void ZrLibrary_CommonState_CommonGlobalState_Free(SZrGlobalState *globalState) {
    if (globalState == ZR_NULL) {
        return;
    }
    ZrLibrary_Project_Free(globalState->mainThreadState, ZR_CAST(SZrLibrary_Project *, globalState->userData));
    globalState->userData = ZR_NULL;

    ZrGlobalStateFree(globalState);
}
