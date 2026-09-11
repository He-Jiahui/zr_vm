#ifndef ZR_VM_TEST_LSP_PROJECT_MODULE_SUMMARY_RANGE_CASES_H
#define ZR_VM_TEST_LSP_PROJECT_MODULE_SUMMARY_RANGE_CASES_H

static void test_lsp_project_modules_publish_exact_source_declaration_range(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrArray modules;
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool foundSourceModule = ZR_FALSE;

    TEST_START("LSP Project Modules Publish Exact Source Declaration Range");
    TEST_INFO("Project module declaration range",
              "A source module summary should point at the explicit module name token instead of the file origin placeholder.");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_ownership/lsp_ownership.zrp",
                                   projectPath,
                                   sizeof(projectPath))) {
        TEST_FAIL(timer,
                  "LSP Project Modules Publish Exact Source Declaration Range",
                  "Failed to build the source project fixture path");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    projectUri = create_file_uri_from_native_path(state, projectPath);
    if (context == ZR_NULL || projectUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Modules Publish Exact Source Declaration Range",
                  "Failed to create the LSP context or project URI");
        return;
    }

    ZrCore_Array_Init(state, &modules, sizeof(SZrLspProjectModuleSummary *), 8);
    if (!ZrLanguageServer_Lsp_GetProjectModules(state, context, projectUri, &modules)) {
        ZrLanguageServer_Lsp_FreeProjectModules(state, &modules);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Modules Publish Exact Source Declaration Range",
                  "The project modules request failed for the source fixture");
        return;
    }

    for (TZrSize index = 0; index < modules.length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(&modules, index);
        const TZrChar *moduleNameText;
        const TZrChar *navigationUriText;

        if (summaryPtr == ZR_NULL || *summaryPtr == ZR_NULL) {
            continue;
        }

        moduleNameText = test_string_ptr((*summaryPtr)->moduleName);
        if (moduleNameText == ZR_NULL ||
            strcmp(moduleNameText, "main") != 0 ||
            (*summaryPtr)->sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE) {
            continue;
        }

        foundSourceModule = ZR_TRUE;
        navigationUriText = test_string_ptr((*summaryPtr)->navigationUri);
        if (navigationUriText == ZR_NULL ||
            strstr(navigationUriText, "main.zr") == ZR_NULL ||
            (*summaryPtr)->range.start.line != 0 ||
            (*summaryPtr)->range.start.character != 7 ||
            (*summaryPtr)->range.end.line != 0 ||
            (*summaryPtr)->range.end.character != 11) {
            ZrLanguageServer_Lsp_FreeProjectModules(state, &modules);
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer,
                      "LSP Project Modules Publish Exact Source Declaration Range",
                      "The source module summary did not expose the exact `main` declaration token range");
            return;
        }
        break;
    }

    ZrLanguageServer_Lsp_FreeProjectModules(state, &modules);
    ZrLanguageServer_LspContext_Free(state, context);

    if (!foundSourceModule) {
        TEST_FAIL(timer,
                  "LSP Project Modules Publish Exact Source Declaration Range",
                  "The source project module summary was not returned");
        return;
    }

    TEST_PASS(timer, "LSP Project Modules Publish Exact Source Declaration Range");
}

#endif
