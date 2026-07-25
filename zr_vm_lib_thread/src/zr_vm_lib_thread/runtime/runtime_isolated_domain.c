#include "runtime/runtime_internal.h"

#if defined(ZR_PLATFORM_WIN)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_lib_thread/module.h"

static const TZrChar *kThreadSchedulerIsolatedRuntimeField = "__zr_thread_scheduler_isolated_runtime";
static const TZrChar *kThreadSchedulerIsolatedRuntimeHeadField =
        "__zr_thread_scheduler_isolated_runtime_head";
static const TZrChar *kThreadSchedulerIsolatedShutdownField =
        "__zr_thread_scheduler_isolated_shutdown";

typedef struct ZrVmIsolatedDomainLaunch ZrVmIsolatedDomainLaunch;

typedef struct ZrVmIsolatedDomainRuntime {
    SZrObject *scheduler;
    ZrVmTaskMutex mutex;
    TZrUInt32 workerLimit;
    TZrUInt32 liveWorkerCount;
    ZrVmIsolatedDomainLaunch *pendingHead;
    ZrVmIsolatedDomainLaunch *pendingTail;
    struct ZrVmIsolatedDomainRuntime *nextRegistered;
    SZrDomainTransferQuota transferQuota;
    ZrLibraryTaskRuntimeAwaitRegistration awaitRegistration;
} ZrVmIsolatedDomainRuntime;

struct ZrVmIsolatedDomainLaunch {
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    ZrVmTaskWorkerLaunch *artifact;
    ZrVmTaskSchedulerMessage *notificationMessage;
    ZrLibraryTaskRuntimeWorkItem workItem;
    ZrVmTaskSchedulerRuntime *ownerRuntime;
    SZrGcDomainIdentity callerDomain;
    SZrGcDomainIdentity workerDomain;
    SZrDomainTransferQuota transferQuota;
    SZrOwnershipTransferEnvelope *requestEnvelope;
    SZrOwnershipTransferEnvelope **captureEnvelopes;
    TZrUInt32 captureCount;
    TZrBool workerReady;
    TZrBool startupFailed;
    TZrBool requestReady;
    TZrBool cancelled;
    TZrBool completionProcessed;
    TZrBool completionSucceeded;
    TZrBool completionWorkerMustDisposeEnvelope;
    TZrBool workerSlotActive;
    TZrBool workerSlotReleased;
    ZrVmIsolatedDomainRuntime *providerRuntime;
    ZrVmIsolatedDomainLaunch *next;
};

static SZrDomainTransferContract zr_vm_thread_isolated_value_copy_contract(void) {
    SZrDomainTransferContract contract;

    memset(&contract, 0, sizeof(contract));
    contract.kind = ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY;
    contract.schemaVersion = 1u;
    contract.schemaHash = 0x49534F4C41544544ULL;
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    return contract;
}

static SZrDomainTransferContract zr_vm_thread_isolated_contract_for_value(
        const SZrDomainTransferQuota *transferQuota,
        const SZrTypeValue *value) {
    SZrDomainTransferContract contract = zr_vm_thread_isolated_value_copy_contract();

    if (value != ZR_NULL &&
        (value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
         value->ownershipControl != ZR_NULL || value->ownershipWeakRef != ZR_NULL)) {
        /* Resource/immutable providers must come from canonical type metadata. */
        contract.kind = ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN;
        contract.schemaHash = 0x49534F464F524249ULL;
    } else if (value != ZR_NULL && value->isGarbageCollectable) {
        contract.kind = ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE;
        contract.schemaHash = 0x49534F4C434C4F4EULL;
        if (transferQuota != ZR_NULL) {
            contract.quota = *transferQuota;
        }
    }
    return contract;
}

static void zr_vm_thread_isolated_release_capture_envelopes(
        SZrState *state,
        ZrVmIsolatedDomainLaunch *launch) {
    TZrUInt32 captureIndex;

    if (state == ZR_NULL || launch == ZR_NULL || launch->captureEnvelopes == ZR_NULL) {
        return;
    }
    for (captureIndex = 0u; captureIndex < launch->captureCount; captureIndex++) {
        if (launch->captureEnvelopes[captureIndex] != ZR_NULL) {
            ZrCore_OwnershipTransfer_Free(state, launch->captureEnvelopes[captureIndex]);
            launch->captureEnvelopes[captureIndex] = ZR_NULL;
        }
    }
}

