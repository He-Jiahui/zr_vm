/*
 * Focused 10.01 backend-service contract test.
 *
 * This fixture deliberately uses a C-only mock backend.  It proves that a
 * compile request is queued rather than executed by the frame-thread API,
 * that completion/publishing are distinct states, and that runtime-only code
 * remains leased until all map registrations have been removed and the
 * backend retires it.
 */
#include "zr_vm_core/execution_backend.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include <assert.h>
#include <string.h>

/* 保存 mock 回调计数、顺序和可控编译结果；栈上 fixture 生命周期覆盖 service；只供串行测试，无锁。 */
typedef struct SZrBackendFixture {
    TZrUInt32 compileCalls;
    TZrUInt32 cancelCalls;
    TZrUInt32 unregisterCalls;
    TZrUInt32 retireCalls;
    TZrUInt32 destroyCalls;
    TZrUInt32 resumeCalls;
    TZrUInt32 eventCount;
    TZrUInt32 events[16];
    TZrBool compileReturnsPending;
    TZrBool compileReturnsCode;
    TZrUInt64 nextCodeIdentity;
} SZrBackendFixture;

/* 用离散事件值检查撤图和退休的先后；事件标号仅为测试日志，不是 backend ABI。 */
enum {
    ZR_TEST_EVENT_COMPILE = 1,
    ZR_TEST_EVENT_CANCEL = 2,
    ZR_TEST_EVENT_UNREGISTER = 3,
    ZR_TEST_EVENT_RETIRE = 4,
    ZR_TEST_EVENT_DESTROY = 5,
    ZR_TEST_EVENT_RESUME = 6
};

/* 记录 mock 回调顺序，让退休测试能区分撤销图注册与释放代码。
 * 事件数组仅供本测试串行使用；容量由 assert 守护，不是线程安全日志。 */
static void test_event(SZrBackendFixture *fixture, TZrUInt32 event) {
    assert(fixture != ZR_NULL);
    assert(fixture->eventCount < (TZrUInt32)(sizeof(fixture->events) /
                                              sizeof(fixture->events[0])));
    fixture->events[fixture->eventCount++] = event;
}

static SZrExecutionBackendCompiledCode test_code(
        const SZrExecutionCompileRequest *request,
        TZrUInt64 codeIdentity);

/* 在 service 选择后端时只接受 HOST_JIT 的 typed-scalar 请求。
 * 不验证真实主机 ABI；mock target hash 只用于契约匹配。 */
static EZrExecutionBackendStatus test_query_target(
        const SZrExecutionBackendTarget *target,
        TZrUInt32 requiredOperations,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(target != ZR_NULL);
    assert(requiredOperations == ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR);
    return target->kind == ZR_EXECUTION_BACKEND_TARGET_HOST_JIT
            ? ZR_EXECUTION_BACKEND_STATUS_OK
            : ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
}

/* 由 ProcessNext 派发，按 fixture 开关模拟异步 pending、同步产物或编译失败。
 * 只返回元数据；未生成或执行机器码，完成产物仍需 service 校验和发布。 */
static EZrExecutionBackendStatus test_compile_async(
        const SZrExecutionBackendCompileInvocation *invocation,
        TZrPtr userData,
        SZrExecutionBackendCompiledCode *completedCode,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)completedCode;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(invocation != ZR_NULL);
    assert(invocation->ticket.ticketId != 0u);
    assert((invocation->request.flags &
            ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT) != 0u);
    /* 三种回调结果由 fixture 控制，同步产物通过相同 request 构造；pending 不写 completedCode。 */
    ++fixture->compileCalls;
    test_event(fixture, ZR_TEST_EVENT_COMPILE);
    if (fixture->compileReturnsCode) {
        assert(completedCode != ZR_NULL);
        *completedCode = test_code(&invocation->request,
                                   fixture->nextCodeIdentity);
        return ZR_EXECUTION_BACKEND_STATUS_OK;
    }
    return fixture->compileReturnsPending
            ? ZR_EXECUTION_BACKEND_STATUS_PENDING
            : ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED;
}

