//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_COMMON_STATE_H
#define ZR_VM_LIBRARY_COMMON_STATE_H
#include "zr_vm_library/conf.h"

/** we provide a default state for user here
 *  - builtin allocator uses default allocator of the system
 *  - global state uses file loader as source code loader, just provide a .zrp file as the project config file
 *  -
 */

struct ZR_STRUCT_ALIGN SZrLibrary_CommonState {
    SZrGlobalState *globalState;
};

typedef struct SZrLibrary_CommonState SZrLibrary_CommonState;

ZR_LIBRARY_API TZrPtr ZrLibrary_CommonState_BuiltinAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
                                                             TZrSize newSize, TInt64 flag);

ZR_LIBRARY_API SZrGlobalState *ZrLibrary_CommonState_CommonGlobalState_New(TNativeString configFilePath);

ZR_LIBRARY_API void ZrLibrary_CommonState_CommonGlobalState_Free(SZrGlobalState *globalState);

#endif // ZR_VM_LIBRARY_COMMON_STATE_H
