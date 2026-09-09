#ifndef ZR_VM_TEST_LSP_COMPILE_TOOL_PROJECTION_CASES_H
#define ZR_VM_TEST_LSP_COMPILE_TOOL_PROJECTION_CASES_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/module/lsp_compile_tool_projection.h"

static const ZrLibTypeDescriptor *lsp_compile_tool_find_type(
        const ZrLibModuleDescriptor *module,
        const TZrChar *name) {
    for (TZrSize index = 0U; module != ZR_NULL && index < module->typeCount; index++) {
        if (module->types[index].name != ZR_NULL &&
            strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return ZR_NULL;
}

static TZrBool lsp_compile_tool_has_field(
        const ZrLibTypeDescriptor *type,
        const TZrChar *name,
        const TZrChar *typeName) {
    for (TZrSize index = 0U; type != ZR_NULL && index < type->fieldCount; index++) {
        if (type->fields[index].name != ZR_NULL &&
            type->fields[index].typeName != ZR_NULL &&
            strcmp(type->fields[index].name, name) == 0 &&
            strcmp(type->fields[index].typeName, typeName) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_lsp_compile_tool_projection_uses_canonical_contract(
        SZrState *state) {
    SZrTestTimer timer;
    const SZrParserCompileToolModuleDescriptor *canonical;
    const ZrLibModuleDescriptor *projection;
    const ZrLibModuleDescriptor *resolved = ZR_NULL;
    const ZrLibTypeDescriptor *generatedField;
    const ZrLibTypeDescriptor *diagnostic;
    const ZrLibTypeDescriptor *attributeData;
    SZrString *uri;
    SZrString *rendered = ZR_NULL;
    EZrLspImportedModuleSourceKind sourceKind =
            ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *renderedText;

    TEST_START("LSP CompileTool Projection Uses Canonical Contract");
    canonical = ZrParser_CompileTool_FindModule(
            ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION);
    projection = ZrLanguageServer_LspCompileToolProjection_FindModule(
            ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION);
    uri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(
            state, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION);
    if (canonical == ZR_NULL || projection == ZR_NULL || uri == ZR_NULL ||
        canonical->providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL ||
        projection->providerPhase != canonical->providerPhase ||
        projection->publicContractHash == ZR_NULL ||
        canonical->publicContractHash == ZR_NULL ||
        strcmp(projection->publicContractHash, canonical->publicContractHash) != 0 ||
        !ZrLanguageServer_LspCompileToolProjection_MatchesCanonical(
                projection, canonical) ||
        !ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(
                state,
                ZR_NULL,
                ZR_NULL,
                uri,
                &resolved,
                &sourceKind,
                moduleName,
                sizeof(moduleName)) ||
        resolved != projection ||
        sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_COMPILE_TOOL ||
        strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) != 0) {
        TEST_FAIL(timer,
                  "LSP CompileTool Projection Uses Canonical Contract",
                  "CompileTool projection did not preserve canonical owner, phase, or descriptor identity");
        return;
    }

    generatedField = lsp_compile_tool_find_type(projection, "GeneratedField");
    diagnostic = lsp_compile_tool_find_type(projection, "CompileDiagnostic");
    attributeData = lsp_compile_tool_find_type(projection, "AttributeData");
    if (!lsp_compile_tool_has_field(generatedField, "type", "TypeId") ||
        !lsp_compile_tool_has_field(generatedField, "initializer", "ConstantValue?") ||
        !lsp_compile_tool_has_field(diagnostic, "target", "SymbolId") ||
        !lsp_compile_tool_has_field(attributeData, "fieldValues", "ConstantValue[]") ||
        !ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(
                state, projection, uri, &rendered)) {
        TEST_FAIL(timer,
                  "LSP CompileTool Projection Uses Canonical Contract",
                  "Typed declaration constructors were absent from the virtual projection");
        return;
    }
    renderedText = rendered != ZR_NULL
                           ? ZrCore_String_GetNativeString(rendered)
                           : ZR_NULL;
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "pub struct GeneratedField") == ZR_NULL ||
        strstr(renderedText, "pub var type: TypeId;") == ZR_NULL ||
        strstr(renderedText, "pub struct CompileDiagnostic") == ZR_NULL ||
        strstr(renderedText, "%compileTime") != ZR_NULL ||
        strstr(renderedText, "%func") != ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP CompileTool Projection Uses Canonical Contract",
                  "Virtual CompileTool source was incomplete or exposed legacy percent syntax");
        return;
    }
    TEST_PASS(timer, "LSP CompileTool Projection Uses Canonical Contract");
}

#endif
