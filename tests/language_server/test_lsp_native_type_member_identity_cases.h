#ifndef ZR_TESTS_LSP_NATIVE_TYPE_MEMBER_IDENTITY_CASES_H
#define ZR_TESTS_LSP_NATIVE_TYPE_MEMBER_IDENTITY_CASES_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.h"

static void test_native_type_member_projection_preserves_descriptor_identity(
        SZrState *state, TZrBool methods, TZrBool reverse) {
    static const ZrLibFieldDescriptor fields[] = {
        {.name = "value", .typeName = "int"},
        {.name = "value", .typeName = "float"},
        {.name = "value", .typeName = "bool"},
        {.name = "value", .typeName = "string"}
    };
    static const ZrLibMethodDescriptor methodRows[] = {
        {.name = "run", .returnTypeName = "int"},
        {.name = "run", .returnTypeName = "float"},
        {.name = "run", .returnTypeName = "bool"},
        {.name = "run", .returnTypeName = "string"}
    };
    static const ZrLibTypeDescriptor types[] = {
        {.name = "Shared", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = fields, .fieldCount = 2U, .methods = methodRows, .methodCount = 2U},
        {.name = "Shared", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = fields + 2, .fieldCount = 2U, .methods = methodRows + 2, .methodCount = 2U}
    };
    static const ZrLibModuleDescriptor descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.test.type.member.identity", .types = types, .typeCount = 2U
    };
    const TZrChar *summary = reverse
            ? (methods ? "LSP Native Method Position Preserves Descriptor Identity"
                       : "LSP Native Field Position Preserves Descriptor Identity")
            : (methods ? "LSP Native Method Projection Preserves Descriptor Identity"
                       : "LSP Native Field Projection Preserves Descriptor Identity");
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrLspMetadataProvider provider;
    SZrLspResolvedMetadataMember member = {0};
    SZrLspResolvedMetadataMember selected;
    SZrString *uri;
    SZrString *rendered = ZR_NULL;
    SZrLspPosition expected;
    SZrLspRange projected;
    const TZrChar *failure = "native descriptor fixture must render";
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, descriptor.moduleName);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLibrary_NativeRegistry_RegisterModule(state->global, &descriptor) ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &rendered) ||
        rendered == ZR_NULL) {
        goto cleanup;
    }
    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    member.module.nativeDescriptor = &descriptor;
    member.module.moduleName = ZrCore_String_Create(
            state, (TZrNativeString)descriptor.moduleName, strlen(descriptor.moduleName));
    member.memberKind = methods ? ZR_LSP_METADATA_MEMBER_METHOD : ZR_LSP_METADATA_MEMBER_FIELD;
    member.memberName = ZrCore_String_Create(state, methods ? "run" : "value", methods ? 3U : 5U);
    for (TZrSize index = 0U; index < 4U; index++) {
        member.ownerTypeDescriptor = &types[index / 2U];
        member.fieldDescriptor = methods ? ZR_NULL : &fields[index];
        member.methodDescriptor = methods ? &methodRows[index] : ZR_NULL;
        failure = "selected descriptor row must determine the declaration range";
        if (!find_position(ZrCore_String_GetNativeString(rendered),
                    methods ? "pub run" : "pub var value", index, methods ? 4 : 8, &expected)) {
            goto cleanup;
        }
        if (reverse) {
            failure = "virtual document position must recover the same owner and member descriptors";
            if (!ZrLanguageServer_LspMetadataProvider_FindNativeTypeMemberDeclaration(
                    &provider, ZR_NULL, uri, expected, &selected) ||
                selected.ownerTypeDescriptor != member.ownerTypeDescriptor ||
                selected.fieldDescriptor != member.fieldDescriptor ||
                selected.methodDescriptor != member.methodDescriptor) {
                fprintf(stderr, "native type member %s row %u: reverse descriptor identity mismatch\n",
                        methods ? "method" : "field", (unsigned int)index);
                goto cleanup;
            }
        } else {
            if (!ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(
                        &provider, ZR_NULL, &member) || !member.hasDeclaration) {
                goto cleanup;
            }
            projected = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                    context, member.declarationUri, member.declarationRange);
            if (projected.start.line != expected.line || projected.start.character != expected.character ||
                projected.end.line != expected.line ||
                projected.end.character != expected.character + (methods ? 3 : 5)) {
                fprintf(stderr, "native type member %s row %u: actual=(%d,%d), expected=(%d,%d)\n",
                        methods ? "method" : "field", (unsigned int)index,
                        projected.start.line, projected.start.character, expected.line, expected.character);
                goto cleanup;
            }
        }
    }
    if (!reverse) {
        failure = "a missing descriptor must clear stale projection instead of matching member text";
        member.fieldDescriptor = ZR_NULL;
        member.methodDescriptor = ZR_NULL;
        if (ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(
                    &provider, ZR_NULL, &member) || member.hasDeclaration ||
            member.declarationRange.source != ZR_NULL) {
            goto cleanup;
        }
    }
    passed = ZR_TRUE;

cleanup:
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