void zr_vm_thread_isolated_abort_pending_caller_transfers(
        SZrState *state,
        TZrPtr completionContext) {
    ZrVmIsolatedDomainLaunch *launch = (ZrVmIsolatedDomainLaunch *)completionContext;
    TZrUInt32 captureIndex;

    if (state == ZR_NULL || launch == ZR_NULL) {
        return;
    }
    if (launch->requestEnvelope != ZR_NULL) {
        ZrCore_OwnershipTransfer_Free(state, launch->requestEnvelope);
        launch->requestEnvelope = ZR_NULL;
    }
    if (launch->captureEnvelopes == ZR_NULL) {
        return;
    }
    for (captureIndex = 0u; captureIndex < launch->captureCount; captureIndex++) {
        if (launch->captureEnvelopes[captureIndex] != ZR_NULL) {
            ZrCore_OwnershipTransfer_Free(state, launch->captureEnvelopes[captureIndex]);
            launch->captureEnvelopes[captureIndex] = ZR_NULL;
        }
    }
}

static ZrVmIsolatedDomainRuntime *zr_vm_thread_isolated_scheduler_runtime(
        SZrState *state,
        SZrObject *scheduler) {
    const SZrTypeValue *runtimeValue;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_NULL;
    }
    runtimeValue = zr_vm_task_get_field_value(state, scheduler, kThreadSchedulerIsolatedRuntimeField);
    if (runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        runtimeValue->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }
    return (ZrVmIsolatedDomainRuntime *)runtimeValue->value.nativeObject.nativePointer;
}

static void zr_vm_thread_isolated_launch_signal_ready(
        ZrVmIsolatedDomainLaunch *launch,
        TZrBool startupFailed,
        SZrGcDomainIdentity workerDomain) {
    zr_vm_task_sync_mutex_lock(&launch->mutex);
    launch->startupFailed = startupFailed;
    launch->workerDomain = workerDomain;
    launch->workerReady = ZR_TRUE;
    zr_vm_task_sync_condition_signal(&launch->condition);
    zr_vm_task_sync_mutex_unlock(&launch->mutex);
}

void zr_vm_thread_isolated_completion_processed(TZrPtr completionContext,
                                                TZrBool completed,
                                                TZrBool workerMustDisposeEnvelope) {
    ZrVmIsolatedDomainLaunch *launch = (ZrVmIsolatedDomainLaunch *)completionContext;
    ZrVmIsolatedDomainRuntime *runtime;

    if (launch == ZR_NULL) {
        return;
    }
    zr_vm_task_sync_mutex_lock(&launch->mutex);
    launch->completionSucceeded = completed;
    launch->completionWorkerMustDisposeEnvelope = workerMustDisposeEnvelope;
    launch->completionProcessed = ZR_TRUE;
    zr_vm_task_sync_condition_signal(&launch->condition);
    zr_vm_task_sync_mutex_unlock(&launch->mutex);

    runtime = launch->providerRuntime;
    if (runtime == ZR_NULL) {
        return;
    }
    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    if (launch->workerSlotActive && !launch->workerSlotReleased) {
        if (runtime->liveWorkerCount > 0u) {
            runtime->liveWorkerCount--;
        }
        launch->workerSlotReleased = ZR_TRUE;
    }
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);
}

static void zr_vm_thread_isolated_launch_free(ZrVmIsolatedDomainLaunch *launch) {
    if (launch == ZR_NULL) {
        return;
    }
    if (launch->requestEnvelope != ZR_NULL) {
        /* A live worker owns this envelope; terminal cleanup happens before free. */
        launch->requestEnvelope = ZR_NULL;
    }
    if (launch->artifact != ZR_NULL) {
        if (launch->artifact->binaryPath != ZR_NULL) {
            remove(launch->artifact->binaryPath);
        }
        zr_vm_task_worker_launch_free(launch->artifact);
    }
    free(launch->notificationMessage);
    free(launch->captureEnvelopes);
    zr_vm_task_sync_condition_destroy(&launch->condition);
    zr_vm_task_sync_mutex_destroy(&launch->mutex);
    free(launch);
}

static TZrBool zr_vm_thread_isolated_wait_for_completion(
        ZrVmIsolatedDomainLaunch *launch,
        TZrBool *outWorkerMustDisposeEnvelope) {
    TZrBool completed;

    if (outWorkerMustDisposeEnvelope != ZR_NULL) {
        *outWorkerMustDisposeEnvelope = ZR_FALSE;
    }
    if (launch == ZR_NULL) {
        return ZR_FALSE;
    }
    zr_vm_task_sync_mutex_lock(&launch->mutex);
    while (!launch->completionProcessed) {
        zr_vm_task_sync_condition_wait(&launch->condition, &launch->mutex, 10u);
    }
    completed = launch->completionSucceeded;
    if (outWorkerMustDisposeEnvelope != ZR_NULL) {
        *outWorkerMustDisposeEnvelope = launch->completionWorkerMustDisposeEnvelope;
    }
    zr_vm_task_sync_mutex_unlock(&launch->mutex);
    return completed;
}

