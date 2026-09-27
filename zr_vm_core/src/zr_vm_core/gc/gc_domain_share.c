#include "zr_vm_core/gc_domain.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"

/* 失败诊断包含双方域身份和目标，便于宿主区分跨域与证明不足。 */
static void gc_domain_share_diagnostic_init(
        SZrGcDomainShareDiagnostic *diagnostic,
        const SZrGcDomainShareRequest *request) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_NONE;
    if (request != ZR_NULL) {
        diagnostic->target = request->target;
        diagnostic->producerDomain = ZrCore_GcDomain_GetIdentity(request->producerState);
        diagnostic->consumerDomain = ZrCore_GcDomain_GetIdentity(request->consumerState);
    }
}

/* 同域共享通过 consumer 的根句柄维持目标存活；释放责任转交句柄持有者。 */
TZrBool ZrCore_GcDomain_ShareValue(
        const SZrGcDomainShareRequest *request,
        SZrGcDomainShareDiagnostic *diagnostic,
        SZrGcRootHandle *outHandle) {
    SZrGcDomainIdentity domain;

    gc_domain_share_diagnostic_init(diagnostic, request);
    /* TODO: 入口会覆盖既有 outHandle；若宿主复用仍持有根的句柄槽，
     * 无论本次成败，旧根仍在域表中但句柄丢失。需定义输出槽须为空。 */
    if (outHandle != ZR_NULL) {
        outHandle->domain.id = 0u;
        outHandle->domain.generation = 0u;
        outHandle->slotIndex = ~(TZrUInt32)0u;
        outHandle->slotGeneration = 0u;
    }
    if (request == ZR_NULL || request->producerState == ZR_NULL ||
        request->consumerState == ZR_NULL || request->target == ZR_NULL ||
        request->consumerState->gcDomain == ZR_NULL || outHandle == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    /* 仅共享当前域对象；跨域所有权移交走转移契约，不注册裸指针根。 */
    if (request->producerState->gcDomain != request->consumerState->gcDomain ||
        !ZrCore_GcDomain_ObjectBelongsToState(
                request->producerState, request->target)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_FOREIGN_DOMAIN;
        }
        return ZR_FALSE;
    }
    /* TODO: sendProof/syncProof 目前只是宿主传入的布尔断言，仓内未见
     * 此 API 调用或证明生成者；需确认可信来源及 Send 路径的实际交接语义。 */
    if (request->sharedReference) {
        if (!request->syncProof) {
            if (diagnostic != ZR_NULL) {
                diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_SYNC_PROOF_REQUIRED;
            }
            return ZR_FALSE;
        }
    } else if (!request->sendProof) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_SEND_PROOF_REQUIRED;
        }
        return ZR_FALSE;
    }
    /* 生命周期已进入析构阶段的对象不得获得新的共享根。 */
    if (request->target->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_DROPPING ||
        request->target->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_DROPPED) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_LIFETIME;
        }
        return ZR_FALSE;
    }
    domain = ZrCore_GcDomain_GetIdentity(request->producerState);
    /* 根注册失败由创建器保留空句柄；调用方不接管目标或回收器内存。 */
    if (!ZrCore_GcRootHandle_Create(request->consumerState, request->target, outHandle)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_ROOT_REGISTRATION;
        }
        return ZR_FALSE;
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->producerDomain = domain;
    }
    return ZR_TRUE;
}
