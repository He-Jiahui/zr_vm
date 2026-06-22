//
// Native AOT module ABI shared by generated backends and the runtime loader.
//

#ifndef ZR_VM_COMMON_ZR_AOT_ABI_H
#define ZR_VM_COMMON_ZR_AOT_ABI_H

#include "zr_vm_common/zr_common_conf.h"

struct SZrFunction;
struct SZrState;
struct SZrAotGcRootMap;

#define ZR_VM_AOT_ABI_VERSION 3u

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

typedef struct SZrAotSignatureType {
    TZrUInt16 baseType;
    TZrUInt16 staticCType;
    TZrUInt32 staticCTypeId;
    TZrUInt32 ownershipQualifier;
    TZrUInt16 elementBaseType;
    TZrUInt8 isNullable;
    TZrUInt8 isArray;
} SZrAotSignatureType;

typedef struct SZrAotSignature {
    TZrUInt32 parameterCount;
    const SZrAotSignatureType *returnType;
    const SZrAotSignatureType *parameterTypes;
    TZrUInt8 hasReturnValue;
    TZrUInt8 hasVarArgs;
} SZrAotSignature;

typedef struct SZrAotMethodInfo {
    TZrUInt32 functionIndex;
    const struct SZrFunction *metadataFunction;
    TZrUInt32 registerFrameBytes;
    const struct SZrAotGcRootMap *gcRootMap;
    const SZrAotSignature *signature;
    TZrUInt8 observationPolicy;
} SZrAotMethodInfo;

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
    const SZrAotMethodInfo *const *methodInfos;
    TZrUInt32 methodInfoCount;
} ZrAotCompiledModule;

typedef const ZrAotCompiledModule *(*FZrVmGetAotCompiledModule)(void);

#if defined(ZR_PLATFORM_WIN)
#define ZR_VM_AOT_EXPORT __declspec(dllexport)
#else
#define ZR_VM_AOT_EXPORT __attribute__((visibility("default")))
#endif

#endif
