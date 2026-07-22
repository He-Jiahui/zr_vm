#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/ownership_transfer.h"
#include "zr_vm_core/string.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrCrossTransferTestThread;
typedef volatile LONG ZrCrossTransferTestAtomic;
#else
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
typedef pthread_t ZrCrossTransferTestThread;
typedef _Atomic int ZrCrossTransferTestAtomic;
#endif

typedef struct ZrRaceProviderContext {
    TZrUInt32 prepareCount;
    TZrUInt32 commitCount;
    TZrUInt32 abortCount;
} ZrRaceProviderContext;

typedef enum EZrCrossTransferRaceOperation {
    ZR_CROSS_TRANSFER_RACE_PUBLISH = 0,
    ZR_CROSS_TRANSFER_RACE_CLAIM,
    ZR_CROSS_TRANSFER_RACE_COMMIT,
    ZR_CROSS_TRANSFER_RACE_ABORT
} EZrCrossTransferRaceOperation;

typedef struct ZrCrossTransferRaceContext {
    SZrOwnershipTransferEnvelope *envelope;
    SZrState *state;
    SZrTypeValue *target;
    ZrCrossTransferTestAtomic *start;
    EZrCrossTransferRaceOperation operation;
    TZrUInt64 workerId;
    TZrUInt64 claimEpoch;
    TZrBool result;
} ZrCrossTransferRaceContext;

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
            state, "CrossDomainRaceResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    if (hasCountedDrop) {
        SZrClosureNative *destructor = ZrCore_ClosureNative_New(state, 0u);
        TEST_ASSERT_NOT_NULL(destructor);
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

