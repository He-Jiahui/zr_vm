#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/ownership_transfer.h"
#include "zr_vm_core/string.h"

typedef struct ZrTransferProviderContext {
    TZrUInt32 prepareCount;
    TZrUInt32 commitCount;
    TZrUInt32 abortCount;
    TZrUInt32 nativeReentryCount;
    EZrDomainTransferStatus prepareStatus;
    EZrDomainTransferStatus commitStatus;
    TZrBool consumeSource;
    TZrBool exerciseNativeReentry;
    TZrBool omitTargetOnCommit;
    TZrBool constructTargetBeforeFailure;
    TZrBool snapshotDuringCommit;
    TZrUInt32 snapshotDuringCommitCount;
    EZrOwnershipTransferState snapshotDuringCommitState;
    SZrOwnershipTransferEnvelope *envelope;
} ZrTransferProviderContext;

static SZrState *g_source_state;
static SZrState *g_target_state;
static TZrUInt32 g_resource_drop_count;

static TZrInt64 transfer_resource_drop(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    g_resource_drop_count++;
    return 0;
}

static void init_resource_unique(
        SZrState *state,
        SZrTypeValue *outOwner,
        TZrBool hasCountedDrop) {
    SZrString *name = ZrCore_String_CreateFromNative(
            state, "CrossDomainResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(state, 0u);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(destructor);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    if (hasCountedDrop) {
        destructor->nativeFunction = transfer_resource_drop;
        ZrCore_RawObject_MarkAsPermanent(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
        ZrCore_ObjectPrototype_AddMeta(
                state,
                prototype,
                ZR_META_DESTRUCTOR,
                ZR_CAST(
                        SZrFunction *,
                        ZR_CAST_RAW_OBJECT_AS_SUPER(destructor)));
    }
    object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    ZrCore_Value_ResetAsNull(outOwner);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            state, outOwner, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
}

static EZrDomainTransferStatus transfer_provider_prepare(
        SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source,
        SZrDomainTransferProviderToken *outToken,
        TZrPtr userData) {
    ZrTransferProviderContext *context =
            (ZrTransferProviderContext *)userData;

    ZR_UNUSED_PARAMETER(targetDomain);
    context->prepareCount++;
    if (context->prepareStatus != ZR_DOMAIN_TRANSFER_STATUS_OK) {
        return context->prepareStatus;
    }
    memset(outToken, 0, sizeof(*outToken));
    outToken->words[0] = UINT64_C(0xc0decafe);
    if (context->consumeSource) {
        ZrCore_Ownership_ReleaseValue(sourceState, source);
    }
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

static EZrDomainTransferStatus transfer_provider_commit(
        SZrState *targetState,
        SZrDomainTransferProviderToken *token,
        SZrTypeValue *target,
        TZrPtr userData) {
    ZrTransferProviderContext *context =
            (ZrTransferProviderContext *)userData;

    context->commitCount++;
    if (context->snapshotDuringCommit) {
        SZrOwnershipTransferSnapshot snapshot;
        ZrCore_OwnershipTransfer_GetSnapshot(context->envelope, &snapshot);
        context->snapshotDuringCommitCount++;
        context->snapshotDuringCommitState = snapshot.state;
    }
    if (context->constructTargetBeforeFailure) {
        init_resource_unique(targetState, target, ZR_TRUE);
    }
    if (context->commitStatus != ZR_DOMAIN_TRANSFER_STATUS_OK) {
        return context->commitStatus;
    }
    if (token->words[0] != UINT64_C(0xc0decafe)) {
        return ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED;
    }
    if (context->exerciseNativeReentry) {
        if (!ZrCore_GcDomain_NativeEnter(
                    targetState, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE)) {
            return ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED;
        }
        context->nativeReentryCount++;
        ZrCore_GcDomain_MutatorPoll(targetState);
        ZrCore_GcDomain_NativeLeave(targetState);
    }
    if (context->omitTargetOnCommit) {
        return ZR_DOMAIN_TRANSFER_STATUS_OK;
    }
    if (context->consumeSource) {
        init_resource_unique(targetState, target, ZR_TRUE);
    } else {
        ZrCore_Value_InitAsInt(targetState, target, 71);
    }
    memset(token, 0, sizeof(*token));
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

static EZrDomainTransferStatus transfer_provider_commit_replaced(
        SZrState *targetState,
        SZrDomainTransferProviderToken *token,
        SZrTypeValue *target,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(targetState);
    ZR_UNUSED_PARAMETER(token);
    ZR_UNUSED_PARAMETER(target);
    ZR_UNUSED_PARAMETER(userData);
    return ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED;
}

static void transfer_provider_abort(
        SZrDomainTransferProviderToken *token,
        TZrPtr userData) {
    ZrTransferProviderContext *context =
            (ZrTransferProviderContext *)userData;

    context->abortCount++;
    memset(token, 0, sizeof(*token));
}

static SZrDomainTransferContract make_contract(
        EZrDomainTransferKind kind,
        const SZrDomainTransferProvider *provider) {
    SZrDomainTransferContract contract;

    memset(&contract, 0, sizeof(contract));
    contract.kind = kind;
    contract.schemaVersion = 1u;
    contract.schemaHash = UINT64_C(0x1122334455667788);
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    contract.quota.maxObjects = 16u;
    contract.quota.maxBytes = 4096u;
    contract.quota.maxDepth = 16u;
    contract.provider = provider;
    if (provider != ZR_NULL) {
        contract.providerToken = provider->providerToken;
        contract.providerContractHash = provider->providerContractHash;
    }
    return contract;
}

static SZrDomainTransferProvider make_provider(
        ZrTransferProviderContext *context,
        TZrUInt32 providerToken,
        TZrUInt64 providerContractHash) {
    SZrDomainTransferProvider provider;

    memset(&provider, 0, sizeof(provider));
    provider.providerToken = providerToken;
    provider.providerContractHash = providerContractHash;
    provider.prepare = transfer_provider_prepare;
    provider.commit = transfer_provider_commit;
    provider.abort = transfer_provider_abort;
    provider.userData = context;
    return provider;
}

static SZrObject *new_plain_object(SZrState *state) {
    SZrObject *object = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    return object;
}

static void object_set_member(
        SZrState *state,
        SZrObject *object,
        TZrNativeString name,
        const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrString *keyString = ZrCore_String_CreateFromNative(state, name);

    TEST_ASSERT_NOT_NULL(keyString);
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

static const SZrTypeValue *object_get_member(
        SZrState *state,
        SZrObject *object,
        TZrNativeString name) {
    SZrTypeValue key;
    SZrString *keyString = ZrCore_String_CreateFromNative(state, name);

    TEST_ASSERT_NOT_NULL(keyString);
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

void setUp(void) {
    g_source_state = ZrTests_Runtime_State_Create(ZR_NULL);
    g_target_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_source_state);
    TEST_ASSERT_NOT_NULL(g_target_state);
    g_resource_drop_count = 0u;
}

void tearDown(void) {
    if (g_target_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_target_state);
        g_target_state = ZR_NULL;
    }
    if (g_source_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_source_state);
        g_source_state = ZR_NULL;
    }
}

static void test_value_copy_cross_domain_roundtrips_without_moving_source(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrOwnershipTransferSnapshot snapshot;
    SZrTypeValue source;
    SZrTypeValue target;

    ZrCore_Value_InitAsInt(g_source_state, &source, 42);
    ZrCore_Value_ResetAsNull(&target);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
    TEST_ASSERT_EQUAL_INT64(42, source.value.nativeObject.nativeInt64);
    ZrCore_OwnershipTransfer_GetSnapshot(envelope, &snapshot);
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY, snapshot.kind);
    TEST_ASSERT_FALSE(snapshot.hasSourceGcEdge);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 7u, 11u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope, g_target_state, 7u, 11u, &target, &diagnostic));
    TEST_ASSERT_EQUAL_INT64(42, target.value.nativeObject.nativeInt64);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_forbidden_and_gc_value_copy_are_rejected_before_publish(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrTypeValue value;
    SZrObject *object = new_plain_object(g_source_state);

    ZrCore_Value_InitAsRawObject(
            g_source_state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &value,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_FORBIDDEN, diagnostic.status);

    contract.kind = ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY;
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &value,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE, diagnostic.status);
}

