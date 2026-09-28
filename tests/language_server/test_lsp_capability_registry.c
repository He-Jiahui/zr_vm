//
// Capability-registry contract tests.
//

#include <stdio.h>
#include <string.h>

#include "zr_vm_language_server/lsp_capability_registry.h"

/* 全部契约断言共享一个失败计数，main 将其转换为 CTest 可见的退出状态。 */
static int g_failures = 0;

/* 继续执行同一批的其他契约断言，使单项漂移不会掩盖其余能力的差异。 */
static void expect_true(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        printf("Fail - %s\n", message);
        g_failures++;
    }
}

/* 对静态描述符的可选入口字段同时比较缺席状态和文本，避免空串冒充不存在的实现。 */
static void expect_metadata_string(const TZrChar *actual,
                                   const TZrChar *expected,
                                   const TZrChar *capabilityKey,
                                   const TZrChar *field) {
    if ((actual == ZR_NULL) != (expected == ZR_NULL) ||
        (actual != ZR_NULL && expected != ZR_NULL && strcmp(actual, expected) != 0)) {
        printf("Fail - %s %s: expected %s, got %s\n",
               capabilityKey, field,
               expected != ZR_NULL ? expected : "<null>",
               actual != ZR_NULL ? actual : "<null>");
        g_failures++;
    }
}

/* 把注册表公开的能力清单与人工核对的核心、native 和 Web 入口逐项对齐。
 * 本测试只检查静态描述符；stdio 协议清单探针另核对真实 initialize 响应与路由，
 * Web 链接产物的可调用性只在提供 JS/WASM 产物时由独立探针验证。 */