static void zr_vm_thread_isolated_enqueue_fault(ZrVmIsolatedDomainLaunch *launch) {
    if (launch == ZR_NULL) {
        return;
    }
    if (zr_vm_task_scheduler_enqueue_isolated_fault(launch->ownerRuntime,
                                                     &launch->workItem,
                                                     launch,
                                                     launch->notificationMessage)) {
        launch->notificationMessage = ZR_NULL;
        zr_vm_thread_isolated_wait_for_completion(launch, ZR_NULL);
    }
}

static TZrBool zr_vm_thread_isolated_build_callable(
        SZrState *state,
        SZrFunction *function,
        ZrVmIsolatedDomainLaunch *launch,
        SZrTypeValue *outCallable) {
    SZrClosure *closure;
    TZrUInt32 captureIndex;
    TZrUInt64 workerId;

    if (state == ZR_NULL || function == ZR_NULL || launch == ZR_NULL || outCallable == ZR_NULL ||
        launch->artifact == ZR_NULL) {
        return ZR_FALSE;
    }
    workerId = launch->artifact->workerIsolateId;
    closure = ZrCore_Closure_New(state, launch->captureCount);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);
    for (captureIndex = 0u; captureIndex < launch->captureCount; captureIndex++) {
        SZrTypeValue capturedValue;
        TZrUInt64 claimEpoch = (TZrUInt64)captureIndex + 2u;
        TZrBool claimed = ZR_FALSE;

        ZrLib_Value_SetNull(&capturedValue);
        if (launch->captureEnvelopes == ZR_NULL || launch->captureEnvelopes[captureIndex] == ZR_NULL ||
            !(claimed = ZrCore_OwnershipTransfer_Claim(launch->captureEnvelopes[captureIndex],
                                                        state,
                                                        workerId,
                                                        claimEpoch)) ||
            !ZrCore_OwnershipTransfer_CommitCrossDomain(launch->captureEnvelopes[captureIndex],
                                                         state,
                                                         workerId,
                                                         claimEpoch,
                                                         &capturedValue,
                                                         ZR_NULL)) {
            if (claimed) {
                ZrCore_OwnershipTransfer_AbortCrossDomain(launch->captureEnvelopes[captureIndex],
                                                           state,
                                                           workerId,
                                                           claimEpoch,
                                                           ZR_NULL);
                ZrCore_OwnershipTransfer_Free(state, launch->captureEnvelopes[captureIndex]);
                launch->captureEnvelopes[captureIndex] = ZR_NULL;
            }
            return ZR_FALSE;
        }
        ZrCore_OwnershipTransfer_Free(state, launch->captureEnvelopes[captureIndex]);
        launch->captureEnvelopes[captureIndex] = ZR_NULL;
        ZrCore_Value_Copy(state,
                          &closure->closureValuesExtend[captureIndex]->link.closedValue,
                          &capturedValue);
    }
    ZrCore_Value_InitAsRawObject(state, outCallable, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    outCallable->type = ZR_VALUE_TYPE_CLOSURE;
    outCallable->isGarbageCollectable = ZR_TRUE;
    outCallable->isNative = ZR_FALSE;
    return ZR_TRUE;
}

