#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/gc_domain_internal.h"
#include "ownership_transfer_internal.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/gc_domain_clone.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

enum {
    clone_worker_id = 31u,
    clone_claim_epoch = 37u,
    clone_retry_worker_id = 41u,
    clone_retry_claim_epoch = 43u,
    max_major_finish_steps = 4096u
};

typedef enum EZrCloneOomRecovery {
    clone_abort_and_prepare_again,
    clone_retry_same_claim,
    clone_execute_cleanup
} EZrCloneOomRecovery;

typedef struct SZrCloneEnvelopeAllocatorContext {
    FZrAllocator delegate;
    TZrPtr delegateArguments;
    SZrOwnershipTransferEnvelope *liveEnvelope;
    TZrUInt32 allocations;
    TZrUInt32 releases;
    TZrBool duplicateLiveEnvelope;
    TZrBool installed;
} SZrCloneEnvelopeAllocatorContext;

typedef struct SZrCloneOomAllocatorContext {
    FZrAllocator delegate;
    TZrPtr delegateArguments;
    SZrState *targetState;
    TZrSize baselineRootCount;
    TZrSize rootCountAtFailure;
    TZrUInt32 matchingFailureCount;
    TZrBool armed;
} SZrCloneOomAllocatorContext;

typedef struct SZrCloneCommitTryContext {
    SZrGcDomainCloneTransaction *transaction;
    const SZrTypeValue *source;
    SZrDomainTransferQuota quota;
    SZrTypeValue *target;
    SZrDomainTransferDiagnostic *diagnostic;
    TZrBool commitReturned;
} SZrCloneCommitTryContext;

static SZrState *g_sourceState;
static SZrState *g_targetState;
static SZrCloneOomAllocatorContext g_oomAllocator;
static SZrCloneEnvelopeAllocatorContext g_envelopeAllocator;
static TZrBool g_targetAllocatorWrapped;
static SZrGcRootHandle g_sourceRootHandle;
static SZrGcRootHandle g_sourceChildHandle;
static SZrGcRootHandle g_sourceLeftKeyHandle;
static SZrGcRootHandle g_sourceRightKeyHandle;
static SZrGcRootHandle g_sourceParentKeyHandle;
static SZrGcRootHandle g_sourceAfterOomKeyHandle;
static SZrGcRootHandle g_targetLeftKeyHandle;
static SZrGcRootHandle g_targetRightKeyHandle;
static SZrGcRootHandle g_targetParentKeyHandle;
static SZrGcRootHandle g_targetAfterOomKeyHandle;
static SZrGcRootHandle g_targetRootHandle;

static SZrObject *new_plain_object(SZrState *state) {
    SZrObject *object = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    return object;
}

static TZrBool resolve_root_handle(
        SZrState *state,
        SZrGcRootHandle *handle,
        SZrRawObject **outObject) {
    if (outObject != ZR_NULL) {
        *outObject = ZR_NULL;
    }
    return ZrCore_GcRootHandle_Resolve(state, handle, outObject);
}

static TZrBool root_handle_is_set(const SZrGcRootHandle *handle) {
    return (TZrBool)(handle != ZR_NULL &&
                     handle->domain.id != 0u &&
                     handle->domain.generation != 0u &&
                     handle->slotGeneration != 0u);
}

static SZrString *new_rooted_string(
        SZrState *state,
        TZrNativeString nativeString,
        SZrGcRootHandle *handle) {
    SZrString *string = ZrCore_String_CreateFromNative(state, nativeString);
    TEST_ASSERT_NOT_NULL(string);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(string), handle));
    return string;
}

static SZrString *resolve_string_handle(
        SZrState *state, SZrGcRootHandle *handle) {
    SZrRawObject *resolved = ZR_NULL;
    TEST_ASSERT_TRUE(resolve_root_handle(state, handle, &resolved));
    return ZR_CAST_STRING(state, resolved);
}