static void test_registry_metadata_matches_current_implementations(void) {
    /* 每行绑定能力键、入口、Web 导出和协议测试名；它是静态契约样本，不负责执行这些入口。 */
    static const struct {
        const TZrChar *capabilityKey;
        const TZrChar *coreEntryPoint;
        const TZrChar *nativeAdapter;
        const TZrChar *wasmExport;
        const TZrChar *testId;
    } expected[] = {
            {"textDocumentSync", "ZrLanguageServer_Lsp_UpdateDocument", "handle_did_change",
             "wasm_ZrLspUpdateDocument", "language_server_stdio_document_sync_conformance"},
            {"positionEncoding", ZR_NULL, "handle_initialize_request", ZR_NULL,
             "language_server_stdio_position_encoding_smoke"},
            {"completionProvider", "ZrLanguageServer_Lsp_GetCompletion", "handle_completion_request",
             "wasm_ZrLspGetCompletion", "language_server_stdio_smoke"},
            {"hoverProvider", "ZrLanguageServer_Lsp_GetHover", "handle_hover_request",
             "wasm_ZrLspGetHover", "language_server_stdio_smoke"},
            {"signatureHelpProvider", "ZrLanguageServer_Lsp_GetSignatureHelp",
             "handle_signature_help_request", ZR_NULL, "language_server_stdio_smoke"},
            {"definitionProvider", "ZrLanguageServer_Lsp_GetDefinition", "handle_definition_request",
             "wasm_ZrLspGetDefinition", "language_server_stdio_navigation_capabilities_smoke"},
            {"referencesProvider", "ZrLanguageServer_Lsp_FindReferences", "handle_references_request",
             "wasm_ZrLspFindReferences", "language_server_stdio_smoke"},
            {"renameProvider", "ZrLanguageServer_Lsp_Rename", "handle_rename_request",
             "wasm_ZrLspRename", "language_server_stdio_smoke"},
            {"documentSymbolProvider", "ZrLanguageServer_Lsp_GetDocumentSymbols",
             "handle_document_symbols_request", "wasm_ZrLspGetDocumentSymbols", "language_server_stdio_smoke"},
            {"workspaceSymbolProvider", "ZrLanguageServer_Lsp_GetWorkspaceSymbols",
             "handle_workspace_symbols_request", ZR_NULL,
             "language_server_stdio_resolve_capabilities_smoke"},
            {"documentHighlightProvider", "ZrLanguageServer_Lsp_GetDocumentHighlights",
             "handle_document_highlights_request", "wasm_ZrLspGetDocumentHighlights", "language_server_stdio_smoke"},
            {"inlayHintProvider", "ZrLanguageServer_Lsp_GetInlayHints", "handle_inlay_hint_request",
             "wasm_ZrLspGetInlayHints", "language_server_stdio_resolve_capabilities_smoke"},
            {"semanticTokensProvider", "ZrLanguageServer_Lsp_GetSemanticTokens",
             "handle_semantic_tokens_full_request", "wasm_ZrLspGetSemanticTokens", "language_server_stdio_smoke"},
            {"codeActionProvider", "ZrLanguageServer_Lsp_GetCodeActions", "handle_code_action_request",
             "wasm_ZrLspGetCodeActions", "language_server_stdio_resolve_capabilities_smoke"},
            {"documentFormattingProvider", "ZrLanguageServer_Lsp_GetFormatting", "handle_formatting_request",
             "wasm_ZrLspGetFormatting", "language_server_stdio_smoke"},
            {"documentRangeFormattingProvider", "ZrLanguageServer_Lsp_GetRangeFormatting",
             "handle_range_formatting_request", "wasm_ZrLspGetRangeFormatting", "language_server_stdio_smoke"},
            {"documentOnTypeFormattingProvider", ZR_NULL, "handle_on_type_formatting_request",
             ZR_NULL, "language_server_stdio_smoke"},
            {"foldingRangeProvider", "ZrLanguageServer_Lsp_GetFoldingRanges", "handle_folding_range_request",
             "wasm_ZrLspGetFoldingRanges", "language_server_stdio_smoke"},
            {"selectionRangeProvider", "ZrLanguageServer_Lsp_GetSelectionRanges", "handle_selection_range_request",
             "wasm_ZrLspGetSelectionRange", "language_server_stdio_smoke"},
            {"linkedEditingRangeProvider", ZR_NULL, "handle_linked_editing_range_request",
             ZR_NULL, "language_server_stdio_smoke"},
            {"monikerProvider", ZR_NULL, "handle_moniker_request", ZR_NULL, "language_server_stdio_smoke"},
            {"inlineValueProvider", ZR_NULL, "handle_inline_value_request", ZR_NULL,
             "language_server_stdio_inline_value_semantic_smoke"},
            {"inlineCompletionProvider", ZR_NULL, "handle_inline_completion_request", ZR_NULL,
             "language_server_stdio_smoke"},
            {"implementationProvider", "ZrLanguageServer_Lsp_GetImplementation", "handle_implementation_request",
             ZR_NULL, "language_server_stdio_navigation_capabilities_smoke"},
            {"callHierarchyProvider", "ZrLanguageServer_Lsp_PrepareCallHierarchy",
             "handle_prepare_call_hierarchy_request", ZR_NULL, "language_server_stdio_smoke"},
            {"typeHierarchyProvider", "ZrLanguageServer_Lsp_PrepareTypeHierarchy",
             "handle_prepare_type_hierarchy_request", ZR_NULL, "language_server_stdio_type_hierarchy_smoke"},
            {"documentLinkProvider", "ZrLanguageServer_Lsp_GetDocumentLinks", "handle_document_link_request",
             "wasm_ZrLspGetDocumentLinks", "language_server_stdio_resolve_capabilities_smoke"},
            {"codeLensProvider", "ZrLanguageServer_Lsp_GetCodeLens", "handle_code_lens_request",
             "wasm_ZrLspGetCodeLens", "language_server_stdio_resolve_capabilities_smoke"},
            {"diagnosticProvider", "ZrLanguageServer_Lsp_GetDiagnostics", "handle_text_document_diagnostic_request",
             "wasm_ZrLspGetDiagnosticReport", "language_server_stdio_smoke"},
            {"workspace", ZR_NULL, "handle_did_change_workspace_folders", ZR_NULL,
             "language_server_stdio_workspace_folders_smoke"},
    };
    TZrSize index;

    expect_true(sizeof(expected) / sizeof(expected[0]) == ZrLanguageServer_LspCapabilityRegistry_Count(),
                "implementation metadata expectations must cover every registered capability");
    /* 数量一致之后还逐键验证，防止重新排序或替换某一能力时只靠总数漏检。 */
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
        const SZrLspCapabilityDescriptor *descriptor =
                ZrLanguageServer_LspCapabilityRegistry_Find(expected[index].capabilityKey);
        const TZrUInt32 expectedRuntimeMask = expected[index].wasmExport != ZR_NULL
                                                    ? ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM
                                                    : ZR_LSP_RUNTIME_NATIVE;
        const EZrLspCapabilityImplementationLayer expectedLayer = expected[index].coreEntryPoint != ZR_NULL
                ? ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE
                : ZR_LSP_CAPABILITY_IMPLEMENTATION_NATIVE_ADAPTER;
        expect_true(descriptor != ZR_NULL, expected[index].capabilityKey);
        if (descriptor == ZR_NULL) {
            continue;
        }
        expect_metadata_string(descriptor->coreEntryPoint, expected[index].coreEntryPoint,
                               descriptor->capabilityKey, "core entry point");
        expect_metadata_string(descriptor->nativeAdapter, expected[index].nativeAdapter,
                               descriptor->capabilityKey, "native adapter");
        expect_metadata_string(descriptor->wasmExport, expected[index].wasmExport,
                               descriptor->capabilityKey, "WASM export");
        expect_metadata_string(descriptor->testId, expected[index].testId,
                               descriptor->capabilityKey, "protocol test id");
        expect_true(descriptor->implementationLayer == expectedLayer,
                    "capability implementation ownership must match its core or native adapter entry");
        if (descriptor->runtimeMask != expectedRuntimeMask) {
            printf("Fail - %s runtime mask must match implemented native and WASM entry points\n",
                   descriptor->capabilityKey);
            g_failures++;
        }
    }
}