static void zr_vm_thread_isolated_worker_run(ZrVmIsolatedDomainLaunch *launch) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *workerGlobal = ZR_NULL;
    SZrState *workerState = ZR_NULL;
    SZrLibrary_Project *project = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue requestValue;
    SZrTypeValue callableValue;
    SZrTypeValue resultValue;
    SZrDomainTransferContract contract;
    SZrOwnershipTransferEnvelope *resultEnvelope = ZR_NULL;
    SZrGcDomainIdentity workerDomain = {0};
    TZrUInt64 workerId;
    TZrBool requestClaimed = ZR_FALSE;
    TZrBool workerMustDisposeResultEnvelope = ZR_FALSE;

    if (launch == ZR_NULL || launch->artifact == ZR_NULL) {
        return;
    }
    workerId = launch->artifact->workerIsolateId;
    workerGlobal = ZrCore_GlobalState_New(launch->artifact->allocator,
                                          launch->artifact->userAllocationArguments,
                                          workerId,
                                          &callbacks);
    if (workerGlobal == ZR_NULL || workerGlobal->mainThreadState == ZR_NULL) {
        zr_vm_thread_isolated_launch_signal_ready(launch, ZR_TRUE, workerDomain);
        zr_vm_thread_isolated_wait_for_completion(launch, ZR_NULL);
        if (workerGlobal != ZR_NULL) {
            ZrCore_GlobalState_Free(workerGlobal);
        }
        return;
    }

    workerState = workerGlobal->mainThreadState;
    ZrParser_ToGlobalState_Register(workerState);
    if (!ZrCore_TaskRuntime_RegisterBuiltins(workerGlobal) || !ZrVmThread_Register(workerGlobal)) {
        zr_vm_thread_isolated_launch_signal_ready(launch, ZR_TRUE, workerDomain);
        zr_vm_thread_isolated_wait_for_completion(launch, ZR_NULL);
        ZrCore_GlobalState_Free(workerGlobal);
        return;
    }
    workerDomain = ZrCore_GcDomain_GetIdentity(workerState);
    zr_vm_thread_isolated_launch_signal_ready(launch, ZR_FALSE, workerDomain);

    zr_vm_task_sync_mutex_lock(&launch->mutex);
    while (!launch->requestReady && !launch->cancelled) {
        zr_vm_task_sync_condition_wait(&launch->condition, &launch->mutex, 10u);
    }
    zr_vm_task_sync_mutex_unlock(&launch->mutex);
    if (launch->cancelled || launch->requestEnvelope == ZR_NULL) {
        zr_vm_thread_isolated_enqueue_fault(launch);
        ZrLibrary_NativeRegistry_Free(workerGlobal);
        ZrCore_GlobalState_Free(workerGlobal);
        return;
    }

    ZrLib_Value_SetNull(&requestValue);
    if (!(requestClaimed = ZrCore_OwnershipTransfer_Claim(launch->requestEnvelope,
                                                           workerState,
                                                           workerId,
                                                           1u)) ||
        !ZrCore_OwnershipTransfer_CommitCrossDomain(launch->requestEnvelope,
                                                     workerState,
                                                     workerId,
                                                     1u,
                                                     &requestValue,
                                                     ZR_NULL)) {
        if (requestClaimed) {
            ZrCore_OwnershipTransfer_AbortCrossDomain(launch->requestEnvelope,
                                                       workerState,
                                                       workerId,
                                                       1u,
                                                       ZR_NULL);
            ZrCore_OwnershipTransfer_Free(workerState, launch->requestEnvelope);
            launch->requestEnvelope = ZR_NULL;
        }
        zr_vm_thread_isolated_enqueue_fault(launch);
        ZrLibrary_NativeRegistry_Free(workerGlobal);
        ZrCore_GlobalState_Free(workerGlobal);
        return;
    }
    ZrCore_OwnershipTransfer_Free(workerState, launch->requestEnvelope);
    launch->requestEnvelope = ZR_NULL;

    project = zr_vm_task_worker_clone_project(workerState, launch->artifact);
    if (project != ZR_NULL) {
        workerGlobal->userData = project;
        workerGlobal->sourceLoader = ZrLibrary_Project_SourceLoadImplementation;
    }
    if (!zr_vm_task_worker_load_function(workerState, launch->artifact->binaryPath, &function)) {
        zr_vm_thread_isolated_enqueue_fault(launch);
        goto cleanup;
    }

    ZrLib_Value_SetNull(&callableValue);
    if (!zr_vm_thread_isolated_build_callable(workerState, function, launch, &callableValue)) {
        zr_vm_thread_isolated_enqueue_fault(launch);
        goto cleanup;
    }

    ZrLib_Value_SetNull(&resultValue);
    if (!zr_vm_task_worker_execute_callable(workerState, &callableValue, &resultValue) ||
        workerState->threadStatus != ZR_THREAD_STATUS_FINE) {
        zr_vm_thread_isolated_enqueue_fault(launch);
        goto cleanup;
    }

    contract = zr_vm_thread_isolated_contract_for_value(&launch->transferQuota, &resultValue);
    resultEnvelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(workerState,
                                                                  launch->callerDomain,
                                                                  &contract,
                                                                  &resultValue,
                                                                  ZR_NULL);
    if (resultEnvelope == ZR_NULL || !ZrCore_OwnershipTransfer_Publish(resultEnvelope) ||
        !zr_vm_task_scheduler_enqueue_isolated_completion(launch->ownerRuntime,
                                                           &launch->workItem,
                                                           resultEnvelope,
                                                           workerDomain,
                                                           launch,
                                                           launch->notificationMessage)) {
        if (resultEnvelope != ZR_NULL) {
            ZrCore_OwnershipTransfer_Free(workerState, resultEnvelope);
        }
        zr_vm_thread_isolated_enqueue_fault(launch);
        goto cleanup;
    }
    launch->notificationMessage = ZR_NULL;
    zr_vm_thread_isolated_wait_for_completion(launch, &workerMustDisposeResultEnvelope);
    if (workerMustDisposeResultEnvelope) {
        ZrCore_OwnershipTransfer_Free(workerState, resultEnvelope);
    }

cleanup:
    if (project != ZR_NULL) {
        ZrLibrary_Project_Free(workerState, project);
        workerGlobal->userData = ZR_NULL;
    }
    ZrLibrary_NativeRegistry_Free(workerGlobal);
    ZrCore_GlobalState_Free(workerGlobal);
}

