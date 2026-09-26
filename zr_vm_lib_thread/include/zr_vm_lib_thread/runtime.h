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

/** @brief 配置之后新建的 ThreadScheduler；已构造实例保留当时策略，调用方应在同一全局状态的宿主线程设置。 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
        struct SZrGlobalState *global,
        EZrVmThreadSchedulerExecutionPolicy policy);
/** @brief 设置之后新建的隔离调度器的跨域复制预算；三个上限都必须非零。 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_SetIsolatedTransferQuota(
        struct SZrGlobalState *global,
        TZrUInt32 maxObjects,
        TZrUInt64 maxBytes,
        TZrUInt32 maxDepth);
/** @brief 阻止隔离调度器继续接单，故障化排队任务并等待在途 worker 确认；销毁 global 前调用。 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_ShutdownIsolatedSchedulers(
        struct SZrGlobalState *global);
/** @brief 读取最近完成的隔离 worker 的域身份；没有有效记录时返回假并清零输出。 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_GetLastSchedulerWorkerDomain(
        const struct SZrGlobalState *global,
        SZrGcDomainIdentity *outDomain);

#endif // ZR_VM_THREAD_RUNTIME_H