/* 3.17 默认协商不得把仅在 3.18 实验路径出现的 inline completion 宣称为稳定能力。 */
static void test_registry_inline_completion_requires_experimental_318(void) {
    const SZrLspCapabilityDescriptor *descriptor =
            ZrLanguageServer_LspCapabilityRegistry_Find("inlineCompletionProvider");

    expect_true(descriptor != ZR_NULL, "inline completion must retain its native implementation contract");
    if (descriptor == ZR_NULL) {
        return;
    }
    expect_true(descriptor->minimumMajor == 3U && descriptor->minimumMinor == 18U,
                "inline completion is an LSP 3.18 capability");
    expect_true(descriptor->isExperimental,
                "inline completion must be marked experimental in the LSP 3.17 baseline");
}

/* 没有类型化实现的 color provider 不应因文本扫描能力而进入公开能力清单。 */
static void test_registry_excludes_untyped_color_provider(void) {
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("colorProvider") == ZR_NULL,
                "untyped color scanning must not be registered as a language capability");
}

/* 枚举所有注册项并检查元数据与发布策略，随后锁定曾撤销的别名和未知键的查询边界。 */
static void test_registry_descriptors_are_complete(void) {
    TZrSize index;

    expect_true(ZrLanguageServer_LspCapabilityRegistry_Count() == 30U,
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
        if (descriptor->implementationLayer == ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE) {
            expect_true(descriptor->coreEntryPoint != ZR_NULL && descriptor->coreEntryPoint[0] != '\0',
                        "core-owned capabilities need a core entry point");
        } else {
            expect_true(descriptor->implementationLayer == ZR_LSP_CAPABILITY_IMPLEMENTATION_NATIVE_ADAPTER,
                        "capabilities must explicitly identify core or native adapter ownership");
            expect_true(descriptor->coreEntryPoint == ZR_NULL,
                        "native adapter-owned capabilities must not invent a core entry point");
        }
        expect_true(descriptor->nativeAdapter != ZR_NULL && descriptor->nativeAdapter[0] != '\0',
                    "every public capability needs a native adapter");
        if ((descriptor->runtimeMask & ZR_LSP_RUNTIME_WASM) != 0U) {
            expect_true(descriptor->wasmExport != ZR_NULL && descriptor->wasmExport[0] != '\0',
                        "WASM capabilities need a WASM export contract");
        } else {
            expect_true(descriptor->wasmExport == ZR_NULL,
                        "native-only capabilities must not invent a WASM export");
        }
        expect_true(descriptor->testId != ZR_NULL && descriptor->testId[0] != '\0',
                    "every public capability needs a protocol test id");
        expect_true((descriptor->runtimeMask & ZR_LSP_RUNTIME_NATIVE) != 0U,
                    "current initialize capabilities must declare native coverage");
        expect_true(ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(descriptor),
                    "registry descriptors must be structurally complete");
        expect_true(ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(descriptor),
                    "registered base capabilities must retain valid publication metadata");
    }

    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("hoverProvider") != ZR_NULL,
                "hover provider must be discoverable by capability key");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("definitionProvider") != ZR_NULL,
                "definition provider must retain its registered contract");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("implementationProvider") != ZR_NULL,
                "implementation provider must retain its canonical relation contract");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("declarationProvider") == ZR_NULL,
                "withdrawn declaration alias must not be registered as an available capability");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("typeDefinitionProvider") == ZR_NULL,
                "withdrawn type definition alias must not be registered as an available capability");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_Find("missingProvider") == ZR_NULL,
                "unknown capability keys must fail closed");
}

/* 合成一个描述符依次破坏 resolve、Web 导出和版本实验标记，验证静态发布判定的拒绝策略。 */
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
            ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE,
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

