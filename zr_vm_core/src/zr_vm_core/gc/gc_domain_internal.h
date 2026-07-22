#ifndef ZR_VM_CORE_GC_DOMAIN_INTERNAL_H
#define ZR_VM_CORE_GC_DOMAIN_INTERNAL_H

#include "zr_vm_core/gc_domain.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

struct SZrGarbageCollector;
struct SZrGlobalState;
struct SZrRawObject;
struct SZrState;

typedef enum EZrGcDomainRootKind {
    ZR_GC_DOMAIN_ROOT_KIND_FREE = 0,
    ZR_GC_DOMAIN_ROOT_KIND_HANDLE = 1,
    ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP = 2
} EZrGcDomainRootKind;

typedef struct SZrGcDomainRootSlot {
    struct SZrRawObject *target;
    TZrUInt32 generation;
    TZrUInt32 retainCount;
    EZrGcDomainRootKind kind;
} SZrGcDomainRootSlot;

typedef enum EZrGcDomainMutatorStatus {
    ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE = 0,
    ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING,
    ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED,
    ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED,
    ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL
} EZrGcDomainMutatorStatus;

typedef struct SZrGcDomainMutatorRecord {
    struct SZrState *state;
    TZrUInt64 mutatorId;
    TZrUInt64 observedEpoch;
    TZrUInt32 executionDepth;
    TZrUInt32 nativeDepth;
    EZrGcNativeSafepointMode nativeMode;
    EZrGcDomainMutatorStatus status;
    TZrBool nativeEnteredFromInactive;
} SZrGcDomainMutatorRecord;

typedef enum EZrGcDomainTransferTelemetryEvent {
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PREPARE = 0,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PUBLISH,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_ABORT,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_CLAIM,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_COMMIT,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_ABORT
} EZrGcDomainTransferTelemetryEvent;

typedef struct SZrGcDomain {
    struct SZrGlobalState *global;
    struct SZrGarbageCollector *collector;
    struct SZrState *attachedState;
    SZrGcDomainIdentity identity;
    SZrGcDomainRootSlot *roots;
    TZrSize rootLength;
    TZrSize rootCapacity;
    TZrSize activeRootCount;
    TZrSize ownershipRootCount;
    SZrGcDomainMutatorRecord *mutators;
    TZrSize mutatorLength;
    TZrSize mutatorCapacity;
    TZrUInt64 nextMutatorId;
    TZrUInt64 safepointEpoch;
    struct SZrState *collectorState;
    TZrUInt32 pauseDepth;
    TZrBool pauseRequested;
    TZrUInt64 safepointWaitCount;
    TZrUInt64 safepointWaitTotalUs;
    TZrUInt64 safepointWaitMaxUs;
    TZrUInt64 outboundTransferPrepareCount;
    TZrUInt64 outboundTransferPublishCount;
    TZrUInt64 outboundTransferAbortCount;
    TZrUInt64 outboundTransferObjectCount;
    TZrUInt64 outboundTransferByteCount;
    TZrUInt64 inboundTransferClaimCount;
    TZrUInt64 inboundTransferCommitCount;
    TZrUInt64 inboundTransferAbortCount;
    TZrUInt64 inboundTransferObjectCount;
    TZrUInt64 inboundTransferByteCount;
#if defined(ZR_PLATFORM_WIN)
    CRITICAL_SECTION coordinationLock;
    CRITICAL_SECTION mutationLock;
    CONDITION_VARIABLE coordinationCondition;
#else
    pthread_mutex_t coordinationLock;
    pthread_mutex_t mutationLock;
    pthread_cond_t coordinationCondition;
#endif
    TZrBool coordinationInitialized;
    TZrBool mutationLockInitialized;
    TZrBool active;
} SZrGcDomain;

SZrGcDomain *ZrCore_GcDomain_New(
        struct SZrGlobalState *global,
        struct SZrGarbageCollector *collector);
void ZrCore_GcDomain_Free(SZrGcDomain *domain);
void ZrCore_GcDomain_AttachState(SZrGcDomain *domain, struct SZrState *state);
void ZrCore_GcDomain_DetachState(SZrGcDomain *domain, struct SZrState *state);
TZrBool ZrCore_GcDomain_CoordinationInit(SZrGcDomain *domain);
void ZrCore_GcDomain_CoordinationFree(SZrGcDomain *domain);
void ZrCore_GcDomain_Lock(SZrGcDomain *domain);
void ZrCore_GcDomain_Unlock(SZrGcDomain *domain);
void ZrCore_GcDomain_Broadcast(SZrGcDomain *domain);
void ZrCore_GcDomain_MutationLock(SZrGcDomain *domain);
void ZrCore_GcDomain_MutationUnlock(SZrGcDomain *domain);
void ZrCore_GcDomain_RecordTransferTelemetry(
        struct SZrGlobalState *global,
        SZrGcDomainIdentity identity,
        EZrGcDomainTransferTelemetryEvent event,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount);
TZrBool ZrCore_GcDomain_MutationBegin(struct SZrState *state);
void ZrCore_GcDomain_MutationEnd(struct SZrState *state, TZrBool locked);
TZrBool ZrCore_GcDomain_RegisterMutator(
        SZrGcDomain *domain,
        struct SZrState *state);
void ZrCore_GcDomain_UnregisterMutator(
        SZrGcDomain *domain,
        struct SZrState *state);
TZrBool ZrCore_GcDomain_AssignObject(SZrGcDomain *domain, struct SZrRawObject *object);
TZrBool ZrCore_GcDomain_RegisterOwnershipRoot(
        struct SZrState *state,
        struct SZrRawObject *object);
void ZrCore_GcDomain_UnregisterOwnershipRoot(
        struct SZrState *state,
        struct SZrRawObject *object);

#endif
