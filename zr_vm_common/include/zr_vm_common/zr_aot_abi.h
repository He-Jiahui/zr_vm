//
// Native AOT module ABI shared by generated backends and the runtime loader.
//

#ifndef ZR_VM_COMMON_ZR_AOT_ABI_H
#define ZR_VM_COMMON_ZR_AOT_ABI_H

#include "zr_vm_common/zr_common_conf.h"

struct SZrFunction;
struct SZrState;
struct SZrTypeValue;
struct SZrAotMethodInfo;
struct SZrAotGcRootMap;
struct SZrTypeLayout;

#define ZR_VM_AOT_ABI_VERSION 10u

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

typedef enum EZrAotReflectionMetadataLevel {
    ZR_AOT_REFLECTION_METADATA_NONE = 0,
    ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING = 1,
    ZR_AOT_REFLECTION_METADATA_DESCRIPTION = 2
} EZrAotReflectionMetadataLevel;

typedef TZrInt64 (*FZrAotEntryThunk)(struct SZrState *state);

typedef void (*FZrAotReflectionInvoker)(struct SZrState *state,
                                        FZrAotEntryThunk target,
                                        const struct SZrAotMethodInfo *method,
                                        struct SZrTypeValue *self,
                                        struct SZrTypeValue *args,
                                        struct SZrTypeValue *outReturn);

typedef enum EZrAotGenericSlotKind {
    ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT = 0,
    ZR_AOT_GENERIC_SLOT_PROTOTYPE = 1,
    ZR_AOT_GENERIC_SLOT_METHOD = 2,
    ZR_AOT_GENERIC_SLOT_BOX_TYPE = 3,
    ZR_AOT_GENERIC_SLOT_SIZEOF = 4
} EZrAotGenericSlotKind;

typedef union SZrAotGenericResolvedValue {
    const struct SZrTypeLayout *typeLayout;
    const void *prototype;
    FZrAotEntryThunk method;
    TZrSize sizeOfValue;
    const void *pointer;
} SZrAotGenericResolvedValue;

typedef struct SZrAotGenericSlot {
    TZrUInt32 kind;
    TZrUInt32 typeLayoutId;
    TZrUInt32 metadataToken;
    TZrUInt32 methodIndex;
    TZrUInt32 flags;
    const TZrChar *debugName;
    const struct SZrTypeLayout *staticTypeLayout;
    FZrAotEntryThunk staticMethod;
} SZrAotGenericSlot;

typedef struct SZrAotGenericResolvedSlot {
    TZrUInt32 kind;
    TZrUInt8 isResolved;
    TZrUInt8 reserved0;
    TZrUInt8 reserved1;
    TZrUInt8 reserved2;
    SZrAotGenericResolvedValue value;
} SZrAotGenericResolvedSlot;

typedef struct SZrAotGenericDictionary {
    TZrUInt32 slotCount;
    const SZrAotGenericSlot *slots;
    SZrAotGenericResolvedSlot *resolvedSlots;
} SZrAotGenericDictionary;

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

typedef enum EZrAotGcRootLocationKind {
    ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET = 0,
    ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS = 1
} EZrAotGcRootLocationKind;

typedef struct SZrAotGcRootSlot {
    TZrUInt32 stackSlot;
    TZrUInt32 frameByteOffset;
    TZrUInt32 typeLayoutId;
    TZrUInt32 fieldByteOffset;
    TZrUInt8 locationKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrAotGcRootSlot;

typedef struct SZrAotGcRootMap {
    TZrUInt32 rootCount;
    const SZrAotGcRootSlot *roots;
} SZrAotGcRootMap;

typedef struct SZrAotMethodInfo {
    TZrUInt32 functionIndex;
    const struct SZrFunction *metadataFunction;
    TZrUInt32 registerFrameBytes;
    const struct SZrAotGcRootMap *gcRootMap;
    const SZrAotSignature *signature;
    const SZrAotGenericDictionary *genericDictionary;
    FZrAotReflectionInvoker invoker;
    TZrUInt8 observationPolicy;
    TZrUInt8 reflectionMetadataLevel;
    TZrUInt8 reserved0;
    TZrUInt8 reserved1;
} SZrAotMethodInfo;

typedef struct SZrAotGcDescriptor {
    TZrUInt32 typeLayoutId;
    TZrUInt32 gcFieldCount;
    const TZrUInt32 *gcFieldOffsets;
} SZrAotGcDescriptor;

typedef struct SZrAotCodeRegistration {
    TZrUInt32 functionCount;
    const FZrAotEntryThunk *functionPointers;
    const SZrAotMethodInfo *const *methodInfos;
    TZrUInt32 methodInfoCount;
    const FZrAotReflectionInvoker *invokers;
    TZrUInt32 invokerCount;
    const struct SZrTypeLayout *const *typeLayouts;
    TZrUInt32 typeLayoutCount;
    const TZrUInt32 *typeLayoutTokens;
    TZrUInt32 typeLayoutTokenCount;
    const SZrAotGcDescriptor *const *gcDescriptors;
    TZrUInt32 gcDescriptorCount;
} SZrAotCodeRegistration;

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
    const struct SZrTypeLayout *const *typeLayouts;
    TZrUInt32 typeLayoutCount;
    const TZrUInt32 *typeLayoutTokens;
    TZrUInt32 typeLayoutTokenCount;
    const SZrAotGcDescriptor *const *gcDescriptors;
    TZrUInt32 gcDescriptorCount;
    const SZrAotCodeRegistration *codeRegistration;
} ZrAotCompiledModule;

typedef const ZrAotCompiledModule *(*FZrVmGetAotCompiledModule)(void);

#if defined(ZR_PLATFORM_WIN)
#define ZR_VM_AOT_EXPORT __declspec(dllexport)
#else
#define ZR_VM_AOT_EXPORT __attribute__((visibility("default")))
#endif

#endif