#if defined(ZR_PLATFORM_WIN)
static unsigned __stdcall zr_vm_thread_isolated_worker_entry(void *argument) {
    ZrVmIsolatedDomainLaunch *launch = (ZrVmIsolatedDomainLaunch *)argument;
    zr_vm_thread_isolated_worker_run(launch);
    zr_vm_thread_isolated_launch_free(launch);
    return 0;
}
#else
static void *zr_vm_thread_isolated_worker_entry(void *argument) {
    ZrVmIsolatedDomainLaunch *launch = (ZrVmIsolatedDomainLaunch *)argument;
    zr_vm_thread_isolated_worker_run(launch);
    zr_vm_thread_isolated_launch_free(launch);
    return ZR_NULL;
}
#endif

static TZrBool zr_vm_thread_isolated_start_worker(ZrVmIsolatedDomainLaunch *launch) {
    if (launch == ZR_NULL) {
        return ZR_FALSE;
    }
#if defined(ZR_PLATFORM_WIN)
    {
        uintptr_t threadHandle = _beginthreadex(ZR_NULL, 0, zr_vm_thread_isolated_worker_entry, launch, 0, ZR_NULL);
        if (threadHandle == 0) {
            return ZR_FALSE;
        }
        CloseHandle((HANDLE)threadHandle);
    }
#else
    {
        pthread_t thread;
        if (pthread_create(&thread, ZR_NULL, zr_vm_thread_isolated_worker_entry, launch) != 0) {
            return ZR_FALSE;
        }
        pthread_detach(thread);
    }
#endif
    return ZR_TRUE;
}

static void zr_vm_thread_isolated_cancel_before_request(ZrVmIsolatedDomainLaunch *launch) {
    if (launch == ZR_NULL) {
        return;
    }
    zr_vm_task_sync_mutex_lock(&launch->mutex);
    launch->cancelled = ZR_TRUE;
    zr_vm_task_sync_condition_signal(&launch->condition);
    zr_vm_task_sync_mutex_unlock(&launch->mutex);
}

/* Start only after this launch owns a bounded provider slot.  The caller
 * creates every request envelope after the worker publishes its domain. */
static TZrBool zr_vm_thread_isolated_launch_begin(
        SZrState *state,
        ZrVmIsolatedDomainLaunch *launch) {
    SZrClosure *closure = ZR_NULL;
    SZrFunction *function;
    SZrTypeValue callableValue;
    SZrTypeValue requestValue;
    SZrDomainTransferContract contract;
    TZrUInt32 captureIndex;

    if (state == ZR_NULL || launch == ZR_NULL || launch->artifact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_vm_thread_isolated_start_worker(launch)) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain could not start worker");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&launch->mutex);
    while (!launch->workerReady) {
        zr_vm_task_sync_condition_wait(&launch->condition, &launch->mutex, 10u);
    }
    zr_vm_task_sync_mutex_unlock(&launch->mutex);
    if (launch->startupFailed) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain worker initialization failed");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_completion_processed(launch, ZR_FALSE, ZR_FALSE);
        return ZR_TRUE;
    }

    ZrLib_Value_SetNull(&callableValue);
    if (!ZrLibrary_TaskRuntime_CopyPreparedCallable(state, &launch->workItem, &callableValue) ||
        callableValue.isNative ||
        (function = ZrCore_Closure_GetMetadataFunctionFromValue(state, &callableValue)) == ZR_NULL) {
        zr_vm_thread_isolated_cancel_before_request(launch);
        return ZR_TRUE;
    }
    if (callableValue.type == ZR_VALUE_TYPE_CLOSURE && !callableValue.isNative) {
        closure = ZR_CAST_VM_CLOSURE(state, callableValue.value.object);
    }
    if (function == ZR_NULL ||
        launch->captureCount != (closure != ZR_NULL ? (TZrUInt32)closure->closureValueCount : 0u)) {
        zr_vm_thread_isolated_cancel_before_request(launch);
        return ZR_TRUE;
    }

    if (launch->captureCount > 0u) {
        launch->captureEnvelopes = (SZrOwnershipTransferEnvelope **)calloc(
                launch->captureCount,
                sizeof(*launch->captureEnvelopes));
        if (launch->captureEnvelopes == ZR_NULL) {
            zr_vm_thread_isolated_cancel_before_request(launch);
            return ZR_TRUE;
        }
        for (captureIndex = 0u; captureIndex < launch->captureCount; captureIndex++) {
            const SZrTypeValue *captureValue = ZrCore_ClosureValue_GetValue(
                    closure->closureValuesExtend[captureIndex]);

            contract = zr_vm_thread_isolated_contract_for_value(&launch->transferQuota, captureValue);
            launch->captureEnvelopes[captureIndex] = ZrCore_OwnershipTransfer_PrepareCrossDomain(
                    state,
                    launch->workerDomain,
                    &contract,
                    (SZrTypeValue *)captureValue,
                    ZR_NULL);
            if (launch->captureEnvelopes[captureIndex] == ZR_NULL ||
                !ZrCore_OwnershipTransfer_Publish(launch->captureEnvelopes[captureIndex])) {
                zr_vm_thread_isolated_release_capture_envelopes(state, launch);
                zr_vm_thread_isolated_cancel_before_request(launch);
                return ZR_TRUE;
            }
        }
    }

    ZrCore_Value_InitAsUInt(state, &requestValue, launch->artifact->workerIsolateId);
    contract = zr_vm_thread_isolated_value_copy_contract();
    launch->requestEnvelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(state,
                                                                           launch->workerDomain,
                                                                           &contract,
                                                                           &requestValue,
                                                                           ZR_NULL);
    if (launch->requestEnvelope == ZR_NULL ||
        !ZrCore_OwnershipTransfer_Publish(launch->requestEnvelope)) {
        if (launch->requestEnvelope != ZR_NULL) {
            ZrCore_OwnershipTransfer_Free(state, launch->requestEnvelope);
            launch->requestEnvelope = ZR_NULL;
        }
        zr_vm_thread_isolated_release_capture_envelopes(state, launch);
        zr_vm_thread_isolated_cancel_before_request(launch);
        return ZR_TRUE;
    }
    zr_vm_task_sync_mutex_lock(&launch->mutex);
    launch->requestReady = ZR_TRUE;
    zr_vm_task_sync_condition_signal(&launch->condition);
    zr_vm_task_sync_mutex_unlock(&launch->mutex);
    return ZR_TRUE;
}

