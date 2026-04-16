//
// Native AOT module ABI shared by generated backends and the runtime loader.
//

#ifndef ZR_VM_COMMON_ZR_AOT_ABI_H
#define ZR_VM_COMMON_ZR_AOT_ABI_H

#include "zr_vm_common/zr_common_conf.h"

struct SZrState;

#define ZR_VM_AOT_ABI_VERSION 2u

typedef enum EZrAotBackendKind {
    ZR_AOT_BACKEND_KIND_NONE = 0,
    ZR_AOT_BACKEND_KIND_C = 1,
    ZR_AOT_BACKEND_KIND_LLVM = 2
} EZrAotBackendKind;

typedef enum EZrAotInputKind {
    ZR_AOT_INPUT_KIND_NONE = 0,
    ZR_AOT_INPUT_KIND_SOURCE = 1,
    ZR_AOT_INPUT_KIND_BINARY = 2
} EZrAotInputKind;

typedef TZrInt64 (*FZrAotEntryThunk)(struct SZrState *state);

typedef struct ZrAotCompiledModule {
    TZrUInt32 abiVersion;
    TZrUInt32 backendKind;
    const TZrChar *moduleName;
    TZrUInt32 inputKind;
    const TZrChar *inputHash;
    const TZrChar *const *runtimeContracts;
    const TZrByte *embeddedModuleBlob;
    TZrSize embeddedModuleBlobLength;
    const FZrAotEntryThunk *functionThunks;
    TZrUInt32 functionThunkCount;
    FZrAotEntryThunk entryThunk;
} ZrAotCompiledModule;

typedef const ZrAotCompiledModule *(*FZrVmGetAotCompiledModule)(void);

#if defined(ZR_PLATFORM_WIN)
#define ZR_VM_AOT_EXPORT __declspec(dllexport)
#else
#define ZR_VM_AOT_EXPORT __attribute__((visibility("default")))
#endif

#endif