static void init_string_value(
        SZrState *state, SZrString *string, SZrTypeValue *value) {
    ZrCore_Value_InitAsRawObject(
            state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value->type = ZR_VALUE_TYPE_STRING;
}

static TZrUInt32 mutation_depth_for_state(SZrState *state) {
    SZrGcDomain *domain;
    TZrUInt32 depth = ~(TZrUInt32)0u;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return depth;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
        if (domain->mutators[index].state == state) {
            depth = domain->mutators[index].mutationDepth;
            break;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    return depth;
}

static TZrPtr fail_target_hash_pair_allocations(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    SZrCloneOomAllocatorContext *context =
            (SZrCloneOomAllocatorContext *)userData;

    if (context != ZR_NULL && context->armed && pointer == ZR_NULL &&
        originalSize == 0u &&
        newSize == sizeof(SZrHashKeyValuePair) &&
        flag == (TZrInt64)ZR_MEMORY_NATIVE_TYPE_HASH_PAIR) {
        TZrSize rootCount =
                ZrCore_GcDomain_GetRootCount(context->targetState);
        context->matchingFailureCount++;
        if (rootCount > context->rootCountAtFailure) {
            context->rootCountAtFailure = rootCount;
        }
        /* Fail the allocation and its GcMalloc retry, then let Throw build
         * the MEMORY_ERROR object so TryRun can reach its recovery point. */
        if (context->matchingFailureCount >= 2u) {
            context->armed = ZR_FALSE;
        }
        return ZR_NULL;
    }

    return context->delegate(
            context->delegateArguments,
            pointer,
            originalSize,
            newSize,
            flag);
}

static void install_target_allocator_failure(void) {
    SZrGlobalState *global = g_targetState->global;

    memset(&g_oomAllocator, 0, sizeof(g_oomAllocator));
    g_oomAllocator.delegate = global->upstreamAllocator;
    g_oomAllocator.delegateArguments = global->upstreamAllocationArguments;
    g_oomAllocator.targetState = g_targetState;
    g_oomAllocator.baselineRootCount =
            ZrCore_GcDomain_GetRootCount(g_targetState);
    global->upstreamAllocator = fail_target_hash_pair_allocations;
    global->upstreamAllocationArguments = &g_oomAllocator;
    g_targetAllocatorWrapped = ZR_TRUE;
}

static void restore_target_allocator(void) {
    if (!g_targetAllocatorWrapped || g_targetState == ZR_NULL) {
        return;
    }
    g_oomAllocator.armed = ZR_FALSE;
    g_targetState->global->upstreamAllocator = g_oomAllocator.delegate;
    g_targetState->global->upstreamAllocationArguments =
            g_oomAllocator.delegateArguments;
    g_targetAllocatorWrapped = ZR_FALSE;
}

static TZrPtr observe_source_envelope_allocations(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    SZrCloneEnvelopeAllocatorContext *context = (SZrCloneEnvelopeAllocatorContext *)userData;
    TZrPtr result;
    if (pointer != ZR_NULL && pointer == context->liveEnvelope && newSize == 0u) {
        ++context->releases;
        context->liveEnvelope = ZR_NULL;
    }
    result = context->delegate(context->delegateArguments, pointer, originalSize, newSize, flag);
    if (pointer == ZR_NULL && originalSize == 0u && result != ZR_NULL &&
        newSize == sizeof(SZrOwnershipTransferEnvelope) &&
        flag == (TZrInt64)ZR_MEMORY_NATIVE_TYPE_MANAGER) {
        ++context->allocations;
        if (context->liveEnvelope != ZR_NULL) context->duplicateLiveEnvelope = ZR_TRUE;
        context->liveEnvelope = (SZrOwnershipTransferEnvelope *)result;
    }
    return result;
}

static void install_source_envelope_observer(void) {
    SZrGlobalState *global = g_sourceState->global;
    memset(&g_envelopeAllocator, 0, sizeof(g_envelopeAllocator));
    g_envelopeAllocator.delegate = global->allocator;
    g_envelopeAllocator.delegateArguments = global->userAllocationArguments;
    global->allocator = observe_source_envelope_allocations;
    global->userAllocationArguments = &g_envelopeAllocator;
    g_envelopeAllocator.installed = ZR_TRUE;
}

static void restore_source_envelope_observer(void) {
    if (!g_envelopeAllocator.installed) return;
    if (g_envelopeAllocator.liveEnvelope != ZR_NULL) {
        ZrCore_OwnershipTransfer_Free(g_sourceState, g_envelopeAllocator.liveEnvelope);
    }
    g_sourceState->global->allocator = g_envelopeAllocator.delegate;
    g_sourceState->global->userAllocationArguments = g_envelopeAllocator.delegateArguments;
    g_envelopeAllocator.installed = ZR_FALSE;
}

static void clone_commit_try_body(SZrState *state, TZrPtr arguments) {
    SZrCloneCommitTryContext *context =
            (SZrCloneCommitTryContext *)arguments;
    if (context->transaction != ZR_NULL) {
        context->commitReturned = ZrCore_GcDomainClone_Commit(
                context->transaction, context->target, context->diagnostic);
    } else {
        context->commitReturned = ZrCore_GcDomainClone_Execute(
                g_sourceState, state, context->source, &context->quota,
                clone_worker_id, clone_claim_epoch, context->target, context->diagnostic);
    }
}

static TZrBool finish_target_concurrent_major(void) {
    SZrGarbageCollector *collector = g_targetState->global->garbageCollector;

    for (TZrUInt32 step = 0u;
         collector->concurrentMajorActive && step < max_major_finish_steps;
         ++step) {
        ZrCore_GarbageCollector_GcStep(g_targetState);
    }
    return (TZrBool)!collector->concurrentMajorActive;
}

static SZrGcDomainCloneTransaction *prepare_and_claim_clone(
        SZrTypeValue *sourceValue,
        TZrUInt32 workerId,
        TZrUInt32 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrDomainTransferQuota quota = {0};
    SZrGcDomainCloneTransaction *transaction;

    quota.maxObjects = 16u;
    quota.maxBytes = 4096u;
    quota.maxDepth = 32u;
    transaction = ZrCore_GcDomainClone_Prepare(
            g_sourceState,
            g_targetState,
            sourceValue,
            &quota,
            diagnostic);
    TEST_ASSERT_NOT_NULL(transaction);
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic->status);
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Publish(transaction, diagnostic));
    TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Claim(
            transaction, workerId, claimEpoch, diagnostic));
    return transaction;
}