static TZrBool zr_vm_thread_isolated_scheduler_is_shutdown(SZrState *state) {
    SZrObject *rootObject;

    if (state == ZR_NULL || (rootObject = zr_vm_task_root_object(state)) == ZR_NULL) {
        return ZR_TRUE;
    }
    return zr_vm_task_get_bool_field(state,
                                     rootObject,
                                     kThreadSchedulerIsolatedShutdownField,
                                     ZR_FALSE);
}

static void zr_vm_thread_isolated_fault_queued_launches(
        SZrState *state,
        ZrVmIsolatedDomainRuntime *runtime) {
    ZrVmIsolatedDomainLaunch *launch;

    if (state == ZR_NULL || runtime == ZR_NULL) {
        return;
    }
    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    launch = runtime->pendingHead;
    runtime->pendingHead = ZR_NULL;
    runtime->pendingTail = ZR_NULL;
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);
    while (launch != ZR_NULL) {
        ZrVmIsolatedDomainLaunch *next = launch->next;
        launch->next = ZR_NULL;
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain scheduler is shut down");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_launch_free(launch);
        launch = next;
    }
}

void zr_vm_thread_isolated_scheduler_shutdown_all(SZrState *state) {
    const SZrTypeValue *runtimeValue;
    SZrObject *rootObject;
    ZrVmIsolatedDomainRuntime *runtime;

    if (state == ZR_NULL || (rootObject = zr_vm_task_root_object(state)) == ZR_NULL) {
        return;
    }
    runtimeValue = zr_vm_task_get_field_value(
            state, rootObject, kThreadSchedulerIsolatedRuntimeHeadField);
    if (runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return;
    }
    runtime = (ZrVmIsolatedDomainRuntime *)runtimeValue->value.nativeObject.nativePointer;
    while (runtime != ZR_NULL) {
        ZrVmIsolatedDomainRuntime *next = runtime->nextRegistered;
        zr_vm_thread_isolated_fault_queued_launches(state, runtime);
        runtime = next;
    }
}

void zr_vm_thread_isolated_scheduler_dispatch_pending(SZrState *state, SZrObject *scheduler) {
    ZrVmIsolatedDomainRuntime *runtime;

    if (state == ZR_NULL || scheduler == ZR_NULL ||
        (runtime = zr_vm_thread_isolated_scheduler_runtime(state, scheduler)) == ZR_NULL) {
        return;
    }
    if (zr_vm_thread_isolated_scheduler_is_shutdown(state)) {
        zr_vm_thread_isolated_fault_queued_launches(state, runtime);
        return;
    }

    for (;;) {
        ZrVmIsolatedDomainLaunch *launch;
        TZrBool started;

        zr_vm_task_sync_mutex_lock(&runtime->mutex);
        if (runtime->pendingHead == ZR_NULL || runtime->liveWorkerCount >= runtime->workerLimit) {
            zr_vm_task_sync_mutex_unlock(&runtime->mutex);
            break;
        }
        launch = runtime->pendingHead;
        runtime->pendingHead = launch->next;
        if (runtime->pendingHead == ZR_NULL) {
            runtime->pendingTail = ZR_NULL;
        }
        launch->next = ZR_NULL;
        runtime->liveWorkerCount++;
        launch->workerSlotActive = ZR_TRUE;
        zr_vm_task_sync_mutex_unlock(&runtime->mutex);

        started = zr_vm_thread_isolated_launch_begin(state, launch);
        if (!started) {
            zr_vm_thread_isolated_completion_processed(launch, ZR_FALSE, ZR_FALSE);
            zr_vm_thread_isolated_launch_free(launch);
        }
    }
}

