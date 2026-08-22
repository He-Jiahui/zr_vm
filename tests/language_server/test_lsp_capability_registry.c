//
// Capability-registry contract tests.
//

#include <stdio.h>
#include <string.h>

#include "zr_vm_language_server/lsp_capability_registry.h"

static int g_failures = 0;

static void expect_true(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        printf("Fail - %s\n", message);
        g_failures++;
    }
}

static void test_registry_descriptors_are_complete(void) {
    TZrSize index;

    expect_true(ZrLanguageServer_LspCapabilityRegistry_Count() == 33U,
                "capability registry must cover every currently declared initialize capability");
    for (index = 0; index < ZrLanguageServer_LspCapabilityRegistry_Count(); index++) {
        const SZrLspCapabilityDescriptor *descriptor =
                ZrLanguageServer_LspCapabilityRegistry_At(index);
        expect_true(descriptor != ZR_NULL, "registry entries must be addressable by index");
        if (descriptor == ZR_NULL) {
            continue;
        }
        expect_true(descriptor->capabilityKey != ZR_NULL && descriptor->capabilityKey[0] != '\0',
                    "every public capability needs a capability key");
        expect_true(descriptor->method != ZR_NULL && descriptor->method[0] != '\0',
                    "every public capability needs a protocol method");
        expect_true(descriptor->clientCapabilityPath != ZR_NULL && descriptor->clientCapabilityPath[0] != '\0',
                    "every public capability needs a client capability path");
        expect_true(descriptor->coreEntryPoint != ZR_NULL && descriptor->coreEntryPoint[0] != '\0',
                    "every public capability needs a core entry point");
        expect_true(descriptor->nativeAdapter != ZR_NULL && descriptor->nativeAdapter[0] != '\0',
                    "every public capability needs a native adapter");
        expect_true(descriptor->wasmExport != ZR_NULL && descriptor->wasmExport[0] != '\0',
                    "every public capability needs a WASM export contract");
        expect_true(descriptor->testId != ZR_NULL && descriptor->testId[0] != '\0',
                    "every public capability needs a protocol test id");
        expect_true((descriptor->runtimeMask & ZR_LSP_RUNTIME_NATIVE) != 0U,
                    "current initialize capabilities must declare native coverage");
        expect_true(ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(descriptor),
                    "registry descriptors must be structurally complete");
    }

    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("hoverProvider") != ZR_NULL,
                "hover provider must be discoverable by capability key");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("missingProvider") == ZR_NULL,
                "unknown capability keys must fail closed");
}

static void test_registry_rejects_invalid_contracts(void) {
    SZrLspCapabilityDescriptor descriptor = {
            "syntheticProvider",
            "textDocument/synthetic",
            "textDocument.synthetic",
            "SyntheticCoreEntry",
            "SyntheticNativeAdapter",
            "SyntheticWasmExport",
            "language_server_lsp_capability_registry",
            ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM,
            3U,
            17U,
            ZR_TRUE,
            ZR_FALSE,
            ZR_LSP_CAPABILITY_RESOLVE_IDENTITY,
    };

    expect_true(!ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(&descriptor),
                "identity-only resolve contracts must be rejected");

    descriptor.hasResolve = ZR_FALSE;
    descriptor.resolveBehavior = ZR_LSP_CAPABILITY_RESOLVE_NONE;
    descriptor.wasmExport = ZR_NULL;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "WASM capability declarations require an export contract");

    descriptor.wasmExport = "SyntheticWasmExport";
    descriptor.minimumMinor = 18U;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(&descriptor),
                "LSP 3.18 capability declarations must be experimental");

    descriptor.isExperimental = ZR_TRUE;
    expect_true(ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(&descriptor),
                "fully declared experimental 3.18 capability must be accepted");

    descriptor.testId = ZR_NULL;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "public capability declarations require a protocol test id");
}

int main(void) {
    test_registry_descriptors_are_complete();
    test_registry_rejects_invalid_contracts();

    if (g_failures != 0) {
        printf("Fail - LSP capability registry: %d failures\n", g_failures);
        return 1;
    }

    printf("Pass - LSP capability registry\n");
    return 0;
}