static void test_structured_clone_preserves_alias_and_cycle_without_source_edges(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrOwnershipTransferSnapshot snapshot;
    SZrObject *root = new_plain_object(g_source_state);
    SZrObject *child = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrTypeValue childValue;
    SZrTypeValue targetValue;
    SZrObject *targetRoot;
    const SZrTypeValue *left;
    const SZrTypeValue *right;
    const SZrTypeValue *parent;

    ZrCore_Value_InitAsRawObject(
            g_source_state,
            &rootValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(root));
    ZrCore_Value_InitAsRawObject(
            g_source_state,
            &childValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    object_set_member(g_source_state, root, "left", &childValue);
    object_set_member(g_source_state, root, "right", &childValue);
    object_set_member(g_source_state, child, "parent", &rootValue);

    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &rootValue,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    ZrCore_OwnershipTransfer_GetSnapshot(envelope, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(2u, snapshot.serializedObjectCount);
    TEST_ASSERT_TRUE(snapshot.serializedByteCount > 0u);
    TEST_ASSERT_FALSE(snapshot.hasSourceGcEdge);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 9u, 13u));
    ZrCore_Value_ResetAsNull(&targetValue);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope, g_target_state, 9u, 13u, &targetValue, &diagnostic));
    targetRoot = ZR_CAST_OBJECT(g_target_state, targetValue.value.object);
    TEST_ASSERT_NOT_NULL(targetRoot);
    TEST_ASSERT_NOT_EQUAL(root, targetRoot);
    left = object_get_member(g_target_state, targetRoot, "left");
    right = object_get_member(g_target_state, targetRoot, "right");
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_NOT_NULL(right);
    TEST_ASSERT_EQUAL_PTR(left->value.object, right->value.object);
    parent = object_get_member(
            g_target_state,
            ZR_CAST_OBJECT(g_target_state, left->value.object),
            "parent");
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_EQUAL_PTR(targetRoot, parent->value.object);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_structured_clone_object_quota_fails_atomically(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrObject *root = new_plain_object(g_source_state);
    SZrObject *child = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrTypeValue childValue;

    contract.quota.maxObjects = 1u;
    ZrCore_Value_InitAsRawObject(
            g_source_state,
            &rootValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(root));
    ZrCore_Value_InitAsRawObject(
            g_source_state,
            &childValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    object_set_member(g_source_state, root, "child", &childValue);
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &rootValue,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_OBJECT_QUOTA, diagnostic.status);
    TEST_ASSERT_EQUAL_PTR(root, rootValue.value.object);
}