static EZrDomainTransferStatus race_provider_prepare(
        SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source,
        SZrDomainTransferProviderToken *outToken,
        TZrPtr userData) {
    ZrRaceProviderContext *context = (ZrRaceProviderContext *)userData;

    ZR_UNUSED_PARAMETER(targetDomain);
    context->prepareCount++;
    memset(outToken, 0, sizeof(*outToken));
    outToken->words[0] = UINT64_C(0x123456789abcdef0);
    ZrCore_Ownership_ReleaseValue(sourceState, source);
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

static EZrDomainTransferStatus race_provider_commit(
        SZrState *targetState,
        SZrDomainTransferProviderToken *token,
        SZrTypeValue *target,
        TZrPtr userData) {
    ZrRaceProviderContext *context = (ZrRaceProviderContext *)userData;

    context->commitCount++;
    if (token->words[0] != UINT64_C(0x123456789abcdef0)) {
        return ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED;
    }
    init_resource_unique(targetState, target, ZR_TRUE);
    memset(token, 0, sizeof(*token));
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

static void race_provider_abort(
        SZrDomainTransferProviderToken *token,
        TZrPtr userData) {
    ZrRaceProviderContext *context = (ZrRaceProviderContext *)userData;

    context->abortCount++;
    memset(token, 0, sizeof(*token));
}

static EZrDomainTransferStatus immutable_provider_prepare(
        SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source,
        SZrDomainTransferProviderToken *outToken,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(sourceState);
    ZR_UNUSED_PARAMETER(targetDomain);
    ZR_UNUSED_PARAMETER(userData);
    if (source == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(source->type)) {
        return ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED;
    }
    memset(outToken, 0, sizeof(*outToken));
    outToken->words[0] = (TZrUInt64)source->value.nativeObject.nativeInt64;
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

static EZrDomainTransferStatus immutable_provider_commit(
        SZrState *targetState,
        SZrDomainTransferProviderToken *token,
        SZrTypeValue *target,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    ZrCore_Value_InitAsInt(
            targetState, target, (TZrInt64)token->words[0]);
    memset(token, 0, sizeof(*token));
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

static void immutable_provider_abort(
        SZrDomainTransferProviderToken *token,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    memset(token, 0, sizeof(*token));
}

static SZrDomainTransferProvider make_provider(
        ZrRaceProviderContext *context) {
    SZrDomainTransferProvider provider;

    memset(&provider, 0, sizeof(provider));
    provider.providerToken = 0x06000007u;
    provider.providerContractHash = UINT64_C(0x5051525354555657);
    provider.prepare = race_provider_prepare;
    provider.commit = race_provider_commit;
    provider.abort = race_provider_abort;
    provider.userData = context;
    return provider;
}

static SZrDomainTransferContract make_resource_contract(
        const SZrDomainTransferProvider *provider) {
    SZrDomainTransferContract contract;

    memset(&contract, 0, sizeof(contract));
    contract.kind = ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE;
    contract.schemaVersion = 1u;
    contract.schemaHash = UINT64_C(0x1122334455667788);
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    contract.providerToken = provider->providerToken;
    contract.providerContractHash = provider->providerContractHash;
    contract.provider = provider;
    return contract;
}

static SZrDomainTransferContract make_immutable_contract(
        SZrDomainTransferProvider *provider) {
    SZrDomainTransferContract contract;

    memset(provider, 0, sizeof(*provider));
    provider->providerToken = 0x0600000bu;
    provider->providerContractHash = UINT64_C(0x6061626364656667);
    provider->prepare = immutable_provider_prepare;
    provider->commit = immutable_provider_commit;
    provider->abort = immutable_provider_abort;
    memset(&contract, 0, sizeof(contract));
    contract.kind = ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE;
    contract.schemaVersion = 1u;
    contract.schemaHash = UINT64_C(0x7172737475767778);
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    contract.providerToken = provider->providerToken;
    contract.providerContractHash = provider->providerContractHash;
    contract.provider = provider;
    return contract;
}

static SZrDomainTransferContract make_value_copy_contract(void) {
    SZrDomainTransferContract contract;

    memset(&contract, 0, sizeof(contract));
    contract.kind = ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY;
    contract.schemaVersion = 1u;
    contract.schemaHash = UINT64_C(0x8877665544332211);
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    return contract;
}

static int cross_transfer_test_atomic_load(ZrCrossTransferTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void cross_transfer_test_atomic_store(
        ZrCrossTransferTestAtomic *value,
        int next) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void cross_transfer_test_yield(void) {
#if defined(ZR_PLATFORM_WIN)
    (void)SwitchToThread();
#else
    (void)sched_yield();
#endif
}

static void cross_transfer_race_run(ZrCrossTransferRaceContext *context) {
    SZrDomainTransferDiagnostic diagnostic;

    while (!cross_transfer_test_atomic_load(context->start)) {
        cross_transfer_test_yield();
    }
    switch (context->operation) {
        case ZR_CROSS_TRANSFER_RACE_PUBLISH:
            context->result = ZrCore_OwnershipTransfer_Publish(
                    context->envelope);
            break;
        case ZR_CROSS_TRANSFER_RACE_CLAIM:
            context->result = ZrCore_OwnershipTransfer_Claim(
                    context->envelope,
                    context->state,
                    context->workerId,
                    context->claimEpoch);
            break;
        case ZR_CROSS_TRANSFER_RACE_COMMIT:
            context->result = ZrCore_OwnershipTransfer_CommitCrossDomain(
                    context->envelope,
                    context->state,
                    context->workerId,
                    context->claimEpoch,
                    context->target,
                    &diagnostic);
            break;
        case ZR_CROSS_TRANSFER_RACE_ABORT:
            context->result = ZrCore_OwnershipTransfer_AbortCrossDomain(
                    context->envelope,
                    context->state,
                    context->workerId,
                    context->claimEpoch,
                    &diagnostic);
            break;
        default:
            context->result = ZR_FALSE;
            break;
    }
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI cross_transfer_race_entry(LPVOID argument) {
    cross_transfer_race_run((ZrCrossTransferRaceContext *)argument);
    return 0u;
}

static ZrCrossTransferTestThread cross_transfer_thread_start(
        ZrCrossTransferRaceContext *context) {
    return CreateThread(
            ZR_NULL, 0u, cross_transfer_race_entry, context, 0u, ZR_NULL);
}

static void cross_transfer_thread_join(ZrCrossTransferTestThread thread) {
    TEST_ASSERT_NOT_NULL(thread);
    TEST_ASSERT_EQUAL_UINT32(
            WAIT_OBJECT_0, WaitForSingleObject(thread, 5000u));
    CloseHandle(thread);
}
#else
static void *cross_transfer_race_entry(void *argument) {
    cross_transfer_race_run((ZrCrossTransferRaceContext *)argument);
    return ZR_NULL;
}

static ZrCrossTransferTestThread cross_transfer_thread_start(
        ZrCrossTransferRaceContext *context) {
    ZrCrossTransferTestThread thread;

    TEST_ASSERT_EQUAL_INT(
            0,
            pthread_create(
                    &thread, ZR_NULL, cross_transfer_race_entry, context));
    return thread;
}

static void cross_transfer_thread_join(ZrCrossTransferTestThread thread) {
    TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, ZR_NULL));
}
#endif

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

static void test_prepared_publish_and_abort_are_linearized(void) {
    ZrRaceProviderContext providerContext = {0};
    SZrDomainTransferProvider provider = make_provider(&providerContext);
    SZrDomainTransferContract contract = make_resource_contract(&provider);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrOwnershipTransferSnapshot snapshot;
    SZrTypeValue source;
    ZrCrossTransferTestAtomic start = 0;
    ZrCrossTransferRaceContext publish = {0};
    ZrCrossTransferRaceContext abort = {0};
    ZrCrossTransferTestThread publishThread;
    ZrCrossTransferTestThread abortThread;

    init_resource_unique(g_source_state, &source, ZR_FALSE);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    publish.envelope = envelope;
    publish.start = &start;
    publish.operation = ZR_CROSS_TRANSFER_RACE_PUBLISH;
    abort = publish;
    abort.state = g_source_state;
    abort.operation = ZR_CROSS_TRANSFER_RACE_ABORT;
    publishThread = cross_transfer_thread_start(&publish);
    abortThread = cross_transfer_thread_start(&abort);
    cross_transfer_test_atomic_store(&start, 1);
    cross_transfer_thread_join(publishThread);
    cross_transfer_thread_join(abortThread);
    TEST_ASSERT_TRUE(abort.result);
    TEST_ASSERT_EQUAL_UINT32(1u, providerContext.abortCount);
    ZrCore_OwnershipTransfer_GetSnapshot(envelope, &snapshot);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_TRANSFER_STATE_ABORTED, snapshot.state);
    TEST_ASSERT_FALSE(snapshot.hasPayload);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_queued_claim_and_abort_have_one_winner(void) {
    ZrRaceProviderContext providerContext = {0};
    SZrDomainTransferProvider provider = make_provider(&providerContext);
    SZrDomainTransferContract contract = make_resource_contract(&provider);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    ZrCrossTransferTestAtomic start = 0;
    ZrCrossTransferRaceContext claim = {0};
    ZrCrossTransferRaceContext abort = {0};
    ZrCrossTransferTestThread claimThread;
    ZrCrossTransferTestThread abortThread;

    init_resource_unique(g_source_state, &source, ZR_FALSE);
    envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    claim.envelope = envelope;
    claim.state = g_target_state;
    claim.start = &start;
    claim.operation = ZR_CROSS_TRANSFER_RACE_CLAIM;
    claim.workerId = 67u;
    claim.claimEpoch = 71u;
    abort = claim;
    abort.state = g_source_state;
    abort.operation = ZR_CROSS_TRANSFER_RACE_ABORT;
    abort.workerId = 0u;
    abort.claimEpoch = 0u;
    claimThread = cross_transfer_thread_start(&claim);
    abortThread = cross_transfer_thread_start(&abort);
    cross_transfer_test_atomic_store(&start, 1);
    cross_transfer_thread_join(claimThread);
    cross_transfer_thread_join(abortThread);
    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)claim.result + (TZrUInt32)abort.result);
    if (claim.result) {
        TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_AbortCrossDomain(
                envelope, g_target_state, 67u, 71u, &diagnostic));
    }
    TEST_ASSERT_EQUAL_UINT32(1u, providerContext.abortCount);
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_claimed_commit_and_abort_have_one_terminal_owner(void) {
    ZrRaceProviderContext providerContext = {0};
    SZrDomainTransferProvider provider = make_provider(&providerContext);
    SZrDomainTransferContract contract = make_resource_contract(&provider);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *envelope;
    SZrTypeValue source;
    SZrTypeValue target;
    ZrCrossTransferTestAtomic start = 0;
    ZrCrossTransferRaceContext commit = {0};
    ZrCrossTransferRaceContext abort = {0};
    ZrCrossTransferTestThread commitThread;
    ZrCrossTransferTestThread abortThread;

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
            envelope, g_target_state, 73u, 79u));
    ZrCore_Value_ResetAsNull(&target);
    commit.envelope = envelope;
    commit.state = g_target_state;
    commit.target = &target;
    commit.start = &start;
    commit.operation = ZR_CROSS_TRANSFER_RACE_COMMIT;
    commit.workerId = 73u;
    commit.claimEpoch = 79u;
    abort = commit;
    abort.target = ZR_NULL;
    abort.operation = ZR_CROSS_TRANSFER_RACE_ABORT;
    commitThread = cross_transfer_thread_start(&commit);
    abortThread = cross_transfer_thread_start(&abort);
    cross_transfer_test_atomic_store(&start, 1);
    cross_transfer_thread_join(commitThread);
    cross_transfer_thread_join(abortThread);
    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)commit.result + (TZrUInt32)abort.result);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)commit.result, providerContext.commitCount);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)abort.result, providerContext.abortCount);
    if (commit.result) {
        TEST_ASSERT_EQUAL_INT(
                ZR_OWNERSHIP_VALUE_KIND_UNIQUE, target.ownershipKind);
        ZrCore_Ownership_ReleaseValue(g_target_state, &target);
        TEST_ASSERT_EQUAL_UINT32(1u, g_resource_drop_count);
    } else {
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(target.type));
        TEST_ASSERT_EQUAL_UINT32(0u, g_resource_drop_count);
    }
    ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
}