static TZrBool zr_vm_thread_isolated_await(SZrState *state, SZrObject *task, TZrPtr context) {
    ZrVmIsolatedDomainRuntime *runtime = (ZrVmIsolatedDomainRuntime *)context;

    if (state == ZR_NULL || task == ZR_NULL || runtime == ZR_NULL || runtime->scheduler == ZR_NULL) {
        return ZR_FALSE;
    }
    while (!ZrLibrary_TaskRuntime_IsTaskComplete(state, task)) {
        zr_vm_thread_isolated_scheduler_dispatch_pending(state, runtime->scheduler);
        if (!zr_vm_task_scheduler_process_external(state, runtime->scheduler)) {
            zr_vm_task_scheduler_wait_for_external(state, runtime->scheduler, 10u);
        }
    }
    return ZR_TRUE;
}

TZrBool zr_vm_thread_isolated_scheduler_init(SZrState *state,
                                              SZrObject *scheduler,
                                              TZrUInt32 workerCount,
                                              const SZrDomainTransferQuota *transferQuota) {
    ZrVmIsolatedDomainRuntime *runtime;
    SZrTypeValue runtimeValue;
    SZrObject *rootObject;
    const SZrTypeValue *previousRuntimeValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || workerCount == 0u || transferQuota == ZR_NULL ||
        transferQuota->maxObjects == 0u || transferQuota->maxBytes == 0u || transferQuota->maxDepth == 0u ||
        zr_vm_task_scheduler_get_runtime(state, scheduler) == ZR_NULL) {
        return ZR_FALSE;
    }
    runtime = (ZrVmIsolatedDomainRuntime *)calloc(1, sizeof(*runtime));
    if (runtime == ZR_NULL) {
        return ZR_FALSE;
    }
    zr_vm_task_sync_mutex_init(&runtime->mutex);
    runtime->scheduler = scheduler;
    runtime->workerLimit = workerCount;
    runtime->transferQuota = *transferQuota;
    runtime->awaitRegistration.awaitHook = zr_vm_thread_isolated_await;
    runtime->awaitRegistration.context = runtime;
    ZrCore_Value_InitAsNativePointer(state, &runtimeValue, runtime);
    zr_vm_task_set_value_field(state, scheduler, kThreadSchedulerIsolatedRuntimeField, &runtimeValue);
    if (!ZrLibrary_TaskRuntime_RegisterAwaitHook(state, scheduler, &runtime->awaitRegistration)) {
        ZrLib_Value_SetNull(&runtimeValue);
        zr_vm_task_set_value_field(
                state, scheduler, kThreadSchedulerIsolatedRuntimeField, &runtimeValue);
        zr_vm_task_sync_mutex_destroy(&runtime->mutex);
        free(runtime);
        return ZR_FALSE;
    }
    rootObject = zr_vm_task_root_object(state);
    previousRuntimeValue = rootObject != ZR_NULL
            ? zr_vm_task_get_field_value(state,
                                         rootObject,
                                         kThreadSchedulerIsolatedRuntimeHeadField)
            : ZR_NULL;
    if (previousRuntimeValue != ZR_NULL &&
        previousRuntimeValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        runtime->nextRegistered = (ZrVmIsolatedDomainRuntime *)
                previousRuntimeValue->value.nativeObject.nativePointer;
    }
    if (rootObject != ZR_NULL) {
        ZrCore_Value_InitAsNativePointer(state, &runtimeValue, runtime);
        zr_vm_task_set_value_field(state,
                                   rootObject,
                                   kThreadSchedulerIsolatedRuntimeHeadField,
                                   &runtimeValue);
    }
    return ZR_TRUE;
}

