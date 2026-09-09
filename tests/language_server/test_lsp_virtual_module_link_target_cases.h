#ifndef ZR_TESTS_LSP_VIRTUAL_MODULE_LINK_TARGET_CASES_H
#define ZR_TESTS_LSP_VIRTUAL_MODULE_LINK_TARGET_CASES_H

static void test_virtual_module_link_requires_exact_target(SZrState *state, TZrBool missing) {
    static const ZrLibModuleLinkDescriptor links[] = {
        {.name = "math", .moduleName = "zr.math"},
        {.name = "missing", .moduleName = "zr.test.unregistered.target"}
    };
    static const ZrLibModuleDescriptor descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.test.virtual.links",
        .moduleLinks = links,
        .moduleLinkCount = 2U
    };
    const TZrChar *summary = missing
            ? "LSP Virtual Module Link Rejects Missing Target"
            : "LSP Virtual Module Link Selects Target Declaration";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrString *text = ZR_NULL;
    SZrString *targetText = ZR_NULL;
    SZrLspPosition position;
    SZrLspPosition targetPosition;
    SZrArray definitions = {0};
    SZrLspLocation *location;
    TZrBool resolved;
    TZrBool passed = ZR_FALSE;
    const TZrChar *failure = "registered module-link descriptor must render";

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, descriptor.moduleName);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLibrary_NativeRegistry_RegisterModule(state->global, &descriptor) ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &text) ||
        text == ZR_NULL ||
        !find_position(ZrCore_String_GetNativeString(text),
                missing ? "pub module missing" : "pub module math", 0U, 11, &position)) {
        goto cleanup;
    }
    resolved = ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, &definitions);
    if (missing) {
        failure = "a module link without a descriptor must not publish a fabricated target URI";
        passed = !resolved && definitions.length == 0U;
        goto cleanup;
    }
    failure = "a valid module link must select the target module identifier";
    if (!resolved || definitions.length != 1U) {
        goto cleanup;
    }
    location = *(SZrLspLocation **)ZrCore_Array_Get(&definitions, 0U);
    if (location == ZR_NULL || location->uri == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(location->uri), "zr-decompiled:/zr.math.zr") != 0 ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, location->uri, &targetText) ||
        targetText == ZR_NULL ||
        !find_position(ZrCore_String_GetNativeString(targetText),
                "native extern(\"zr.math", 0U, 15, &targetPosition)) {
        goto cleanup;
    }
    passed = location->range.start.line == targetPosition.line &&
            location->range.start.character == targetPosition.character &&
            location->range.end.line == targetPosition.line &&
            location->range.end.character == targetPosition.character + 7;
    if (!passed) {
        fprintf(stderr, "virtual link target: actual=(%d,%d)-(%d,%d), expected=(%d,%d)+7\n",
                location->range.start.line, location->range.start.character,
                location->range.end.line, location->range.end.character,
                targetPosition.line, targetPosition.character);
    }

cleanup:
    free_local_reference_projection_results(state, &definitions, ZR_NULL);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}

#endif
