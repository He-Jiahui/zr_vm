//
// Runtime-neutral LSP capability contract registry.
//

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CAPABILITY_REGISTRY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CAPABILITY_REGISTRY_H

#include "zr_vm_language_server/conf.h"

typedef enum EZrLspRuntimeMask {
    ZR_LSP_RUNTIME_NATIVE = 1U,
    ZR_LSP_RUNTIME_WASM = 2U,
} EZrLspRuntimeMask;

typedef enum EZrLspCapabilityResolveBehavior {
    ZR_LSP_CAPABILITY_RESOLVE_NONE = 0,
    ZR_LSP_CAPABILITY_RESOLVE_MATERIAL,
    ZR_LSP_CAPABILITY_RESOLVE_IDENTITY,
} EZrLspCapabilityResolveBehavior;

typedef struct SZrLspCapabilityDescriptor {
    const TZrChar *capabilityKey;
    const TZrChar *method;
    const TZrChar *clientCapabilityPath;
    const TZrChar *coreEntryPoint;
    const TZrChar *nativeAdapter;
    const TZrChar *wasmExport;
    const TZrChar *testId;
    TZrUInt32 runtimeMask;
    TZrUInt16 minimumMajor;
    TZrUInt16 minimumMinor;
    TZrBool hasResolve;
    TZrBool isExperimental;
    EZrLspCapabilityResolveBehavior resolveBehavior;
} SZrLspCapabilityDescriptor;

ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_LspCapabilityRegistry_Count(void);
ZR_LANGUAGE_SERVER_API const SZrLspCapabilityDescriptor *
ZrLanguageServer_LspCapabilityRegistry_At(TZrSize index);
ZR_LANGUAGE_SERVER_API const SZrLspCapabilityDescriptor *
ZrLanguageServer_LspCapabilityRegistry_Find(const TZrChar *capabilityKey);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(
        const SZrLspCapabilityDescriptor *descriptor);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(
        const SZrLspCapabilityDescriptor *descriptor);

#endif
