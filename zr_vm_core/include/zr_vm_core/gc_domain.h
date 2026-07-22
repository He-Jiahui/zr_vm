#ifndef ZR_VM_CORE_GC_DOMAIN_H
#define ZR_VM_CORE_GC_DOMAIN_H

#include "zr_vm_core/conf.h"

struct SZrRawObject;
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