TZrBool zr_vm_thread_isolated_scheduler_schedule(SZrState *state,
                                                  SZrObject *scheduler,
                                                  SZrObject *job,
                                                  SZrTypeValue *result) {
    ZrVmIsolatedDomainRuntime *runtime;
    ZrVmIsolatedDomainLaunch *launch;
    ZrVmTaskWorkerLaunch *artifact;
    SZrFunction *function;
    SZrClosure *closure = ZR_NULL;
    SZrTypeValue callableValue;
    SZrLibrary_Project *project;
    TZrChar tempPath[512];

    if (state == ZR_NULL || scheduler == ZR_NULL || job == ZR_NULL || result == ZR_NULL ||
        (runtime = zr_vm_thread_isolated_scheduler_runtime(state, scheduler)) == ZR_NULL) {
        return ZR_FALSE;
    }
    launch = (ZrVmIsolatedDomainLaunch *)calloc(1, sizeof(*launch));
    if (launch == ZR_NULL) {
        return ZR_FALSE;
    }
    zr_vm_task_sync_mutex_init(&launch->mutex);
    zr_vm_task_sync_condition_init(&launch->condition);
    launch->ownerRuntime = zr_vm_task_scheduler_get_runtime(state, scheduler);
    launch->callerDomain = ZrCore_GcDomain_GetIdentity(state);
    launch->transferQuota = runtime->transferQuota;
    if (launch->ownerRuntime == ZR_NULL ||
        !ZrLibrary_TaskRuntime_PrepareJob(state, scheduler, job, result, &launch->workItem)) {
        zr_vm_thread_isolated_launch_free(launch);
        return ZR_FALSE;
    }
    if (zr_vm_task_get_bool_field(state,
                                  zr_vm_task_root_object(state),
                                  kThreadSchedulerIsolatedShutdownField,
                                  ZR_FALSE)) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain scheduler is shut down");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_launch_free(launch);
        return ZR_TRUE;
    }

    ZrLib_Value_SetNull(&callableValue);
    if (!ZrLibrary_TaskRuntime_CopyPreparedCallable(state, &launch->workItem, &callableValue) ||
        callableValue.isNative ||
        (function = ZrCore_Closure_GetMetadataFunctionFromValue(state, &callableValue)) == ZR_NULL) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain only supports script Job callables");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_launch_free(launch);
        return ZR_TRUE;
    }
    if (callableValue.type == ZR_VALUE_TYPE_CLOSURE && !callableValue.isNative) {
        closure = ZR_CAST_VM_CLOSURE(state, callableValue.value.object);
    }
    launch->captureCount = closure != ZR_NULL ? (TZrUInt32)closure->closureValueCount : 0u;
    if (!zr_vm_task_worker_make_temp_path(tempPath, sizeof(tempPath)) ||
        !ZrParser_Writer_WriteBinaryFile(state, function, tempPath)) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain could not serialize Job callable");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_launch_free(launch);
        return ZR_TRUE;
    }

    artifact = (ZrVmTaskWorkerLaunch *)calloc(1, sizeof(*artifact));
    if (artifact == ZR_NULL || (artifact->binaryPath = zr_vm_task_worker_strdup(tempPath)) == ZR_NULL) {
        free(artifact);
        remove(tempPath);
        ZrLibrary_TaskRuntime_FaultPreparedJob(state, &launch->workItem, "IsolatedDomain could not allocate request");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_launch_free(launch);
        return ZR_TRUE;
    }
    launch->artifact = artifact;
    artifact->allocator = state->global->allocator;
    artifact->userAllocationArguments = state->global->userAllocationArguments;
    artifact->workerIsolateId = zr_vm_task_next_worker_isolate_id();
    artifact->supportMultithread = zr_vm_task_default_support_multithread(state);
    project = (SZrLibrary_Project *)state->global->userData;
    if (project != ZR_NULL) {
        artifact->projectFile = project->file != ZR_NULL ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->file)) : ZR_NULL;
        artifact->projectDirectory = project->directory != ZR_NULL ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->directory)) : ZR_NULL;
        artifact->projectSource = project->source != ZR_NULL ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->source)) : ZR_NULL;
        artifact->projectBinary = project->binary != ZR_NULL ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->binary)) : ZR_NULL;
        artifact->projectEntry = project->entry != ZR_NULL ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->entry)) : ZR_NULL;
    }

    launch->notificationMessage = (ZrVmTaskSchedulerMessage *)calloc(
            1,
            sizeof(*launch->notificationMessage));
    if (launch->notificationMessage == ZR_NULL) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state,
                                               &launch->workItem,
                                               "IsolatedDomain could not reserve completion delivery");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &launch->workItem);
        zr_vm_thread_isolated_launch_free(launch);
        return ZR_TRUE;
    }

    launch->providerRuntime = runtime;
    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    if (runtime->pendingTail != ZR_NULL) {
        runtime->pendingTail->next = launch;
    } else {
        runtime->pendingHead = launch;
    }
    runtime->pendingTail = launch;
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);
    zr_vm_thread_isolated_scheduler_dispatch_pending(state, scheduler);
    return ZR_TRUE;
}
