#ifndef ZR_VM_TEST_RESOURCE_CROSS_DOMAIN_TRANSFER_REVIEW_CASES_H
#define ZR_VM_TEST_RESOURCE_CROSS_DOMAIN_TRANSFER_REVIEW_CASES_H

typedef struct ZrTransferRootObservationContext {
    SZrState *state;
    TZrSize allocationCount;
    TZrSize maximumObservedRootCount;
} ZrTransferRootObservationContext;

static TZrPtr transfer_observe_roots_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    ZrTransferRootObservationContext *context =
            (ZrTransferRootObservationContext *)userData;

    if (newSize > 0u && pointer == ZR_NULL) {
        TZrSize activeRootCount = context->state->gcDomain->activeRootCount;

        context->allocationCount++;
        if (activeRootCount > context->maximumObservedRootCount) {
            context->maximumObservedRootCount = activeRootCount;
        }
    }
    return ZrTests_Runtime_Allocator_Default(
            ZR_NULL, pointer, originalSize, newSize, flag);
}

static void test_provider_descriptor_is_snapshotted_by_the_envelope(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;

    memset(&context, 0, sizeof(context));
    provider = make_provider(
            &context, 0x06000021u, UINT64_C(0x1111222233334444));
    contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE, &provider);
    ZrCore_Value_InitAsInt(g_source_state, &source, 71);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    provider.commit = transfer_provider_commit_replaced;
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 211u, 223u));
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope,
            g_target_state,
            211u,
            223u,
            &target,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT64(71, target.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, context.commitCount);
    ZrCore_OwnershipTransfer_Free(g_target_state, envelope);
}

static void test_provider_commit_failure_releases_partial_target_once(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;

    memset(&context, 0, sizeof(context));
    context.consumeSource = ZR_TRUE;
    context.constructTargetBeforeFailure = ZR_TRUE;
    context.commitStatus = ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED;
    provider = make_provider(
            &context, 0x06000022u, UINT64_C(0x2222333344445555));
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE, &provider);
    init_resource_unique(g_source_state, &source, ZR_FALSE);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 227u, 229u));
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope,
            g_target_state,
            227u,
            229u,
            &target,
            &diagnostic));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(target.type));
    TEST_ASSERT_EQUAL_UINT32(1u, g_resource_drop_count);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_target_state, 227u, 229u, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, context.abortCount);
    ZrCore_OwnershipTransfer_Free(g_target_state, envelope);
}

static void test_provider_commit_can_query_envelope_without_lock_reentry_deadlock(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;

    memset(&context, 0, sizeof(context));
    context.snapshotDuringCommit = ZR_TRUE;
    provider = make_provider(
            &context, 0x06000023u, UINT64_C(0x3333444455556666));
    contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE, &provider);
    ZrCore_Value_InitAsInt(g_source_state, &source, 71);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    context.envelope = envelope;
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 233u, 239u));
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope,
            g_target_state,
            233u,
            239u,
            &target,
            &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, context.snapshotDuringCommitCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
            context.snapshotDuringCommitState);
    ZrCore_OwnershipTransfer_Free(g_target_state, envelope);
}

static void test_structured_clone_decode_roots_exist_before_next_allocation(void) {
    ZrTransferRootObservationContext allocatorContext;
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrObject *first = new_plain_object(g_source_state);
    SZrObject *second = new_plain_object(g_source_state);
    SZrTypeValue source;
    SZrTypeValue secondValue;
    SZrTypeValue target;
    FZrAllocator originalAllocator;
    TZrPtr originalAllocatorUserData;
    TZrBool commitResult;

    ZrCore_Value_InitAsRawObject(
            g_source_state,
            &secondValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(second));
    secondValue.type = ZR_VALUE_TYPE_OBJECT;
    object_set_member(g_source_state, first, "next", &secondValue);
    object_set_member(g_source_state, second, "next", &secondValue);
    ZrCore_Value_InitAsRawObject(
            g_source_state, &source, ZR_CAST_RAW_OBJECT_AS_SUPER(first));
    source.type = ZR_VALUE_TYPE_OBJECT;
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 241u, 251u));
    memset(&allocatorContext, 0, sizeof(allocatorContext));
    allocatorContext.state = g_target_state;
    originalAllocator = g_target_state->global->allocator;
    originalAllocatorUserData = g_target_state->global->userAllocationArguments;
    g_target_state->global->allocator = transfer_observe_roots_allocator;
    g_target_state->global->userAllocationArguments = &allocatorContext;
    ZrCore_Value_ResetAsNull(&target);
    commitResult = ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope,
            g_target_state,
            241u,
            251u,
            &target,
            &diagnostic);
    g_target_state->global->allocator = originalAllocator;
    g_target_state->global->userAllocationArguments =
            originalAllocatorUserData;
    TEST_ASSERT_TRUE(commitResult);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.allocationCount);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u, allocatorContext.maximumObservedRootCount);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(
            g_target_state, target.value.object));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_GcDomain_GetRootCount(g_target_state));
    ZrCore_OwnershipTransfer_Free(g_target_state, envelope);
}

#endif