static void assert_resource_move_commit_failure_aborts_exactly_once(
        EZrDomainTransferStatus callbackStatus,
        TZrBool omitTarget,
        EZrDomainTransferStatus expectedStatus,
        TZrUInt32 providerToken) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;

    memset(&context, 0, sizeof(context));
    context.consumeSource = ZR_TRUE;
    context.commitStatus = callbackStatus;
    context.omitTargetOnCommit = omitTarget;
    provider = make_provider(
            &context, providerToken, UINT64_C(0xaabbccddeeff0011));
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE, &provider);
    init_resource_unique(g_source_state, &source, ZR_FALSE);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 17u, 19u));
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope, g_target_state, 17u, 19u, &target, &diagnostic));
    TEST_ASSERT_EQUAL_INT(expectedStatus, diagnostic.status);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_target_state, 17u, 19u, &diagnostic));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_target_state, 17u, 19u, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, context.prepareCount);
    TEST_ASSERT_EQUAL_UINT32(1u, context.commitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, context.abortCount);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_resource_move_provider_commit_failure_aborts_exactly_once(void) {
    assert_resource_move_commit_failure_aborts_exactly_once(
            ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED,
            ZR_FALSE,
            ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED,
            0x06000001u);
}

static void test_resource_move_target_allocation_failure_aborts_exactly_once(void) {
    assert_resource_move_commit_failure_aborts_exactly_once(
            ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
            ZR_FALSE,
            ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
            0x06000008u);
}

