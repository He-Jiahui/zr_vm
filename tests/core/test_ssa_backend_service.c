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

enum {
    ZR_TEST_EVENT_COMPILE = 1,
    ZR_TEST_EVENT_CANCEL = 2,
    ZR_TEST_EVENT_UNREGISTER = 3,
    ZR_TEST_EVENT_RETIRE = 4,
    ZR_TEST_EVENT_DESTROY = 5,
    ZR_TEST_EVENT_RESUME = 6
};

static void test_event(SZrBackendFixture *fixture, TZrUInt32 event) {
    assert(fixture != ZR_NULL);
    assert(fixture->eventCount < (TZrUInt32)(sizeof(fixture->events) /
                                              sizeof(fixture->events[0])));
    fixture->events[fixture->eventCount++] = event;
}

static SZrExecutionBackendCompiledCode test_code(
        const SZrExecutionCompileRequest *request,
        TZrUInt64 codeIdentity);

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

static void test_destroy(TZrPtr userData) {
    SZrBackendFixture *fixture = (SZrBackendFixture *)userData;
    assert(fixture != ZR_NULL);
    ++fixture->destroyCalls;
    test_event(fixture, ZR_TEST_EVENT_DESTROY);
}

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
