#ifndef ZR_VM_TEST_RESOURCE_CROSS_DOMAIN_TRANSFER_VALUE_COPY_CASES_H
#define ZR_VM_TEST_RESOURCE_CROSS_DOMAIN_TRANSFER_VALUE_COPY_CASES_H

typedef struct ZrInlineValueCopyFixture {
    TZrUInt32 tag;
    TZrUInt64 payload;
    TZrByte suffix[7];
} ZrInlineValueCopyFixture;

static void test_inline_value_copy_uses_canonical_layout_snapshot(void) {
    SZrTypeLayout layout;
    SZrTypeLayout driftedLayout;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrOwnershipTransferSnapshot snapshot;
    ZrInlineValueCopyFixture source;
    ZrInlineValueCopyFixture expected;
    ZrInlineValueCopyFixture target;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            (TZrUInt32)sizeof(source),
            (TZrUInt32)sizeof(TZrUInt64),
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY, ZR_NULL);
    contract.schemaVersion = layout.domainTransferSchemaVersion;
    contract.schemaHash = layout.domainTransferSchemaHash;
    memset(&source, 0, sizeof(source));
    source.tag = 7u;
    source.payload = UINT64_C(0x1122334455667788);
    memcpy(source.suffix, "payload", sizeof(source.suffix));
    expected = source;
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomainValueCopy(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &layout,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    memset(&source, 0, sizeof(source));
    ZrCore_OwnershipTransfer_GetSnapshot(envelope, &snapshot);
    TEST_ASSERT_EQUAL_UINT64(sizeof(expected), snapshot.serializedByteCount);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 11u, 13u));
    driftedLayout = layout;
    driftedLayout.domainTransferSchemaHash ^= 1u;
    driftedLayout.layoutHash = ZrCore_TypeLayout_ComputeHash(&driftedLayout);
    memset(&target, 0, sizeof(target));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_CommitCrossDomainValueCopy(
            envelope,
            g_target_state,
            11u,
            13u,
            &driftedLayout,
            &target,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED, diagnostic.status);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomainValueCopy(
            envelope,
            g_target_state,
            11u,
            13u,
            &layout,
            &target,
            &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&expected, &target, sizeof(expected));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);

    source = expected;
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomainValueCopy(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &layout,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_source_state, 0u, 0u, &diagnostic));
    ZrCore_OwnershipTransfer_GetSnapshot(envelope, &snapshot);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_TRANSFER_STATE_ABORTED, snapshot.state);
    TEST_ASSERT_FALSE(snapshot.hasPayload);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_inline_value_copy_reports_stale_target_generation(void) {
    SZrTypeLayout layout;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrState *replacement;
    ZrInlineValueCopyFixture source;
    ZrInlineValueCopyFixture target;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            (TZrUInt32)sizeof(source),
            (TZrUInt32)sizeof(TZrUInt64),
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY, ZR_NULL);
    contract.schemaVersion = layout.domainTransferSchemaVersion;
    contract.schemaHash = layout.domainTransferSchemaHash;
    memset(&source, 0, sizeof(source));
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomainValueCopy(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &layout,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    ZrTests_Runtime_State_Destroy(g_target_state);
    g_target_state = ZR_NULL;
    replacement = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(replacement);
    memset(&target, 0, sizeof(target));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_CommitCrossDomainValueCopy(
            envelope,
            replacement,
            257u,
            263u,
            &layout,
            &target,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION, diagnostic.status);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_source_state, 0u, 0u, &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
    ZrTests_Runtime_State_Destroy(replacement);
}

#endif