/* 为 service 的入口查询给出固定哨兵地址，证明查询被转发到已注册后端。
 * 0x1234 只比较数值，调用方不能执行它。 */
static EZrExecutionBackendStatus test_lookup_entry(
        const SZrExecutionBackendCodeInfo *code,
        TZrNativePtr *entryAddress,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(code != ZR_NULL && code->codeIdentity != 0u);
    assert(entryAddress != ZR_NULL);
    *entryAddress = (TZrNativePtr)0x1234;
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/* 用 map kind 生成固定 hash，核对 service 的 DEOPT 查询派发。
 * 当前场景仅断言 DEOPT 的 1003，不验证真实状态图内容。 */
static EZrExecutionBackendStatus test_query_map(
        const SZrExecutionBackendCodeInfo *code,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(code != ZR_NULL && mapHash != ZR_NULL);
    *mapHash = (TZrUInt64)(1000u + (TZrUInt32)mapKind);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/* 模拟可同步完成的取消回调，使关闭和失效场景检查取消次数。
 * 没有 worker 或异步取消协议，返回 OK 后即视为取消已完成。 */
static EZrExecutionBackendStatus test_cancel_compile(
        const SZrExecutionCompileTicket *ticket,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(ticket != ZR_NULL && ticket->ticketId != 0u);
    ++fixture->cancelCalls;
    test_event(fixture, ZR_TEST_EVENT_CANCEL);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/* 在释放或丢弃代码时记录图注册撤销，供测试检查先撤图后退休的顺序。
 * 不操作系统注册资源，只统计回调次数和事件。 */
static EZrExecutionBackendStatus test_unregister_maps(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(code != ZR_NULL && code->codeIdentity != 0u);
    ++fixture->unregisterCalls;
    test_event(fixture, ZR_TEST_EVENT_UNREGISTER);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/* 记录 mock 代码退休，让 lease 与重复完成场景核对回收次数。
 * 代码只含身份与 hash，不释放可执行内存。 */
static EZrExecutionBackendStatus test_retire(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    (void)diagnostic;
    assert(fixture != ZR_NULL);
    assert(code != ZR_NULL && code->codeIdentity != 0u);
    ++fixture->retireCalls;
    test_event(fixture, ZR_TEST_EVENT_RETIRE);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/* 记录 service 最终关闭后的后端销毁，核对活跃 lease 时仍保留后端。
 * fixture 是调用方栈对象；此回调不释放 userData。 */
static void test_destroy(TZrPtr userData) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    assert(fixture != ZR_NULL);
    ++fixture->destroyCalls;
    test_event(fixture, ZR_TEST_EVENT_DESTROY);
}

/* 供 ResumeInterpreter 派发，写入 sourceId 哨兵以核对诊断转发。
 * 不执行解释器，不检查 request 内部字段；diagnostic 可为空。 */
static TZrBool test_resume(const struct SZrExecIrResumeRequest *request,
                           TZrPtr userData,
                           SZrExecIrDiagnostic *diagnostic) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    assert(fixture != ZR_NULL);
    assert(request != ZR_NULL);
    ++fixture->resumeCalls;
    test_event(fixture, ZR_TEST_EVENT_RESUME);
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = 77u;
    }
    return ZR_TRUE;
}

/* 构造与请求 layout 匹配的 HOST_JIT mock target。
 * 固定 hash 不探测主机 ABI；仅用于 service 注册选择。 */
static SZrExecutionBackendTarget test_target(void) {
    SZrExecutionBackendTarget target;
    memset(&target, 0, sizeof(target));
    target.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    target.kind = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    target.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    target.targetTripleHash = 101u;
    target.layoutHash = 102u;
    target.capabilityHash = 103u;
    return target;
}

/* 把栈 fixture 接入完整 mock vtable，供每个 service 场景独立注册。
 * userData 借用 fixture，场景结束前必须完成 service 关闭。 */
static SZrExecutionBackendDescriptor test_descriptor(SZrBackendFixture *fixture) {
    SZrExecutionBackendDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.magic = ZR_EXECUTION_BACKEND_MAGIC;
    descriptor.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    descriptor.backendKind = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    descriptor.target = test_target();
    descriptor.supportedOperations = ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR;
    descriptor.vtable.queryTarget = test_query_target;
    descriptor.vtable.compileAsync = test_compile_async;
    descriptor.vtable.cancelCompile = test_cancel_compile;
    descriptor.vtable.lookupEntry = test_lookup_entry;
    descriptor.vtable.queryMap = test_query_map;
    descriptor.vtable.unregisterMaps = test_unregister_maps;
    descriptor.vtable.retire = test_retire;
    descriptor.vtable.destroy = test_destroy;
    descriptor.userData = fixture;
    return descriptor;
}

/* 为请求构造 domain/module/generation 命名空间，注册身份由 service 补齐。
 * 零初始化保留 backendRegistrationIdentity 为零；不能直接当作完成代码的完整键。 */
static SZrExecutionGenerationKey test_key(TZrUInt64 domain,
                                          TZrUInt64 module,
                                          TZrUInt64 generation) {
    SZrExecutionGenerationKey key;
    memset(&key, 0, sizeof(key));
    key.domainIdentity = domain;
    key.moduleIdentity = module;
    key.generation = generation;
    return key;
}

/* 构造允许 ExecBC fallback 的不可变 typed-scalar 请求，供各场景按需改动。
 * 四图要求与固定 source/instruction ID 用于完成校验和失败定位；不携带可执行 IR。 */
static SZrExecutionCompileRequest test_request(TZrUInt64 domain,
                                                TZrUInt64 module,
                                                TZrUInt64 generation) {
    SZrExecutionCompileRequest request;
    memset(&request, 0, sizeof(request));
    request.magic = ZR_EXECUTION_BACKEND_MAGIC;
    request.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    request.flags = ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT |
                    ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_EXECBC_FALLBACK;
    request.requestedTarget = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    request.generationKey = test_key(domain, module, generation);
    request.contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    request.contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    request.contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    request.contract.generation = generation;
    request.contract.targetToken = 41u;
    request.contract.signatureHash = 42u;
    request.contract.layoutHash = 102u;
    request.contract.moduleHash = module;
    request.contract.requiredCapabilities = ZR_EXECUTION_CAPABILITY_ARITHMETIC;
    request.contract.declaredEffects = ZR_EXECUTION_EFFECT_READ_MEMORY;
    request.immutableIrHash = 51u;
    request.compileInputHash = 52u;
    request.sourceId = 53u;
    request.instructionId = 54u;
    request.requiredOperations = ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR;
    request.requiredMapFlags = ZR_EXECUTION_BACKEND_MAP_ROOTS |
                               ZR_EXECUTION_BACKEND_MAP_EH |
                               ZR_EXECUTION_BACKEND_MAP_DEBUG |
                               ZR_EXECUTION_BACKEND_MAP_DEOPT;
    return request;
}

/* 复制请求契约和输入 hash，构造待 Complete 接收的 mock 产物。
 * 调用方须补 registration identity；固定图 hash 和 codeSize 不证明机器码可用。 */
static SZrExecutionBackendCompiledCode test_code(
        const SZrExecutionCompileRequest *request,
        TZrUInt64 codeIdentity) {
    SZrExecutionBackendCompiledCode code;
    assert(request != ZR_NULL);
    memset(&code, 0, sizeof(code));
    code.magic = ZR_EXECUTION_BACKEND_MAGIC;
    code.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    code.generationKey = request->generationKey;
    code.contract = request->contract;
    code.immutableIrHash = request->immutableIrHash;
    code.compileInputHash = request->compileInputHash;
    code.codeIdentity = codeIdentity;
    code.codeSize = 256u;
    code.mapRegistrationFlags = request->requiredMapFlags;
    code.rootMapHash = 61u;
    code.ehMapHash = 62u;
    code.debugMapHash = 63u;
    code.deoptMapHash = 64u;
    code.runtimeImportsHash = 65u;
    return code;
}

/* 覆盖排队不立即编译、pending 完成后显式发布，以及执行 lease 阻止失效代码回收。
 * 串行手动 Complete；只检查撤图先于退休的最后两个事件，并用 resume 哨兵检查诊断转发。 */
static void test_queue_complete_publish_and_lease(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[4];
    SZrExecutionCodeRecord codeRecords[4];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest request;
    SZrExecutionCompileTicket ticket;
    SZrExecutionCompileTicketView ticketView;
    SZrExecutionBackendCompiledCode code;
    SZrExecutionCodeHandle handle;
    SZrExecutionCodeView codeView;
    SZrExecutionBackendDiagnostic diagnostic;
    SZrExecIrDiagnostic resumeDiagnostic;
    SZrExecIrResumeRequest resumeRequest;
    TZrUInt32 collected;

    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsPending = ZR_TRUE;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 901u, registrations, 1u, jobs, 4u, codeRecords, 4u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);

    request = test_request(11u, 12u, 13u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ticket.generationKey.backendRegistrationIdentity ==
           registration.registrationIdentity);
    assert(ZrCore_ExecutionBackendService_QueryTicket(
                   &service, &ticket, &ticketView, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ticketView.state == ZR_EXECUTION_BACKEND_JOB_QUEUED);
    assert(fixture.compileCalls == 0u);

    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(fixture.compileCalls == 1u);
    assert(ZrCore_ExecutionBackendService_QueryTicket(
                   &service, &ticket, &ticketView, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ticketView.state == ZR_EXECUTION_BACKEND_JOB_COMPILING);

    code = test_code(&request, 71u);
    code.generationKey.backendRegistrationIdentity = registration.registrationIdentity;
    assert(ZrCore_ExecutionBackendService_Complete(
                   &service, &ticket, &code, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_QueryTicket(
                   &service, &ticket, &ticketView, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ticketView.state == ZR_EXECUTION_BACKEND_JOB_READY);
    assert(ZrCore_ExecutionBackendService_Publish(&service, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_QueryTicket(
                   &service, &ticket, &ticketView, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ticketView.state == ZR_EXECUTION_BACKEND_JOB_PUBLISHED);

    assert(ZrCore_ExecutionBackendService_AcquireCode(
                   &service, &ticket, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_QueryCode(
                   &service, &handle, &codeView, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(codeView.info.codeIdentity == 71u && codeView.leaseCount == 1u);
    assert(ZrCore_ExecutionBackendService_InvalidateGeneration(
                   &service, &ticket.generationKey, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    collected = 0u;
    assert(ZrCore_ExecutionBackendService_CollectRetired(
                   &service, &collected, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(collected == 0u && fixture.retireCalls == 0u);
    assert(ZrCore_ExecutionBackendService_ReleaseCode(
                   &service, &handle, &diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_CollectRetired(
                   &service, &collected, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(collected == 1u && fixture.unregisterCalls == 1u && fixture.retireCalls == 1u);
    assert(fixture.events[fixture.eventCount - 2u] == ZR_TEST_EVENT_UNREGISTER);
    assert(fixture.events[fixture.eventCount - 1u] == ZR_TEST_EVENT_RETIRE);

    memset(&resumeDiagnostic, 0, sizeof(resumeDiagnostic));
    memset(&resumeRequest, 0, sizeof(resumeRequest));
    assert(ZrCore_ExecutionBackendService_ResumeInterpreter(
                   &service, &resumeRequest,
                   &resumeDiagnostic));
    assert(fixture.resumeCalls == 1u && resumeDiagnostic.sourceId == 77u);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(fixture.destroyCalls == 1u);
}

/* 覆盖 worker 同步返回产物、入口与 DEOPT 查询，以及依赖 lease 必须先于执行 lease 释放。
 * 两个 lease 各为一；入口只是哨兵地址；未测试依赖 lease 计数并发或多持有者。 */
static void test_sync_compile_entry_map_and_dependency_lease(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[2];
    SZrExecutionCodeRecord codeRecords[2];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest request;
    SZrExecutionCompileTicket ticket;
    SZrExecutionCodeHandle handle;
    SZrExecutionCodeView view;
    SZrExecutionBackendDiagnostic diagnostic;
    TZrNativePtr entryAddress;
    TZrUInt64 mapHash;
    TZrUInt32 collected;

    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsCode = ZR_TRUE;
    fixture.nextCodeIdentity = 171u;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 904u, registrations, 1u, jobs, 2u, codeRecords, 2u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    request = test_request(51u, 52u, 53u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Publish(&service, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_AcquireCode(
                   &service, &ticket, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_AcquireDependencyLease(
                   &service, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_QueryCode(
                   &service, &handle, &view, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(view.leaseCount == 1u && view.dependencyLeaseCount == 1u);
    mapHash = 0u;
    assert(ZrCore_ExecutionBackendService_QueryMap(
                   &service, &handle, ZR_EXECUTION_BACKEND_MAP_KIND_DEOPT,
                   &mapHash, &diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(mapHash == 1003u);
    entryAddress = (TZrNativePtr)0;
    assert(ZrCore_ExecutionBackendService_LookupEntry(
                   &service, &handle, &entryAddress, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(entryAddress == (TZrNativePtr)0x1234);
    assert(ZrCore_ExecutionBackendService_ReleaseCode(
                   &service, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE);
    assert(ZrCore_ExecutionBackendService_ReleaseDependencyLease(
                   &service, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_ReleaseCode(
                   &service, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_InvalidateGeneration(
                   &service, &ticket.generationKey, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    collected = 0u;
    assert(ZrCore_ExecutionBackendService_CollectRetired(
                   &service, &collected, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(collected == 1u);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
}

/* 分别覆盖同步取消 pending 作业，以及活跃执行 lease 使 FinalizeShutdown 拒绝销毁。
 * 两次重新初始化使用同一栈空间；归还 lease 和 Collect 后才重试最终关闭。 */
static void test_shutdown_cancels_inflight_and_waits_for_active_lease(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[2];
    SZrExecutionCodeRecord codeRecords[2];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest request;
    SZrExecutionCompileTicket ticket;
    SZrExecutionCodeHandle handle;
    SZrExecutionBackendDiagnostic diagnostic;
    TZrUInt32 collected;

    /* A synchronous cancellation callback leaves no in-flight worker. */
    /* 取消回调同步返回 OK，没有遗留实际 worker；这与异步取消尚未完成的状态不同。 */
    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsPending = ZR_TRUE;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 905u, registrations, 1u, jobs, 2u, codeRecords, 2u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    request = test_request(61u, 62u, 63u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(fixture.cancelCalls == 1u);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);

    /* A retired code remains owned until its execution lease is released. */
    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsCode = ZR_TRUE;
    /* 活跃执行 lease 延迟 FinalizeShutdown；归还、Collect 后才验证销毁计数。 */
    fixture.nextCodeIdentity = 191u;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 906u, registrations, 1u, jobs, 2u, codeRecords, 2u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    request = test_request(71u, 72u, 73u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Publish(&service, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_AcquireCode(
                   &service, &ticket, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_InvalidateGeneration(
                   &service, &ticket.generationKey, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE);
    assert(fixture.destroyCalls == 0u);
    assert(ZrCore_ExecutionBackendService_ReleaseCode(
                   &service, &handle, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    collected = 0u;
    assert(ZrCore_ExecutionBackendService_CollectRetired(
                   &service, &collected, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(collected == 1u);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(fixture.destroyCalls == 1u);
}

/* 让两个不同请求返回相同 code identity，核对第二份产物被拒绝、撤图退休并保留请求位置。
 * 第二份失败不影响第一份已发布记录；最终关闭后两份产物各有一次撤图和退休。 */
static void test_duplicate_completion_is_disposed_and_failure_is_located(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[3];
    SZrExecutionCodeRecord codeRecords[3];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest firstRequest;
    SZrExecutionCompileRequest secondRequest;
    SZrExecutionCompileTicket firstTicket;
    SZrExecutionCompileTicket secondTicket;
    SZrExecutionBackendCompiledCode firstCode;
    SZrExecutionBackendCompiledCode duplicateCode;
    SZrExecutionBackendDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsPending = ZR_TRUE;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 907u, registrations, 1u, jobs, 3u, codeRecords, 3u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    firstRequest = test_request(81u, 82u, 83u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &firstRequest, &firstTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    firstCode = test_code(&firstRequest, 201u);
    firstCode.generationKey.backendRegistrationIdentity =
            registration.registrationIdentity;
    assert(ZrCore_ExecutionBackendService_Complete(
                   &service, &firstTicket, &firstCode, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Publish(&service, &firstTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);

    secondRequest = test_request(84u, 85u, 86u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &secondRequest, &secondTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    duplicateCode = test_code(&secondRequest, 201u);
    duplicateCode.generationKey.backendRegistrationIdentity =
            registration.registrationIdentity;
    assert(ZrCore_ExecutionBackendService_Complete(
                   &service, &secondTicket, &duplicateCode, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID);
    assert(secondTicket.state == ZR_EXECUTION_BACKEND_JOB_FAILED);
    assert(diagnostic.sourceId == secondRequest.sourceId &&
           diagnostic.instructionId == secondRequest.instructionId);
    assert(fixture.unregisterCalls == 1u && fixture.retireCalls == 1u);

    assert(ZrCore_ExecutionBackendService_InvalidateGeneration(
                   &service, &firstTicket.generationKey, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(fixture.unregisterCalls == 2u && fixture.retireCalls == 2u);
}

/* 覆盖三个 lease API 的全空参数拒绝，以及缺失不可变输入标志时的请求位置保留。
 * 没有触发编译失败回调；失败发生于 CompileAsync 入队前的请求验证。 */
static void test_immutable_input_failure_keeps_source_location(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[1];
    SZrExecutionCodeRecord codeRecords[1];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest request;
    SZrExecutionCompileTicket ticket;
    SZrExecutionBackendDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    assert(ZrCore_ExecutionBackendService_AcquireDependencyLease(
                   ZR_NULL, ZR_NULL, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
    assert(ZrCore_ExecutionBackendService_ReleaseDependencyLease(
                   ZR_NULL, ZR_NULL, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
    assert(ZrCore_ExecutionBackendService_ReleaseCode(
                   ZR_NULL, ZR_NULL, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 908u, registrations, 1u, jobs, 1u, codeRecords, 1u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    request = test_request(91u, 92u, 93u);
    request.flags &= ~ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT;
    memset(&ticket, 0, sizeof(ticket));
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_IMMUTABLE_INPUT_REQUIRED);
    assert(diagnostic.sourceId == request.sourceId &&
           diagnostic.instructionId == request.instructionId);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
}

/* 覆盖失效取消后的迟到完成清理、同键再次请求拒绝、ExecBC/AOT fallback 和强制机器码拒绝。
 * AOT fallback 只是状态选择；没有 AOT 或 ExecBC 执行；诊断保留原 unsupported 原因。 */
static void test_cancel_stale_and_fallback_are_explicit(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[2];
    SZrExecutionCodeRecord codeRecords[2];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest request;
    SZrExecutionCompileTicket ticket;
    SZrExecutionBackendCompiledCode staleCode;
    SZrExecutionBackendDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsPending = ZR_TRUE;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 902u, registrations, 1u, jobs, 2u, codeRecords, 2u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    request = test_request(21u, 22u, 23u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_InvalidateGeneration(
                   &service, &ticket.generationKey, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(fixture.cancelCalls == 1u);
    staleCode = test_code(&request, 81u);
    staleCode.generationKey.backendRegistrationIdentity = registration.registrationIdentity;
    assert(ZrCore_ExecutionBackendService_Complete(
                   &service, &ticket, &staleCode, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_CANCELLED);
    assert(fixture.unregisterCalls == 1u && fixture.retireCalls == 1u);
    request = test_request(21u, 22u, 23u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION);
    assert(diagnostic.sourceId == request.sourceId &&
           diagnostic.instructionId == request.instructionId);

    request = test_request(31u, 32u, 33u);
    request.requiredOperations = ZR_EXECUTION_BACKEND_OPERATION_ARRAY;
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC);
    assert(ticket.fallback == ZR_EXECUTION_BACKEND_FALLBACK_EXECBC);
    assert(diagnostic.status == ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION);
    request = test_request(34u, 35u, 36u);
    request.requestedTarget = ZR_EXECUTION_BACKEND_TARGET_AOT;
    request.flags |= ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_AOT_FALLBACK;
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT);
    assert(ticket.fallback == ZR_EXECUTION_BACKEND_FALLBACK_AOT);
    assert(diagnostic.status == ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET);
    request.flags |= ZR_EXECUTION_BACKEND_REQUEST_FLAG_REQUIRE_MACHINE_CODE;
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &request, &ticket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE);
    assert(ticket.fallback == ZR_EXECUTION_BACKEND_FALLBACK_NONE);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
}

/* 用相同 module/generation、不同 domain 的两份发布代码，核对失效第一份后仍能取得第二份。
 * 实际只改变 domain；不独立覆盖 module 或 backendRegistrationIdentity 差异。 */
static void test_generation_namespace_is_not_global(void) {
    SZrBackendFixture fixture;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[1];
    SZrExecutionCompileJob jobs[3];
    SZrExecutionCodeRecord codeRecords[3];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileRequest firstRequest;
    SZrExecutionCompileRequest secondRequest;
    SZrExecutionCompileTicket firstTicket;
    SZrExecutionCompileTicket secondTicket;
    SZrExecutionBackendCompiledCode firstCode;
    SZrExecutionBackendCompiledCode secondCode;
    SZrExecutionCodeHandle secondLease;
    SZrExecutionBackendDiagnostic diagnostic;

    memset(&fixture, 0, sizeof(fixture));
    fixture.compileReturnsPending = ZR_TRUE;
    assert(ZrCore_ExecutionBackendService_Init(
                   &service, 903u, registrations, 1u, jobs, 3u, codeRecords, 3u,
                   test_resume, &fixture, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    descriptor = test_descriptor(&fixture);
    assert(ZrCore_ExecutionBackendService_Register(
                   &service, &descriptor, &registration, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    firstRequest = test_request(41u, 42u, 7u);
    secondRequest = test_request(43u, 42u, 7u);
    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &firstRequest, &firstTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    firstCode = test_code(&firstRequest, 91u);
    firstCode.generationKey.backendRegistrationIdentity = registration.registrationIdentity;
    assert(ZrCore_ExecutionBackendService_Complete(
                   &service, &firstTicket, &firstCode, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Publish(&service, &firstTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);

    assert(ZrCore_ExecutionBackendService_CompileAsync(
                   &service, &secondRequest, &secondTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    assert(ZrCore_ExecutionBackendService_ProcessNext(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_PENDING);
    secondCode = test_code(&secondRequest, 92u);
    secondCode.generationKey.backendRegistrationIdentity = registration.registrationIdentity;
    assert(ZrCore_ExecutionBackendService_Complete(
                   &service, &secondTicket, &secondCode, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Publish(&service, &secondTicket, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_InvalidateGeneration(
                   &service, &firstTicket.generationKey, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_AcquireCode(
                   &service, &secondTicket, &secondLease, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_ReleaseCode(
                   &service, &secondLease, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_Shutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
    assert(ZrCore_ExecutionBackendService_FinalizeShutdown(&service, &diagnostic) ==
           ZR_EXECUTION_BACKEND_STATUS_OK);
}

/* 由 CTest 的 ssa_backend_service 入口串行运行七个 mock service 契约场景。
 * API 调用多在 assert 内，验证需启用断言；没有真实 worker 或 JIT 执行。 */
int main(void) {
    test_queue_complete_publish_and_lease();
    test_sync_compile_entry_map_and_dependency_lease();
    test_shutdown_cancels_inflight_and_waits_for_active_lease();
    test_duplicate_completion_is_disposed_and_failure_is_located();
    test_immutable_input_failure_keeps_source_location();
    test_cancel_stale_and_fallback_are_explicit();
    test_generation_namespace_is_not_global();
    return 0;
}