void setUp(void) {
    g_sourceState = ZrTests_Runtime_State_Create(ZR_NULL);
    g_targetState = ZrTests_Runtime_State_Create(ZR_NULL);
    memset(&g_oomAllocator, 0, sizeof(g_oomAllocator));
    memset(&g_envelopeAllocator, 0, sizeof(g_envelopeAllocator));
    memset(&g_sourceRootHandle, 0, sizeof(g_sourceRootHandle));
    memset(&g_sourceChildHandle, 0, sizeof(g_sourceChildHandle));
    memset(&g_sourceLeftKeyHandle, 0, sizeof(g_sourceLeftKeyHandle));
    memset(&g_sourceRightKeyHandle, 0, sizeof(g_sourceRightKeyHandle));
    memset(&g_sourceParentKeyHandle, 0, sizeof(g_sourceParentKeyHandle));
    memset(&g_sourceAfterOomKeyHandle, 0, sizeof(g_sourceAfterOomKeyHandle));
    memset(&g_targetLeftKeyHandle, 0, sizeof(g_targetLeftKeyHandle));
    memset(&g_targetRightKeyHandle, 0, sizeof(g_targetRightKeyHandle));
    memset(&g_targetParentKeyHandle, 0, sizeof(g_targetParentKeyHandle));
    memset(&g_targetAfterOomKeyHandle, 0, sizeof(g_targetAfterOomKeyHandle));
    memset(&g_targetRootHandle, 0, sizeof(g_targetRootHandle));
    g_targetAllocatorWrapped = ZR_FALSE;
    TEST_ASSERT_NOT_NULL(g_sourceState);
    TEST_ASSERT_NOT_NULL(g_targetState);
}