static void test_resource_move_decode_failure_aborts_exactly_once(void) {
    assert_resource_move_commit_failure_aborts_exactly_once(
            ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
            ZR_FALSE,
            ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
            0x06000009u);
}

static void test_resource_move_success_without_target_is_rejected(void) {
    assert_resource_move_commit_failure_aborts_exactly_once(
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            ZR_TRUE,
            ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED,
            0x0600000au);
}

static void test_immutable_handle_provider_keeps_source_and_commits_target(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;

    memset(&context, 0, sizeof(context));
    memset(&provider, 0, sizeof(provider));
    provider.providerToken = 0x06000002u;
    provider.providerContractHash = UINT64_C(0x0102030405060708);
    provider.prepare = transfer_provider_prepare;
    provider.commit = transfer_provider_commit;
    provider.abort = transfer_provider_abort;
    provider.userData = &context;
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
    TEST_ASSERT_EQUAL_INT64(71, source.value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 23u, 29u));
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope, g_target_state, 23u, 29u, &target, &diagnostic));
    TEST_ASSERT_EQUAL_INT64(71, target.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(0u, context.abortCount);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_stale_target_generation_cannot_claim_and_source_can_abort(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrState *wrongTarget = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue source;

    TEST_ASSERT_NOT_NULL(wrongTarget);
    ZrCore_Value_InitAsInt(g_source_state, &source, 5);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Claim(
            envelope, wrongTarget, 31u, 37u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_source_state, 0u, 0u, &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
    ZrTests_Runtime_State_Destroy(wrongTarget);
}

static void test_resource_move_prepare_failure_drops_consumed_source_once(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrTypeValue source;

    memset(&context, 0, sizeof(context));
    context.prepareStatus = ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED;
    provider = make_provider(
            &context, 0x06000003u, UINT64_C(0x1011121314151617));
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE, &provider);
    init_resource_unique(g_source_state, &source, ZR_TRUE);
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED,
            diagnostic.status);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_EQUAL_UINT32(1u, context.prepareCount);
    TEST_ASSERT_EQUAL_UINT32(1u, context.abortCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resource_drop_count);
}

static void test_resource_move_provider_identity_mismatch_never_consumes_source(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrTypeValue source;

    memset(&context, 0, sizeof(context));
    context.consumeSource = ZR_TRUE;
    provider = make_provider(
            &context, 0x06000004u, UINT64_C(0x2021222324252627));
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE, &provider);
    contract.providerContractHash ^= 1u;
    init_resource_unique(g_source_state, &source, ZR_TRUE);
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT, diagnostic.status);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_VALUE_KIND_UNIQUE, source.ownershipKind);
    TEST_ASSERT_EQUAL_UINT32(0u, context.prepareCount);
    ZrCore_Ownership_ReleaseValue(g_source_state, &source);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resource_drop_count);
}

static void test_resource_move_success_commits_one_target_owner(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;

    memset(&context, 0, sizeof(context));
    context.consumeSource = ZR_TRUE;
    context.exerciseNativeReentry = ZR_TRUE;
    provider = make_provider(
            &context, 0x06000005u, UINT64_C(0x3031323334353637));
    contract = make_contract(ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE, &provider);
    init_resource_unique(g_source_state, &source, ZR_FALSE);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 41u, 43u));
    ZrCore_Value_ResetAsNull(&target);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope, g_target_state, 41u, 43u, &target, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_VALUE_KIND_UNIQUE, target.ownershipKind);
    TEST_ASSERT_EQUAL_UINT32(1u, context.prepareCount);
    TEST_ASSERT_EQUAL_UINT32(1u, context.commitCount);
    TEST_ASSERT_EQUAL_UINT32(1u, context.nativeReentryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, context.abortCount);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
    TEST_ASSERT_EQUAL_UINT32(0u, g_resource_drop_count);
    ZrCore_Ownership_ReleaseValue(g_target_state, &target);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resource_drop_count);
}

