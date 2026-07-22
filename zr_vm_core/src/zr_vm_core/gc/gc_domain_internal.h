#ifndef ZR_VM_CORE_GC_DOMAIN_INTERNAL_H
#define ZR_VM_CORE_GC_DOMAIN_INTERNAL_H

#include "zr_vm_core/gc_domain.h"

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
    TZrBool active;
} SZrGcDomain;

SZrGcDomain *ZrCore_GcDomain_New(
        struct SZrGlobalState *global,
        struct SZrGarbageCollector *collector);
void ZrCore_GcDomain_Free(SZrGcDomain *domain);
void ZrCore_GcDomain_AttachState(SZrGcDomain *domain, struct SZrState *state);
void ZrCore_GcDomain_DetachState(SZrGcDomain *domain, struct SZrState *state);
TZrBool ZrCore_GcDomain_AssignObject(SZrGcDomain *domain, struct SZrRawObject *object);
TZrBool ZrCore_GcDomain_RegisterOwnershipRoot(
        struct SZrState *state,
        struct SZrRawObject *object);
void ZrCore_GcDomain_UnregisterOwnershipRoot(
        struct SZrState *state,
        struct SZrRawObject *object);

#endif