void tearDown(void) {
    restore_target_allocator();
    restore_source_envelope_observer();
    if (g_targetState != ZR_NULL) {
        TZrUInt32 mutationDepth = mutation_depth_for_state(g_targetState);
        if (mutationDepth != 0u) {
            fprintf(stderr,
                    "cross-domain clone OOM tearDown refused target cleanup "
                    "with mutation depth %u\n",
                    (unsigned int)mutationDepth);
            fflush(stderr);
            _Exit(EXIT_FAILURE);
        }
        if (!finish_target_concurrent_major()) {
            fprintf(stderr,
                    "cross-domain clone OOM tearDown refused target cleanup "
                    "while concurrent major remained active\n");
            fflush(stderr);
            _Exit(EXIT_FAILURE);
        }
        if (root_handle_is_set(&g_targetAfterOomKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_targetState, &g_targetAfterOomKeyHandle);
        }
        if (root_handle_is_set(&g_targetParentKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_targetState, &g_targetParentKeyHandle);
        }
        if (root_handle_is_set(&g_targetRightKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_targetState, &g_targetRightKeyHandle);
        }
        if (root_handle_is_set(&g_targetLeftKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_targetState, &g_targetLeftKeyHandle);
        }
        if (root_handle_is_set(&g_targetRootHandle)) {
            ZrCore_GcRootHandle_Release(g_targetState, &g_targetRootHandle);
        }
        ZrTests_Runtime_State_Destroy(g_targetState);
        g_targetState = ZR_NULL;
    }
    if (g_sourceState != ZR_NULL) {
        if (root_handle_is_set(&g_sourceAfterOomKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_sourceState, &g_sourceAfterOomKeyHandle);
        }
        if (root_handle_is_set(&g_sourceParentKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_sourceState, &g_sourceParentKeyHandle);
        }
        if (root_handle_is_set(&g_sourceRightKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_sourceState, &g_sourceRightKeyHandle);
        }
        if (root_handle_is_set(&g_sourceLeftKeyHandle)) {
            ZrCore_GcRootHandle_Release(
                    g_sourceState, &g_sourceLeftKeyHandle);
        }
        if (root_handle_is_set(&g_sourceChildHandle)) {
            ZrCore_GcRootHandle_Release(g_sourceState, &g_sourceChildHandle);
        }
        if (root_handle_is_set(&g_sourceRootHandle)) {
            ZrCore_GcRootHandle_Release(g_sourceState, &g_sourceRootHandle);
        }
        ZrTests_Runtime_State_Destroy(g_sourceState);
        g_sourceState = ZR_NULL;
    }
}

