#ifndef ZR_VM_TEST_LSP_VIRTUAL_DECLARATION_PROJECTION_CASES_H
#define ZR_VM_TEST_LSP_VIRTUAL_DECLARATION_PROJECTION_CASES_H

#include "zr_vm_library/native_registry.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.h"

static void test_native_virtual_definition_selects_rendered_declaration(
        SZrState *state, TZrSize caseIndex) {
    const TZrChar *summaries[] = {
        "LSP Native Function Definition Selects Rendered Declaration",
        "LSP Native Constant Definition Selects Rendered Declaration",
        "LSP Native Module Definition Selects Rendered Declaration",
        "LSP Native Type Definition Selects Rendered Declaration"
    };
    const TZrChar *names[] = {"sqrt", "PI", "zr.math", "Vector2"};
    const TZrChar *declarationPrefixes[] = {
        "pub sqrt", "pub const PI", "native extern(\"zr.math", "struct Vector2"
    };
    const TZrInt32 prefixLengths[] = {4, 10, 15, 7};
    const TZrChar *content =
            "var math = import(\"zr.math\");\n"
            "var value = math.sqrt(math.PI);\n"
            "var mathType = math.Vector2;\n"
            "return value;\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrString *rendered = ZR_NULL;
    SZrArray definitions = {0};
    SZrLspPosition usePosition;
    SZrLspPosition declarationPosition;
    SZrLspLocation *location;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrAstNode *savedAst = ZR_NULL;
    const TZrChar *failure = "native source fixture must analyze";
    TZrBool passed = ZR_FALSE;

    TEST_START(summaries[caseIndex]);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, "file:///virtual_declaration_projection.zr",
            strlen("file:///virtual_declaration_projection.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, names[caseIndex], 0U, 0, &usePosition)) {
        goto cleanup;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        goto cleanup;
    }
    savedAst = analyzer->ast;
    analyzer->ast = ZR_NULL;
    failure = "canonical native identity must yield one virtual definition without an AST";
    if (!ZrLanguageServer_Lsp_GetDefinition(
                state, context, uri, usePosition, &definitions) || definitions.length != 1U) {
        goto cleanup;
    }
    location = *(SZrLspLocation **)ZrCore_Array_Get(&definitions, 0U);
    if (location == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(location->uri) ||
        strcmp(ZrCore_String_GetNativeString(location->uri), "zr-decompiled:/zr.math.zr") != 0 ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(
                state, context, location->uri, &rendered) || rendered == ZR_NULL ||
        !find_position(ZrCore_String_GetNativeString(rendered),
                declarationPrefixes[caseIndex], 0U, prefixLengths[caseIndex],
                &declarationPosition)) {
        goto cleanup;
    }
    failure = "definition range must select its rendered identifier, not the module entry";
    if (location->range.start.line != declarationPosition.line ||
        location->range.start.character != declarationPosition.character ||
        location->range.end.line != declarationPosition.line ||
        location->range.end.character != declarationPosition.character +
                (TZrInt32)strlen(names[caseIndex])) {
        fprintf(stderr, "virtual definition %s: actual=(%d,%d)-(%d,%d), expected=(%d,%d)+%u\n",
                names[caseIndex], location->range.start.line, location->range.start.character,
                location->range.end.line, location->range.end.character,
                declarationPosition.line, declarationPosition.character,
                (unsigned int)strlen(names[caseIndex]));
        goto cleanup;
    }
    passed = ZR_TRUE;

cleanup:
    if (analyzer != ZR_NULL && savedAst != ZR_NULL) {
        analyzer->ast = savedAst;
    }
    free_local_reference_projection_results(state, &definitions, ZR_NULL);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, summaries[caseIndex]);
    } else {
        TEST_FAIL(timer, summaries[caseIndex], failure);
    }
}

static void test_native_declaration_projection_preserves_descriptor_identity(SZrState *state) {
    static const ZrLibFunctionDescriptor functions[] = {
        {.name = "\xF0\x9D\x92\x9C" "overload", .returnTypeName = "int"},
        {.name = "\xF0\x9D\x92\x9C" "overload", .returnTypeName = "float"}
    };
    static const ZrLibModuleDescriptor descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.test.declaration.identity",
        .functions = functions,
        .functionCount = 2U
    };
    ZrLibFunctionDescriptor unrelated = functions[0];
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrString *rendered = ZR_NULL;
    SZrFileRange ranges[2];
    SZrFileRange rejected;
    SZrLspRange lspRange;
    TZrBool passed = ZR_FALSE;
    const TZrChar *failure = "descriptor fixture must render its virtual declaration";

    TEST_START("LSP Native Declaration Projection Preserves Descriptor Identity And UTF16");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, descriptor.moduleName);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLibrary_NativeRegistry_RegisterModule(state->global, &descriptor) ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &rendered) ||
        rendered == ZR_NULL) {
        goto cleanup;
    }
    failure = "same-named descriptor rows must select their distinct rendered ranges";
    for (TZrSize iteration = 0U; iteration < 32U; iteration++) {
        for (TZrSize index = 0U; index < 2U; index++) {
            if (!ZrLanguageServer_LspNativeDeclarationProjection_Find(
                        state, &descriptor, uri, ZR_LSP_VIRTUAL_DECLARATION_FUNCTION,
                        &functions[index], &ranges[index]) ||
                ranges[index].start.line != (TZrInt32)index + 2 ||
                ranges[index].start.column != 9 ||
                ranges[index].end.offset - ranges[index].start.offset != strlen(functions[index].name) ||
                strncmp(ZrCore_String_GetNativeString(rendered) + ranges[index].start.offset,
                        functions[index].name, strlen(functions[index].name)) != 0) {
                goto cleanup;
            }
            lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, ranges[index]);
            if (lspRange.start.line != (TZrInt32)index + 1 ||
                lspRange.end.line != (TZrInt32)index + 1 ||
                lspRange.start.character != 8 || lspRange.end.character != 18) {
                failure = "unopened virtual document coordinates must use UTF16 columns";
                goto cleanup;
            }
        }
    }
    failure = "unrelated or wrong-kind identities must not fabricate a declaration";
    rejected = ranges[0];
    if (ZrLanguageServer_LspNativeDeclarationProjection_Find(
                state, &descriptor, uri, ZR_LSP_VIRTUAL_DECLARATION_FUNCTION,
                &unrelated, &rejected) || rejected.source != ZR_NULL ||
        rejected.start.offset != 0U || rejected.end.offset != 0U ||
        ZrLanguageServer_LspNativeDeclarationProjection_Find(
                state, &descriptor, uri, ZR_LSP_VIRTUAL_DECLARATION_CONSTANT,
                &functions[0], &rejected) ||
        ZrLanguageServer_LspNativeDeclarationProjection_Find(
                state, &descriptor, uri, ZR_LSP_VIRTUAL_DECLARATION_FUNCTION,
                ZR_NULL, &rejected)) {
        goto cleanup;
    }
    passed = ZR_TRUE;

cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, "LSP Native Declaration Projection Preserves Descriptor Identity And UTF16");
    } else {
        TEST_FAIL(timer, "LSP Native Declaration Projection Preserves Descriptor Identity And UTF16", failure);
    }
}

#endif