static void test_structured_clone_rejects_byte_depth_and_foreign_domain_edges(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrObject *root = new_plain_object(g_source_state);
    SZrObject *child = new_plain_object(g_source_state);
    SZrObject *grandchild = new_plain_object(g_source_state);
    SZrObject *foreign = new_plain_object(g_target_state);
    SZrTypeValue rootValue;
    SZrTypeValue childValue;
    SZrTypeValue grandchildValue;
    SZrTypeValue foreignValue;

    ZrCore_Value_InitAsRawObject(
            g_source_state, &rootValue, ZR_CAST_RAW_OBJECT_AS_SUPER(root));
    ZrCore_Value_InitAsRawObject(
            g_source_state, &childValue, ZR_CAST_RAW_OBJECT_AS_SUPER(child));
    ZrCore_Value_InitAsRawObject(
            g_source_state,
            &grandchildValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(grandchild));
    object_set_member(g_source_state, root, "child", &childValue);
    object_set_member(g_source_state, child, "child", &grandchildValue);

    contract.quota.maxBytes = 1u;
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &rootValue,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_BYTE_QUOTA, diagnostic.status);

    contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    contract.quota.maxDepth = 1u;
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &rootValue,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_DEPTH_QUOTA, diagnostic.status);

    contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    ZrCore_Value_InitAsRawObject(
            g_target_state,
            &foreignValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(foreign));
    TEST_ASSERT_NULL(ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &foreignValue,
            &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE, diagnostic.status);
}

static void test_structured_clone_decode_failure_is_abortable_without_target(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrObject *root = new_plain_object(g_source_state);
    SZrTypeValue rootValue;
    SZrTypeValue fieldValue;
    SZrTypeValue target;

    ZrCore_Value_InitAsRawObject(
            g_source_state, &rootValue, ZR_CAST_RAW_OBJECT_AS_SUPER(root));
    ZrCore_Value_InitAsInt(g_source_state, &fieldValue, 9);
    object_set_member(g_source_state, root, "value", &fieldValue);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &rootValue,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 47u, 53u));
    ZrCore_Value_ResetAsNull(&target);
    g_target_state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope, g_target_state, 47u, 53u, &target, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED, diagnostic.status);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(target.type));
    g_target_state->threadStatus = ZR_THREAD_STATUS_FINE;
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_target_state, 47u, 53u, &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_target_domain_shutdown_aborts_resource_token_from_source(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrState *replacement;
    SZrTypeValue source;

    memset(&context, 0, sizeof(context));
    context.consumeSource = ZR_TRUE;
    provider = make_provider(
            &context, 0x06000006u, UINT64_C(0x4041424344454647));
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
    ZrTests_Runtime_State_Destroy(g_target_state);
    g_target_state = ZR_NULL;
    replacement = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(replacement);
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Claim(
            envelope, replacement, 59u, 61u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_source_state, 0u, 0u, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, context.abortCount);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
    ZrTests_Runtime_State_Destroy(replacement);
}

static void test_claimed_domain_shutdown_aborts_resource_token_from_target(void) {
    ZrTransferProviderContext context;
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract;
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;

    memset(&context, 0, sizeof(context));
    context.consumeSource = ZR_TRUE;
    provider = make_provider(
            &context, 0x0600000cu, UINT64_C(0x8182838485868788));
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
            envelope, g_target_state, 107u, 109u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_target_state, 107u, 109u, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, context.abortCount);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
    ZrTests_Runtime_State_Destroy(g_target_state);
    g_target_state = ZR_NULL;
}

