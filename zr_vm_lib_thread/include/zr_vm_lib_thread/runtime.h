//
// zr.thread runtime descriptor.
//

#ifndef ZR_VM_THREAD_RUNTIME_H
#define ZR_VM_THREAD_RUNTIME_H

#include "zr_vm_lib_thread/conf.h"
#include "zr_vm_core/gc_domain.h"

struct SZrGlobalState;

/*
 * ThreadScheduler policy is selected by the embedding host before the
 * scheduler is constructed. It intentionally has no ZR source-level
 * equivalent: each scheduler snapshots the configured provider at creation.
 */
typedef enum EZrVmThreadSchedulerExecutionPolicy {
    ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ATTACHED_DOMAIN = 0,
    ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN = 1
} EZrVmThreadSchedulerExecutionPolicy;

ZR_VM_THREAD_API const ZrLibModuleDescriptor *ZrVmThread_Runtime_GetModuleDescriptor(void);

ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
        struct SZrGlobalState *global,
        EZrVmThreadSchedulerExecutionPolicy policy);
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_SetIsolatedTransferQuota(
        struct SZrGlobalState *global,
        TZrUInt32 maxObjects,
        TZrUInt64 maxBytes,
        TZrUInt32 maxDepth);
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_ShutdownIsolatedSchedulers(
        struct SZrGlobalState *global);
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_GetLastSchedulerWorkerDomain(
        const struct SZrGlobalState *global,
        SZrGcDomainIdentity *outDomain);

#endif // ZR_VM_THREAD_RUNTIME_H
