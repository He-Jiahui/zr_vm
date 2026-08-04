#ifndef ZR_VM_TEST_LSP_STABLE_SLOT_CONTRACT_CASES_H
#define ZR_VM_TEST_LSP_STABLE_SLOT_CONTRACT_CASES_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_stable_slot_contract.h"

static void test_lsp_stable_slot_contract_classification_is_name_independent(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Stable Slot Contract Classification Is Name Independent";
    SZrTestTimer timer;
    SZrTypePrototypeInfo handle = {0};
    SZrTypePrototypeInfo source = {0};
    SZrTypePrototypeInfo writableRef = {0};
    SZrTypePrototypeInfo readonlyRef = {0};
    SZrTypeMemberInfo member = {0};
    SZrLspStableSlotContract contract;
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    ZrCore_Array_Init(state, &handle.members, sizeof(SZrTypeMemberInfo), 3u);
    member.contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_POOL_ID;
    ZrCore_Array_Push(state, &handle.members, &member);
    member.contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_SLOT;
    ZrCore_Array_Push(state, &handle.members, &member);
    member.contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_HANDLE_GENERATION;
    ZrCore_Array_Push(state, &handle.members, &member);

    ZrCore_Array_Init(state, &source.members, sizeof(SZrTypeMemberInfo), 2u);
    source.protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE);
    memset(&member, 0, sizeof(member));
    member.name = ZrCore_String_CreateFromNative(state, "read_window");
    member.contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_READ;
    ZrCore_Array_Push(state, &source.members, &member);
    member.name = ZrCore_String_CreateFromNative(state, "write_window");
    member.contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_ACQUIRE_WRITE;
    ZrCore_Array_Push(state, &source.members, &member);

    ZrCore_Array_Init(state, &writableRef.members, sizeof(SZrTypeMemberInfo), 1u);
    writableRef.protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE);
    memset(&member, 0, sizeof(member));
    member.contractRole = ZR_MEMBER_CONTRACT_ROLE_POOL_REF_PROJECTION;
    member.hasStructuredReturnType = ZR_TRUE;
    member.structuredReturnType.referenceAccess = ZR_REFERENCE_ACCESS_WRITABLE;
    ZrCore_Array_Push(state, &writableRef.members, &member);

    ZrCore_Array_Init(state, &readonlyRef.members, sizeof(SZrTypeMemberInfo), 1u);
    readonlyRef.protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE);
    member.structuredReturnType.referenceAccess = ZR_REFERENCE_ACCESS_READONLY;
    ZrCore_Array_Push(state, &readonlyRef.members, &member);

    if (!ZrLanguageServer_LspStableSlotContract_Classify(&handle, &contract) ||
        contract.kind != ZR_LSP_STABLE_SLOT_CONTRACT_HANDLE ||
        !ZrLanguageServer_LspStableSlotContract_Classify(&source, &contract) ||
        contract.kind != ZR_LSP_STABLE_SLOT_CONTRACT_SOURCE ||
        contract.acquireRead == ZR_NULL || contract.acquireWrite == ZR_NULL ||
        !ZrLanguageServer_LspStableSlotContract_Classify(&writableRef, &contract) ||
        contract.kind != ZR_LSP_STABLE_SLOT_CONTRACT_WRITABLE_REF ||
        !ZrLanguageServer_LspStableSlotContract_Classify(&readonlyRef, &contract) ||
        contract.kind != ZR_LSP_STABLE_SLOT_CONTRACT_READONLY_REF) {
        TEST_FAIL(timer, summary, "Capability/role facts did not classify renamed stable-slot surfaces");
        goto cleanup;
    }
    success = ZR_TRUE;

cleanup:
    ZrCore_Array_Free(state, &handle.members);
    ZrCore_Array_Free(state, &source.members);
    ZrCore_Array_Free(state, &writableRef.members);
    ZrCore_Array_Free(state, &readonlyRef.members);
    if (success) {
        TEST_PASS(timer, summary);
    }
}

static void test_lsp_pooling_hover_completion_and_projection_expose_guard_contract(
        SZrState *state) {
    static const TZrChar *content =
            "let {Pool, PoolRef, PoolReadRef} = import(\"zr.pooling\");\n"
            "var pool = new Pool<int>();\n"
            "var handle = pool.deliver(7);\n"
            "var writeView: PoolRef<int>;\n"
            "var readView: PoolReadRef<int>;\n"
            "pool.tryBorrow(handle, out writeView);\n"
            "pool.tryRead(handle, out readView);\n"
            "writeView.value;\n"
            "handle;\n"
            "pool;\n";
    const TZrChar *summary =
            "LSP Pooling Hover Completion And Projection Expose Guard Contract";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrLspPosition poolCompletionPosition;
    SZrLspPosition handleHoverPosition;
    SZrLspPosition sourceHoverPosition;
    SZrLspPosition refHoverPosition;
    SZrLspPosition propertyHoverPosition;
    SZrArray completions = {0};
    SZrLspHover *handleHover = ZR_NULL;
    SZrLspHover *sourceHover = ZR_NULL;
    SZrLspHover *refHover = ZR_NULL;
    SZrLspHover *propertyHover = ZR_NULL;
    TZrBool success = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_CreateFromNative(state, "file:///stable_slot_contract.zr");
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16u);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1u) ||
        !lsp_find_position_for_substring(content, "pool.tryBorrow", 0u, 5, &poolCompletionPosition) ||
        !lsp_find_position_for_substring(content, "handle;", 0u, 1, &handleHoverPosition) ||
        !lsp_find_position_for_substring(content, "pool;", 0u, 1, &sourceHoverPosition) ||
        !lsp_find_position_for_substring(content, "writeView.value", 0u, 2, &refHoverPosition) ||
        !lsp_find_position_for_substring(content, "writeView.value", 0u, 11, &propertyHoverPosition)) {
        TEST_FAIL(timer, summary, "Failed to prepare pooling LSP fixture");
        goto cleanup;
    }

    if (!ZrLanguageServer_Lsp_GetCompletion(
                state, context, uri, poolCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "tryBorrow") ||
        !completion_array_contains_label(&completions, "tryRead") ||
        !ZrLanguageServer_Lsp_GetHover(state, context, uri, handleHoverPosition, &handleHover) ||
        !hover_contains_text(handleHover, "weak identity") ||
        !ZrLanguageServer_Lsp_GetHover(state, context, uri, sourceHoverPosition, &sourceHover) ||
        !hover_contains_text(sourceHover, "stable slot source") ||
        !ZrLanguageServer_Lsp_GetHover(state, context, uri, refHoverPosition, &refHover) ||
        !hover_contains_text(refHover, "scoped writable ref") ||
        !ZrLanguageServer_Lsp_GetHover(state, context, uri, propertyHoverPosition, &propertyHover) ||
        !hover_contains_text(propertyHover, "getter-only") ||
        !hover_contains_text(propertyHover, "active stable-slot guard")) {
        TEST_FAIL(timer, summary, "Pooling LSP facts did not expose the capability-driven guard workflow");
        goto cleanup;
    }
    success = ZR_TRUE;

cleanup:
    ZrCore_Array_Free(state, &completions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (success) {
        TEST_PASS(timer, summary);
    }
}

#endif /* ZR_VM_TEST_LSP_STABLE_SLOT_CONTRACT_CASES_H */