static void test_transfer_telemetry_is_attributed_to_source_and_target_domains(void) {
    SZrDomainTransferContract contract = make_contract(
            ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY, ZR_NULL);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrGarbageCollectorStatsSnapshot sourceStats;
    SZrGarbageCollectorStatsSnapshot targetStats;
    SZrTypeValue source;
    SZrTypeValue target;

    ZrCore_Value_InitAsInt(g_source_state, &source, 42);
    ZrCore_Value_ResetAsNull(&target);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 101u, 103u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            envelope,
            g_target_state,
            101u,
            103u,
            &target,
            &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);

    ZrCore_Value_InitAsInt(g_source_state, &source, 43);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_source_state, 0u, 0u, &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);

    ZrCore_Value_InitAsInt(g_source_state, &source, 44);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_target_state, 107u, 109u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
            envelope, g_target_state, 107u, 109u, &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);

    memset(&sourceStats, 0, sizeof(sourceStats));
    memset(&targetStats, 0, sizeof(targetStats));
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_source_state->global, &sourceStats);
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_target_state->global, &targetStats);
    TEST_ASSERT_EQUAL_UINT64(
            ZrCore_GcDomain_GetIdentity(g_source_state).id,
            sourceStats.domainId);
    TEST_ASSERT_EQUAL_UINT64(
            ZrCore_GcDomain_GetIdentity(g_target_state).id,
            targetStats.domainId);
    TEST_ASSERT_EQUAL_UINT64(3u, sourceStats.outboundTransferPrepareCount);
    TEST_ASSERT_EQUAL_UINT64(3u, sourceStats.outboundTransferPublishCount);
    TEST_ASSERT_EQUAL_UINT64(1u, sourceStats.outboundTransferAbortCount);
    TEST_ASSERT_EQUAL_UINT64(2u, targetStats.inboundTransferClaimCount);
    TEST_ASSERT_EQUAL_UINT64(1u, targetStats.inboundTransferCommitCount);
    TEST_ASSERT_EQUAL_UINT64(1u, targetStats.inboundTransferAbortCount);
}

#include "test_resource_cross_domain_transfer_value_copy_cases.h"
#include "test_resource_cross_domain_transfer_review_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_value_copy_cross_domain_roundtrips_without_moving_source);
    RUN_TEST(test_inline_value_copy_uses_canonical_layout_snapshot);
    RUN_TEST(test_forbidden_and_gc_value_copy_are_rejected_before_publish);
    RUN_TEST(test_structured_clone_preserves_alias_and_cycle_without_source_edges);
    RUN_TEST(test_structured_clone_object_quota_fails_atomically);
    RUN_TEST(test_resource_move_provider_commit_failure_aborts_exactly_once);
    RUN_TEST(test_resource_move_target_allocation_failure_aborts_exactly_once);
    RUN_TEST(test_resource_move_decode_failure_aborts_exactly_once);
    RUN_TEST(test_resource_move_success_without_target_is_rejected);
    RUN_TEST(test_immutable_handle_provider_keeps_source_and_commits_target);
    RUN_TEST(test_stale_target_generation_cannot_claim_and_source_can_abort);
    RUN_TEST(test_resource_move_prepare_failure_drops_consumed_source_once);
    RUN_TEST(test_resource_move_provider_identity_mismatch_never_consumes_source);
    RUN_TEST(test_resource_move_success_commits_one_target_owner);
    RUN_TEST(test_structured_clone_rejects_byte_depth_and_foreign_domain_edges);
    RUN_TEST(test_structured_clone_decode_failure_is_abortable_without_target);
    RUN_TEST(test_target_domain_shutdown_aborts_resource_token_from_source);
    RUN_TEST(test_claimed_domain_shutdown_aborts_resource_token_from_target);
    RUN_TEST(test_transfer_telemetry_is_attributed_to_source_and_target_domains);
    RUN_TEST(test_provider_descriptor_is_snapshotted_by_the_envelope);
    RUN_TEST(test_provider_commit_failure_releases_partial_target_once);
    RUN_TEST(test_provider_commit_can_query_envelope_without_lock_reentry_deadlock);
    RUN_TEST(test_structured_clone_decode_roots_exist_before_next_allocation);
    RUN_TEST(test_inline_value_copy_reports_stale_target_generation);
    return UNITY_END();
}