static void test_immutable_handle_commits_concurrently_without_consuming_source(void) {
    SZrState *secondTargetState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrDomainTransferProvider provider;
    SZrDomainTransferContract contract = make_immutable_contract(&provider);
    SZrDomainTransferDiagnostic diagnostic;
    SZrOwnershipTransferEnvelope *firstEnvelope;
    SZrOwnershipTransferEnvelope *secondEnvelope;
    SZrTypeValue source;
    SZrTypeValue firstTarget;
    SZrTypeValue secondTarget;
    ZrCrossTransferTestAtomic start = 0;
    ZrCrossTransferRaceContext firstCommit = {0};
    ZrCrossTransferRaceContext secondCommit = {0};
    ZrCrossTransferTestThread firstThread;
    ZrCrossTransferTestThread secondThread;

    TEST_ASSERT_NOT_NULL(secondTargetState);
    ZrCore_Value_InitAsInt(g_source_state, &source, 4242);
    firstEnvelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(g_target_state),
            &contract,
            &source,
            &diagnostic);
    secondEnvelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            g_source_state,
            ZrCore_GcDomain_GetIdentity(secondTargetState),
            &contract,
            &source,
            &diagnostic);
    TEST_ASSERT_NOT_NULL(firstEnvelope);
    TEST_ASSERT_NOT_NULL(secondEnvelope);
    TEST_ASSERT_EQUAL_INT64(4242, source.value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(firstEnvelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(secondEnvelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            firstEnvelope, g_target_state, 89u, 97u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            secondEnvelope, secondTargetState, 101u, 103u));
    ZrCore_Value_ResetAsNull(&firstTarget);
    ZrCore_Value_ResetAsNull(&secondTarget);
    firstCommit.envelope = firstEnvelope;
    firstCommit.state = g_target_state;
    firstCommit.target = &firstTarget;
    firstCommit.start = &start;
    firstCommit.operation = ZR_CROSS_TRANSFER_RACE_COMMIT;
    firstCommit.workerId = 89u;
    firstCommit.claimEpoch = 97u;
    secondCommit.envelope = secondEnvelope;
    secondCommit.state = secondTargetState;
    secondCommit.target = &secondTarget;
    secondCommit.start = &start;
    secondCommit.operation = ZR_CROSS_TRANSFER_RACE_COMMIT;
    secondCommit.workerId = 101u;
    secondCommit.claimEpoch = 103u;
    firstThread = cross_transfer_thread_start(&firstCommit);
    secondThread = cross_transfer_thread_start(&secondCommit);
    cross_transfer_test_atomic_store(&start, 1);
    cross_transfer_thread_join(firstThread);
    cross_transfer_thread_join(secondThread);
    TEST_ASSERT_TRUE(firstCommit.result);
    TEST_ASSERT_TRUE(secondCommit.result);
    TEST_ASSERT_EQUAL_INT64(4242, firstTarget.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(4242, secondTarget.value.nativeObject.nativeInt64);
    ZrCore_Value_ResetAsNull(&firstTarget);
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_CommitCrossDomain(
            firstEnvelope,
            g_target_state,
            89u,
            97u,
            &firstTarget,
            &diagnostic));
    ZrCore_OwnershipTransfer_Free(g_source_state, firstEnvelope);
    ZrCore_OwnershipTransfer_Free(g_source_state, secondEnvelope);
    ZrTests_Runtime_State_Destroy(secondTargetState);
}

