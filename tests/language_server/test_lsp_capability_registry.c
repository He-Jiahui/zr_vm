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
            ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM,
    };

    expect_true(!ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(&descriptor),
                "identity-only resolve contracts must be rejected");

    descriptor.hasResolve = ZR_FALSE;
    descriptor.resolveBehavior = ZR_LSP_CAPABILITY_RESOLVE_NONE;
    descriptor.resolveRuntimeMask = 0U;
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

static void test_registry_resolve_contracts_match_implemented_behavior(void) {
    const TZrChar *completeProviders[] = {
            "workspaceSymbolProvider",
            "inlayHintProvider",
            "documentLinkProvider",
            "codeLensProvider",
    };
    TZrSize index;
    const SZrLspCapabilityDescriptor *codeAction;

    for (index = 0; index < sizeof(completeProviders) / sizeof(completeProviders[0]); index++) {
        const SZrLspCapabilityDescriptor *descriptor =
                ZrLanguageServer_LspCapabilityRegistry_Find(completeProviders[index]);
        expect_true(descriptor != ZR_NULL, "complete initial-response providers must remain registered");
        if (descriptor == ZR_NULL) {
            continue;
        }
        expect_true(!descriptor->hasResolve, descriptor->capabilityKey);
        expect_true(descriptor->resolveBehavior == ZR_LSP_CAPABILITY_RESOLVE_NONE,
                    "complete initial-response providers must not publish identity resolve");
        expect_true(descriptor->resolveRuntimeMask == 0U,
                    "withdrawn identity resolve must have no supported runtime");
        expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                            descriptor->capabilityKey, ZR_LSP_RUNTIME_NATIVE) &&
                            !ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                    descriptor->capabilityKey, ZR_LSP_RUNTIME_WASM),
                    "withdrawn identity resolve must not be published by either runtime");
        expect_true(ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(descriptor),
                    "withdrawing identity resolve must keep the base provider publishable");
    }

    codeAction = ZrLanguageServer_LspCapabilityRegistry_Find("codeActionProvider");
    expect_true(codeAction != ZR_NULL, "code actions must remain registered");
    if (codeAction != ZR_NULL) {
        expect_true(codeAction->hasResolve &&
                            codeAction->resolveBehavior == ZR_LSP_CAPABILITY_RESOLVE_MATERIAL,
                    "native code action resolve must describe its snapshot revalidation");
        expect_true(ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(codeAction),
                    "material code action resolve must remain publishable");
    }
}

static void test_registry_material_resolve_is_native_only(void) {
    const TZrChar *providers[] = {"completionProvider", "codeActionProvider"};
    TZrSize index;

    for (index = 0; index < sizeof(providers) / sizeof(providers[0]); index++) {
        const SZrLspCapabilityDescriptor *descriptor =
                ZrLanguageServer_LspCapabilityRegistry_Find(providers[index]);
        expect_true(descriptor != ZR_NULL, "material resolve providers must be registered");
        if (descriptor == ZR_NULL) {
            continue;
        }
        expect_true(descriptor->runtimeMask == (ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM),
                    "native-only resolve must preserve both base provider runtimes");
        expect_true(descriptor->resolveRuntimeMask == ZR_LSP_RUNTIME_NATIVE,
                    "completion and code action resolve are implemented only by the native adapter");
        expect_true(ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                            providers[index], ZR_LSP_RUNTIME_NATIVE),
                    "native material resolve must remain available");
        expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                            providers[index], ZR_LSP_RUNTIME_WASM),
                    "native material resolve must not overclaim WASM support");
    }
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                        "missingProvider", ZR_LSP_RUNTIME_NATIVE),
                "unknown providers must not publish resolve");
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                        "completionProvider", (EZrLspRuntimeMask)0U) &&
                        !ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                "completionProvider", (EZrLspRuntimeMask)4U),
                "invalid runtime queries must fail closed");
}

static void test_registry_rejects_invalid_resolve_runtime_contracts(void) {
    const SZrLspCapabilityDescriptor *completion =
            ZrLanguageServer_LspCapabilityRegistry_Find("completionProvider");
    SZrLspCapabilityDescriptor descriptor;

    expect_true(completion != ZR_NULL, "completion descriptor is needed for runtime contract checks");
    if (completion == ZR_NULL) {
        return;
    }
    descriptor = *completion;
    descriptor.resolveRuntimeMask = 0U;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "resolve declarations must name at least one supported runtime");
    descriptor.runtimeMask = ZR_LSP_RUNTIME_NATIVE;
    descriptor.resolveRuntimeMask = ZR_LSP_RUNTIME_WASM;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "resolve runtimes must be a subset of base provider runtimes");
    descriptor.resolveRuntimeMask = ZR_LSP_RUNTIME_NATIVE | 4U;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "unknown resolve runtime bits must be rejected");
    descriptor.resolveRuntimeMask = ZR_LSP_RUNTIME_NATIVE;
    descriptor.hasResolve = ZR_FALSE;
    descriptor.resolveBehavior = ZR_LSP_CAPABILITY_RESOLVE_NONE;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "providers without resolve must have an empty resolve runtime mask");
    descriptor.hasResolve = ZR_TRUE;
    descriptor.resolveBehavior = (EZrLspCapabilityResolveBehavior)99;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(&descriptor),
                "only material resolve behavior may be published");
}

int main(void) {
    test_registry_descriptors_are_complete();
    test_registry_rejects_invalid_contracts();
    test_registry_resolve_contracts_match_implemented_behavior();
    test_registry_material_resolve_is_native_only();
    test_registry_rejects_invalid_resolve_runtime_contracts();

    if (g_failures != 0) {
        printf("Fail - LSP capability registry: %d failures\n", g_failures);
        return 1;
    }

    printf("Pass - LSP capability registry\n");
    return 0;
}
