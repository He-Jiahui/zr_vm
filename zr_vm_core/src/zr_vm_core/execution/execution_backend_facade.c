#include "execution_backend_internal.h"

/* 默认服务指针仅借用宿主存储；默认锁保护指针本身，宿主另行保证替换及关闭期间的寿命。 */
static volatile TZrUInt32 zr_execution_backend_default_lock;
static SZrExecutionBackendService *zr_execution_backend_default_service;

/* 保护默认指针的短暂读写；不得在此锁内调用 service API 或宿主回调。 */
static void zr_execution_backend_default_lock_enter(void) {
#if defined(_MSC_VER)
    while (_InterlockedExchange((volatile long *)&zr_execution_backend_default_lock,
                                1L) != 0L) {
    }
#else
    while (__sync_lock_test_and_set(&zr_execution_backend_default_lock, 1u) != 0u) {
    }
#endif
}

/* 结束默认指针访问；解锁不提供指针所指服务的寿命保证。 */
static void zr_execution_backend_default_lock_leave(void) {
#if defined(_MSC_VER)
    _InterlockedExchange((volatile long *)&zr_execution_backend_default_lock, 0L);
#else
    __sync_lock_release(&zr_execution_backend_default_lock);
#endif
}

TZrBool ZrCore_ExecutionBackend_SetDefaultService(
        SZrExecutionBackendService *service) {
    if (service != ZR_NULL && !zr_execution_backend_service_shape_valid(service)) {
        return ZR_FALSE;
    }
    zr_execution_backend_default_lock_enter();
    zr_execution_backend_default_service = service;
    zr_execution_backend_default_lock_leave();
    return ZR_TRUE;
}

/* 在默认锁下借用当前指针，再由短接口派发；宿主必须另行协调释放与并发替换。 */
static SZrExecutionBackendService *zr_execution_backend_get_default(void) {
    SZrExecutionBackendService *service;
    zr_execution_backend_default_lock_enter();
    service = zr_execution_backend_default_service;
    zr_execution_backend_default_lock_leave();
    return service;
}

TZrBool ZrCore_ExecutionBackend_Register(
        const SZrExecutionBackendDescriptor *backend) {
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDiagnostic diagnostic;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    status = ZrCore_ExecutionBackendService_Register(
            service, backend, &registration, &diagnostic);
    return (TZrBool)(status == ZR_EXECUTION_BACKEND_STATUS_OK);
}

TZrBool ZrCore_ExecutionBackend_CompileAsync(
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket) {
    SZrExecutionBackendDiagnostic diagnostic;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    status = ZrCore_ExecutionBackendService_CompileAsync(
            service, request, ticket, &diagnostic);
    return (TZrBool)(status == ZR_EXECUTION_BACKEND_STATUS_PENDING ||
                     status == ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC ||
                     status == ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT);
}

TZrBool ZrCore_ExecutionBackend_InvalidateGeneration(
        const SZrExecutionGenerationKey *key) {
    SZrExecutionBackendDiagnostic diagnostic;
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    return (TZrBool)(ZrCore_ExecutionBackendService_InvalidateGeneration(
                             service, key, &diagnostic) ==
                     ZR_EXECUTION_BACKEND_STATUS_OK);
}

TZrBool ZrCore_ExecutionBackend_ResumeInterpreter(
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    return ZrCore_ExecutionBackendService_ResumeInterpreter(
            service, request, diagnostic);
}