static void test_release_publish_acquire_claim_litmus_never_observes_partial_value(void) {
    SZrDomainTransferContract contract = make_value_copy_contract();
    SZrDomainTransferDiagnostic diagnostic;

    for (TZrUInt32 iteration = 0u; iteration < 64u; iteration++) {
        SZrOwnershipTransferEnvelope *envelope;
        SZrTypeValue source;
        SZrTypeValue target;
        ZrCrossTransferTestAtomic start = 0;
        ZrCrossTransferRaceContext publish = {0};
        ZrCrossTransferTestThread publishThread;
        TZrBool claimed = ZR_FALSE;

        ZrCore_Value_InitAsInt(
                g_source_state,
                &source,
                (TZrInt64)UINT64_C(0x10203040) + iteration);
        envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
                g_source_state,
                ZrCore_GcDomain_GetIdentity(g_target_state),
                &contract,
                &source,
                &diagnostic);
        TEST_ASSERT_NOT_NULL(envelope);
        publish.envelope = envelope;
        publish.start = &start;
        publish.operation = ZR_CROSS_TRANSFER_RACE_PUBLISH;
        publishThread = cross_transfer_thread_start(&publish);
        cross_transfer_test_atomic_store(&start, 1);
        for (TZrUInt32 attempt = 0u; attempt < 100000u && !claimed; attempt++) {
            claimed = ZrCore_OwnershipTransfer_Claim(
                    envelope, g_target_state, 83u, iteration + 1u);
            if (!claimed) {
                cross_transfer_test_yield();
            }
        }
        cross_transfer_thread_join(publishThread);
        TEST_ASSERT_TRUE(publish.result);
        TEST_ASSERT_TRUE(claimed);
        ZrCore_Value_ResetAsNull(&target);
        TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_CommitCrossDomain(
                envelope,
                g_target_state,
                83u,
                iteration + 1u,
                &target,
                &diagnostic));
        TEST_ASSERT_EQUAL_INT64(
                (TZrInt64)UINT64_C(0x10203040) + iteration,
                target.value.nativeObject.nativeInt64);
        ZrCore_OwnershipTransfer_Free(g_source_state, envelope);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prepared_publish_and_abort_are_linearized);
    RUN_TEST(test_queued_claim_and_abort_have_one_winner);
    RUN_TEST(test_claimed_commit_and_abort_have_one_terminal_owner);
    RUN_TEST(test_immutable_handle_commits_concurrently_without_consuming_source);
    RUN_TEST(test_release_publish_acquire_claim_litmus_never_observes_partial_value);
    return UNITY_END();
}