static void run_clone_target_oom_recovery(EZrCloneOomRecovery recovery) {
    SZrObject *sourceRoot = new_plain_object(g_sourceState);
    SZrObject *sourceChild;
    SZrString *leftName;
    SZrString *rightName;
    SZrString *parentName;
    SZrString *afterOomName;
    SZrRawObject *resolvedRoot = ZR_NULL;
    SZrRawObject *resolvedChild = ZR_NULL;
    SZrTypeValue sourceRootValue;
    SZrTypeValue sourceChildValue;
    SZrTypeValue leftKey;
    SZrTypeValue rightKey;
    SZrTypeValue parentKey;
    SZrTypeValue afterOomKey;
    SZrTypeValue sourceMarker;
    SZrTypeValue targetValue;
    SZrDomainTransferDiagnostic diagnostic;
    SZrGcDomainCloneTransaction *transaction = ZR_NULL;
    SZrCloneCommitTryContext tryContext;
    EZrThreadStatus tryStatus;
    EZrThreadStatus targetThreadStatusAfterTryRun;
    TZrSize baselineTargetRootCount;
    TZrSize rootsAfterFailure;
    TZrUInt32 mutationDepthBefore;
    TZrUInt32 mutationDepthAfter;
    const SZrTypeValue *member;

    /* Keep target key decoding allocation-free so the injected pair OOM is
     * reached inside Object_SetValue after the graph roots have been created. */
    (void)new_rooted_string(
            g_targetState, "left", &g_targetLeftKeyHandle);
    (void)new_rooted_string(
            g_targetState, "right", &g_targetRightKeyHandle);
    (void)new_rooted_string(
            g_targetState, "parent", &g_targetParentKeyHandle);
    (void)new_rooted_string(
            g_targetState, "after-oom", &g_targetAfterOomKeyHandle);

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_sourceState,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceRoot),
            &g_sourceRootHandle));
    sourceChild = new_plain_object(g_sourceState);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_sourceState,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceChild),
            &g_sourceChildHandle));

    leftName = new_rooted_string(
            g_sourceState, "left", &g_sourceLeftKeyHandle);
    rightName = new_rooted_string(
            g_sourceState, "right", &g_sourceRightKeyHandle);
    parentName = new_rooted_string(
            g_sourceState, "parent", &g_sourceParentKeyHandle);
    afterOomName = new_rooted_string(
            g_sourceState, "after-oom", &g_sourceAfterOomKeyHandle);

    leftName = resolve_string_handle(
            g_sourceState, &g_sourceLeftKeyHandle);
    rightName = resolve_string_handle(
            g_sourceState, &g_sourceRightKeyHandle);
    parentName = resolve_string_handle(
            g_sourceState, &g_sourceParentKeyHandle);
    afterOomName = resolve_string_handle(
            g_sourceState, &g_sourceAfterOomKeyHandle);

    TEST_ASSERT_TRUE(resolve_root_handle(
            g_sourceState, &g_sourceRootHandle, &resolvedRoot));
    TEST_ASSERT_TRUE(resolve_root_handle(
            g_sourceState, &g_sourceChildHandle, &resolvedChild));
    sourceRoot = ZR_CAST_OBJECT(g_sourceState, resolvedRoot);
    sourceChild = ZR_CAST_OBJECT(g_sourceState, resolvedChild);
    init_string_value(g_sourceState, leftName, &leftKey);
    init_string_value(g_sourceState, rightName, &rightKey);
    init_string_value(g_sourceState, parentName, &parentKey);
    init_string_value(g_sourceState, afterOomName, &afterOomKey);
    ZrCore_Value_InitAsRawObject(
            g_sourceState,
            &sourceRootValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceRoot));
    sourceRootValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(
            g_sourceState,
            &sourceChildValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceChild));
    sourceChildValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(
            g_sourceState, sourceRoot, &leftKey, &sourceChildValue);
    ZrCore_Object_SetValue(
            g_sourceState, sourceRoot, &rightKey, &sourceChildValue);
    ZrCore_Object_SetValue(
            g_sourceState, sourceChild, &parentKey, &sourceRootValue);

    TEST_ASSERT_TRUE(resolve_root_handle(
            g_sourceState, &g_sourceRootHandle, &resolvedRoot));
    sourceRoot = ZR_CAST_OBJECT(g_sourceState, resolvedRoot);
    ZrCore_Value_InitAsRawObject(
            g_sourceState,
            &sourceRootValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceRoot));
    sourceRootValue.type = ZR_VALUE_TYPE_OBJECT;
    if (recovery == clone_execute_cleanup) {
        install_source_envelope_observer();
    } else {
        transaction = prepare_and_claim_clone(
                &sourceRootValue, clone_worker_id, clone_claim_epoch, &diagnostic);
    }

    g_targetState->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_targetState->global,
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_targetState);
    TEST_ASSERT_TRUE(
            g_targetState->global->garbageCollector->concurrentMajorActive);
    baselineTargetRootCount =
            ZrCore_GcDomain_GetRootCount(g_targetState);
    g_oomAllocator.baselineRootCount = baselineTargetRootCount;
    mutationDepthBefore = mutation_depth_for_state(g_targetState);
    TEST_ASSERT_EQUAL_UINT32(0u, mutationDepthBefore);
    install_target_allocator_failure();
    g_oomAllocator.armed = ZR_TRUE;

    ZrCore_Value_ResetAsNull(&targetValue);
    memset(&tryContext, 0, sizeof(tryContext));
    tryContext.transaction = transaction;
    tryContext.source = &sourceRootValue;
    tryContext.quota.maxObjects = 16u;
    tryContext.quota.maxBytes = 4096u;
    tryContext.quota.maxDepth = 32u;
    tryContext.target = &targetValue;
    tryContext.diagnostic = &diagnostic;
    tryStatus = ZrCore_Exception_TryRun(
            g_targetState, clone_commit_try_body, &tryContext);
    targetThreadStatusAfterTryRun = g_targetState->threadStatus;
    g_oomAllocator.armed = ZR_FALSE;
    restore_target_allocator();

    rootsAfterFailure = ZrCore_GcDomain_GetRootCount(g_targetState);
    mutationDepthAfter = mutation_depth_for_state(g_targetState);
    (void)fprintf(stderr,
                  "clone OOM recovery: roots %llu->%llu mutation %u->%u status %d failures %u\n",
                  (unsigned long long)baselineTargetRootCount,
                  (unsigned long long)rootsAfterFailure,
                  (unsigned)mutationDepthBefore, (unsigned)mutationDepthAfter,
                  (int)tryStatus, (unsigned)g_oomAllocator.matchingFailureCount);
    if (mutationDepthAfter != mutationDepthBefore) {
        fprintf(stderr,
                "cross-domain clone OOM left target mutation depth at %u "
                "(baseline %u), roots %llu (baseline %llu), TryRun status %d, "
                "matching allocator failures %u; exiting before unsafe "
                "transaction/state cleanup\n",
                (unsigned int)mutationDepthAfter,
                (unsigned int)mutationDepthBefore,
                (unsigned long long)rootsAfterFailure,
                (unsigned long long)baselineTargetRootCount,
                (int)tryStatus,
                (unsigned int)g_oomAllocator.matchingFailureCount);
        fflush(stderr);
        _Exit(EXIT_FAILURE);
    }
    ZrCore_Exception_ClearCurrent(g_targetState);
    g_targetState->threadStatus = ZR_THREAD_STATUS_FINE;

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_MEMORY_ERROR, tryStatus);
    TEST_ASSERT_EQUAL_INT(
            ZR_THREAD_STATUS_MEMORY_ERROR, targetThreadStatusAfterTryRun);
    TEST_ASSERT_FALSE(tryContext.commitReturned);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(targetValue.type));
    TEST_ASSERT_TRUE(g_oomAllocator.matchingFailureCount >= 2u);
    TEST_ASSERT_TRUE(
            g_oomAllocator.rootCountAtFailure > baselineTargetRootCount);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)baselineTargetRootCount,
            (TZrUInt32)rootsAfterFailure);
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED, diagnostic.status);
    if (recovery == clone_execute_cleanup) {
        fprintf(stderr, "clone Execute envelope recovery: allocated %u released %u live %u\n",
                (unsigned)g_envelopeAllocator.allocations,
                (unsigned)g_envelopeAllocator.releases,
                (unsigned)(g_envelopeAllocator.liveEnvelope != ZR_NULL));
        TEST_ASSERT_EQUAL_UINT32(1u, g_envelopeAllocator.allocations);
        TEST_ASSERT_EQUAL_UINT32(1u, g_envelopeAllocator.releases);
        TEST_ASSERT_NULL(g_envelopeAllocator.liveEnvelope);
        TEST_ASSERT_FALSE(g_envelopeAllocator.duplicateLiveEnvelope);
    } else {
        SZrOwnershipTransferSnapshot snapshot;
        TEST_ASSERT_TRUE(ZrCore_GcDomainClone_GetSnapshot(transaction, &snapshot));
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED, snapshot.state);
    }

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, g_sourceState->threadStatus);
    TEST_ASSERT_TRUE(resolve_root_handle(
            g_sourceState, &g_sourceRootHandle, &resolvedRoot));
    TEST_ASSERT_TRUE(resolve_root_handle(
            g_sourceState, &g_sourceChildHandle, &resolvedChild));
    sourceRoot = ZR_CAST_OBJECT(g_sourceState, resolvedRoot);
    sourceChild = ZR_CAST_OBJECT(g_sourceState, resolvedChild);
    leftName = resolve_string_handle(
            g_sourceState, &g_sourceLeftKeyHandle);
    rightName = resolve_string_handle(
            g_sourceState, &g_sourceRightKeyHandle);
    parentName = resolve_string_handle(
            g_sourceState, &g_sourceParentKeyHandle);
    afterOomName = resolve_string_handle(
            g_sourceState, &g_sourceAfterOomKeyHandle);
    init_string_value(g_sourceState, leftName, &leftKey);
    init_string_value(g_sourceState, rightName, &rightKey);
    init_string_value(g_sourceState, parentName, &parentKey);
    init_string_value(g_sourceState, afterOomName, &afterOomKey);
    member = ZrCore_Object_GetValue(g_sourceState, sourceRoot, &leftKey);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_PTR(sourceChild, member->value.object);
    member = ZrCore_Object_GetValue(g_sourceState, sourceRoot, &rightKey);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_PTR(sourceChild, member->value.object);
    member = ZrCore_Object_GetValue(g_sourceState, sourceChild, &parentKey);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_PTR(sourceRoot, member->value.object);

    ZrCore_Value_InitAsInt(g_sourceState, &sourceMarker, 31337);
    ZrCore_Object_SetValue(
            g_sourceState, sourceRoot, &afterOomKey, &sourceMarker);
    afterOomName = resolve_string_handle(
            g_sourceState, &g_sourceAfterOomKeyHandle);
    init_string_value(g_sourceState, afterOomName, &afterOomKey);
    member = ZrCore_Object_GetValue(g_sourceState, sourceRoot, &afterOomKey);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_INT64(31337, member->value.nativeObject.nativeInt64);

    TEST_ASSERT_TRUE(finish_target_concurrent_major());
    if (recovery == clone_abort_and_prepare_again) {
        TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Abort(transaction, &diagnostic));
        TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
        ZrCore_GcDomainClone_Free(transaction);
        TEST_ASSERT_EQUAL_UINT32(
                (TZrUInt32)baselineTargetRootCount,
                (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_targetState));

    }
    if (recovery != clone_retry_same_claim) {
        TEST_ASSERT_TRUE(resolve_root_handle(
                g_sourceState, &g_sourceRootHandle, &resolvedRoot));
        sourceRoot = ZR_CAST_OBJECT(g_sourceState, resolvedRoot);
        ZrCore_Value_InitAsRawObject(
                g_sourceState, &sourceRootValue, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceRoot));
        sourceRootValue.type = ZR_VALUE_TYPE_OBJECT;
        if (recovery == clone_abort_and_prepare_again) {
            transaction = prepare_and_claim_clone(
                    &sourceRootValue, clone_retry_worker_id, clone_retry_claim_epoch, &diagnostic);
        }
    }
    ZrCore_Value_ResetAsNull(&targetValue);
    if (recovery == clone_execute_cleanup) {
        TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Execute(
                g_sourceState, g_targetState, &sourceRootValue, &tryContext.quota,
                clone_retry_worker_id, clone_retry_claim_epoch, &targetValue, &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(2u, g_envelopeAllocator.allocations);
        TEST_ASSERT_EQUAL_UINT32(2u, g_envelopeAllocator.releases);
        TEST_ASSERT_NULL(g_envelopeAllocator.liveEnvelope);
        TEST_ASSERT_FALSE(g_envelopeAllocator.duplicateLiveEnvelope);
    } else {
        TEST_ASSERT_TRUE(ZrCore_GcDomainClone_Commit(
                transaction, &targetValue, &diagnostic));
    }
    TEST_ASSERT_EQUAL_INT(ZR_DOMAIN_TRANSFER_STATUS_OK, diagnostic.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, targetValue.type);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_targetState,
            targetValue.value.object,
            &g_targetRootHandle));
    ZrCore_GcDomainClone_Free(transaction);

    {
        SZrObject *targetRoot = ZR_CAST_OBJECT(g_targetState, targetValue.value.object);
        SZrObject *targetChild;
        init_string_value(g_targetState,
                resolve_string_handle(g_targetState, &g_targetLeftKeyHandle), &leftKey);
        init_string_value(g_targetState,
                resolve_string_handle(g_targetState, &g_targetRightKeyHandle), &rightKey);
        init_string_value(g_targetState,
                resolve_string_handle(g_targetState, &g_targetParentKeyHandle), &parentKey);
        member = ZrCore_Object_GetValue(g_targetState, targetRoot, &leftKey);
        TEST_ASSERT_NOT_NULL(member);
        targetChild = ZR_CAST_OBJECT(g_targetState, member->value.object);
        TEST_ASSERT_NOT_NULL(targetChild);
        TEST_ASSERT_NOT_EQUAL(sourceRoot, targetRoot);
        TEST_ASSERT_NOT_EQUAL(sourceChild, targetChild);
        member = ZrCore_Object_GetValue(g_targetState, targetRoot, &rightKey);
        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_PTR(targetChild, member->value.object);
        member = ZrCore_Object_GetValue(g_targetState, targetChild, &parentKey);
        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_PTR(targetRoot, member->value.object);
    }

    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(baselineTargetRootCount + 1u),
            (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_targetState));
}

static void test_clone_target_allocation_oom_is_abortable_and_retryable(void) {
    run_clone_target_oom_recovery(clone_abort_and_prepare_again);
}

static void test_clone_target_allocation_oom_can_retry_the_same_claim(void) {
    run_clone_target_oom_recovery(clone_retry_same_claim);
}

static void test_clone_execute_target_oom_closes_hidden_transaction(void) {
    run_clone_target_oom_recovery(clone_execute_cleanup);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clone_target_allocation_oom_is_abortable_and_retryable);
    RUN_TEST(test_clone_target_allocation_oom_can_retry_the_same_claim);
    RUN_TEST(test_clone_execute_target_oom_closes_hidden_transaction);
    return UNITY_END();
}
