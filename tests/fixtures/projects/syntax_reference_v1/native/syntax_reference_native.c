#include "zr_vm_library/native_binding.h"

static const ZrLibFunctionDescriptor kSyntaxReferenceFunctions[] = {
    {
        .name = "value",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = ZR_NULL,
        .returnTypeName = "int",
        .documentation = "Returns the syntax reference native checksum value.",
    },
};

static const ZrLibModuleDescriptor kSyntaxReferenceModule = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "engine.render",
    .functions = kSyntaxReferenceFunctions,
    .functionCount = sizeof(kSyntaxReferenceFunctions) /
                     sizeof(kSyntaxReferenceFunctions[0]),
    .documentation = "RegisteredNative provider for Syntax Reference v1.",
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
    .publicContractHash = "engine.render:v1:value-int",
};

const ZrLibModuleDescriptor *ZrVm_GetSyntaxReferenceRenderModule_v1(void);

const ZrLibModuleDescriptor *ZrVm_GetSyntaxReferenceRenderModule_v1(void) {
    return &kSyntaxReferenceModule;
}