/* 在现有 hover 契约的副本上切换运行时位，核对每个启用后端都有对应适配入口。 */
static void test_registry_metadata_requirements_follow_runtime(void) {
    const SZrLspCapabilityDescriptor *hover = ZrLanguageServer_LspCapabilityRegistry_Find("hoverProvider");
    SZrLspCapabilityDescriptor descriptor;

    expect_true(hover != ZR_NULL, "hover descriptor is needed for runtime metadata checks");
    if (hover == ZR_NULL) {
        return;
    }
    descriptor = *hover;
    descriptor.implementationLayer = ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE;
    descriptor.runtimeMask = 0U;
    descriptor.nativeAdapter = ZR_NULL;
    descriptor.wasmExport = ZR_NULL;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "capabilities must name at least one runtime");
    descriptor.runtimeMask = ZR_LSP_RUNTIME_NATIVE | 4U;
    descriptor.nativeAdapter = hover->nativeAdapter;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "unknown base runtime bits must be rejected");

    descriptor = *hover;
    descriptor.runtimeMask = ZR_LSP_RUNTIME_NATIVE;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native-only capabilities must not claim a disabled WASM export");
    descriptor.wasmExport = "";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "disabled WASM metadata must be explicitly null, not empty");
    descriptor.wasmExport = ZR_NULL;
    expect_true(ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native-only capabilities do not require a WASM export");
    descriptor.nativeAdapter = ZR_NULL;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native capabilities require their native adapter");
    descriptor.nativeAdapter = "";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "an empty native adapter does not satisfy native metadata");

    descriptor.runtimeMask = ZR_LSP_RUNTIME_WASM;
    descriptor.nativeAdapter = ZR_NULL;
    descriptor.wasmExport = "wasm_ZrLspGetHover";
    expect_true(ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "WASM-only core capabilities do not require a native adapter");
    descriptor.nativeAdapter = "handle_hover_request";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "WASM-only capabilities must not claim a disabled native adapter");
    descriptor.nativeAdapter = "";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "disabled native metadata must be explicitly null, not empty");
    descriptor.nativeAdapter = ZR_NULL;
    descriptor.wasmExport = ZR_NULL;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "enabled WASM runtime requires an export");
    descriptor.wasmExport = "";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "an empty WASM export does not satisfy WASM metadata");
}

/* 核心实现和仅 native 适配实现必须互斥，不能借另一层入口伪造发布资格。 */
static void test_registry_metadata_requirements_follow_ownership(void) {
    const SZrLspCapabilityDescriptor *hover = ZrLanguageServer_LspCapabilityRegistry_Find("hoverProvider");
    SZrLspCapabilityDescriptor descriptor;

    expect_true(hover != ZR_NULL, "hover descriptor is needed for ownership metadata checks");
    if (hover == ZR_NULL) {
        return;
    }
    descriptor = *hover;
    descriptor.implementationLayer = (EZrLspCapabilityImplementationLayer)0;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "capabilities must explicitly declare implementation ownership");
    descriptor.implementationLayer = (EZrLspCapabilityImplementationLayer)99;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "unknown implementation ownership must be rejected");

    descriptor.implementationLayer = ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE;
    descriptor.coreEntryPoint = ZR_NULL;
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "core ownership requires a core entry point");
    descriptor.coreEntryPoint = "";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "an empty core entry point does not satisfy core ownership");

    descriptor.implementationLayer = ZR_LSP_CAPABILITY_IMPLEMENTATION_NATIVE_ADAPTER;
    descriptor.runtimeMask = ZR_LSP_RUNTIME_NATIVE;
    descriptor.coreEntryPoint = ZR_NULL;
    descriptor.wasmExport = ZR_NULL;
    expect_true(ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native adapter ownership allows capabilities without a dedicated core API");
    expect_true(ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(&descriptor),
                "complete native adapter-owned base capabilities retain publishable metadata");
    descriptor.wasmExport = "wasm_ZrLspGetHover";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native adapter ownership must not claim a disabled WASM export");
    descriptor.wasmExport = ZR_NULL;
    descriptor.coreEntryPoint = "ZrLanguageServer_Lsp_GetHover";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native adapter ownership must not also claim a core entry point");
    descriptor.coreEntryPoint = "";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native adapter ownership requires an explicitly null core entry point");
    descriptor.coreEntryPoint = ZR_NULL;
    descriptor.runtimeMask = ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM;
    descriptor.wasmExport = "wasm_ZrLspGetHover";
    expect_true(!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(&descriptor),
                "native adapter ownership cannot claim WASM coverage");
}

/* 初始响应已经完整的 provider 不应宣告 identity resolve；code action 的 resolve 复验快照，
 * 失效时移除旧 edit 并禁用操作，有效时返回原操作的深拷贝。 */
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

/* 补全与 code action 的基础能力可在 native/Web 提供，但 material resolve 目前只由 native 实现。 */
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

/* resolve 的运行时掩码必须属于基础能力，并且只有 material 行为可以作为公开 resolve。 */
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
    descriptor.wasmExport = ZR_NULL;
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

/* CMake 只在 LSP、parser、core 目标齐备时注册本可执行文件，退出码汇总整个静态契约矩阵。 */
int main(void) {
    test_registry_metadata_matches_current_implementations();
    test_registry_inline_completion_requires_experimental_318();
    test_registry_excludes_untyped_color_provider();
    test_registry_descriptors_are_complete();
    test_registry_rejects_invalid_contracts();
    test_registry_metadata_requirements_follow_runtime();
    test_registry_metadata_requirements_follow_ownership();
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
