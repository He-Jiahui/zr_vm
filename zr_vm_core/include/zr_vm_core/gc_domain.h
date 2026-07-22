#ifndef ZR_VM_CORE_GC_DOMAIN_H
#define ZR_VM_CORE_GC_DOMAIN_H

#include "zr_vm_core/conf.h"

struct SZrRawObject;
struct SZrCallInfo;
struct SZrState;
struct SZrTypeValue;

typedef struct SZrGcDomainIdentity {
    TZrUInt64 id;
    TZrUInt32 generation;
} SZrGcDomainIdentity;

typedef struct SZrGcRootHandle {
    SZrGcDomainIdentity domain;
    TZrUInt32 slotIndex;
    TZrUInt32 slotGeneration;
} SZrGcRootHandle;

typedef enum EZrGcNativeSafepointMode {
    ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE = 0,
    ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED = 1,
    ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL = 2
} EZrGcNativeSafepointMode;

typedef struct SZrGcDomainPauseDiagnostic {
    TZrBool timedOut;
    TZrUInt64 safepointEpoch;
    TZrUInt64 blockingMutatorId;
    EZrGcNativeSafepointMode blockingNativeMode;
    const struct SZrState *blockingState;
    const struct SZrCallInfo *blockingNativeFrame;
} SZrGcDomainPauseDiagnostic;

typedef struct SZrGcDomainMutatorSnapshot {
    TZrUInt64 safepointEpoch;
    TZrUInt32 registeredMutatorCount;
    TZrUInt32 runningMutatorCount;
    TZrUInt32 parkedMutatorCount;
    TZrUInt32 blockingDetachedMutatorCount;
    TZrUInt32 noSafepointCriticalMutatorCount;
    TZrBool pauseRequested;
} SZrGcDomainMutatorSnapshot;

ZR_CORE_API SZrGcDomainIdentity ZrCore_GcDomain_GetIdentity(
        const struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_GcDomain_IdentityEquals(
        SZrGcDomainIdentity left,
        SZrGcDomainIdentity right);
ZR_CORE_API TZrBool ZrCore_GcDomain_IdentityIsCurrent(
        const struct SZrState *state,
        SZrGcDomainIdentity identity);
ZR_CORE_API TZrBool ZrCore_GcDomain_GetObjectIdentity(
        const struct SZrRawObject *object,
        SZrGcDomainIdentity *outIdentity);
ZR_CORE_API TZrBool ZrCore_GcDomain_ObjectBelongsToState(
        const struct SZrState *state,
        const struct SZrRawObject *object);
ZR_CORE_API TZrBool ZrCore_GcDomain_ValidateWrite(
        const struct SZrState *state,
        const struct SZrRawObject *owner,
        const struct SZrRawObject *target);
ZR_CORE_API TZrBool ZrCore_GcDomain_ValidateValueWrite(
        const struct SZrState *state,
        const struct SZrRawObject *owner,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_GcDomain_MutatorEnter(struct SZrState *state);
ZR_CORE_API void ZrCore_GcDomain_MutatorLeave(struct SZrState *state);
ZR_CORE_API void ZrCore_GcDomain_MutatorUnwindScopes(
        struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_GcDomain_MutatorPoll(struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_GcDomain_MutatorAttach(
        struct SZrState *ownerState,
        struct SZrState *mutatorState);
ZR_CORE_API void ZrCore_GcDomain_MutatorDetach(
        struct SZrState *mutatorState);
ZR_CORE_API TZrUInt64 ZrCore_GcDomain_GetMutatorId(
        const struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_GcDomain_NativeEnter(
        struct SZrState *state,
        EZrGcNativeSafepointMode mode);
ZR_CORE_API void ZrCore_GcDomain_NativeLeave(struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_GcDomain_StopTheWorldBegin(
        struct SZrState *state,
        TZrUInt32 timeoutMilliseconds,
        SZrGcDomainPauseDiagnostic *outDiagnostic);
ZR_CORE_API void ZrCore_GcDomain_StopTheWorldEnd(struct SZrState *state);
ZR_CORE_API void ZrCore_GcDomain_GetMutatorSnapshot(
        const struct SZrState *state,
        SZrGcDomainMutatorSnapshot *outSnapshot);
ZR_CORE_API void ZrCore_GcDomain_WakeMutators(struct SZrState *state);

ZR_CORE_API TZrBool ZrCore_GcRootHandle_Create(
        struct SZrState *state,
        struct SZrRawObject *target,
        SZrGcRootHandle *outHandle);
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Clone(
        struct SZrState *state,
        const SZrGcRootHandle *source,
        SZrGcRootHandle *outHandle);
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Update(
        struct SZrState *state,
        SZrGcRootHandle *handle,
        struct SZrRawObject *target);
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Resolve(
        const struct SZrState *state,
        const SZrGcRootHandle *handle,
        struct SZrRawObject **outTarget);
ZR_CORE_API void ZrCore_GcRootHandle_Release(
        struct SZrState *state,
        SZrGcRootHandle *handle);

ZR_CORE_API TZrSize ZrCore_GcDomain_GetRootCount(
        const struct SZrState *state);
ZR_CORE_API TZrSize ZrCore_GcDomain_GetOwnershipRootCount(
        const struct SZrState *state);
ZR_CORE_API TZrBool ZrCore_GcDomain_IsOwnershipRoot(
        const struct SZrState *state,
        const struct SZrRawObject *object);

#endif
