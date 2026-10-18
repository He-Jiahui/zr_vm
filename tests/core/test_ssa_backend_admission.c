/* Scalar admission tests: no IR, entry address, worker, or runtime execution. */
#include "zr_vm_core/execution_backend.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_ABI ((TZrUInt32)23u)
#define TEST_CODE_ID ((TZrUInt64)77u)
enum { TEST_CAPACITY = 2 };
typedef struct TestFixture {
    const char *name;
    SZrExecutionBackendService service;
    SZrExecutionBackendRegistrationRecord registrations[TEST_CAPACITY];
    SZrExecutionCompileJob jobs[TEST_CAPACITY];
    SZrExecutionCodeRecord codes[TEST_CAPACITY];
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendCompileInvocation invocation;
    SZrExecutionCompileTicket ticket;
    SZrExecutionBackendDiagnostic diagnostic;
    unsigned failures, queryCalls, compileCalls, unregisterCalls, retireCalls;
    unsigned destroyCalls, registered, eventCount;
    char events[8];
    TZrBool initialized;
} TestFixture;

static unsigned caseCount, failedCases;
static void check(TestFixture *f, int condition, const char *text, int line) {
    if (!condition) {
        ++f->failures;
        fprintf(stderr, "FAIL name=%s line=%d check=%s\n", f->name, line, text);
    }
}
#define CHECK(F, CONDITION) check((F), !!(CONDITION), #CONDITION, __LINE__)

static void event(TestFixture *f, char value) {
    CHECK(f, f->eventCount < sizeof(f->events));
    if (f->eventCount < sizeof(f->events)) f->events[f->eventCount++] = value;
}
static EZrExecutionBackendStatus query_target(
        const SZrExecutionBackendTarget *target, TZrUInt32 operations,
        TZrPtr userData, SZrExecutionBackendDiagnostic *diagnostic) {
    TestFixture *f = (TestFixture *)userData;
    (void)diagnostic;
    ++f->queryCalls;
    CHECK(f, target != ZR_NULL);
    if (target == ZR_NULL) return ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
    CHECK(f, target->kind == ZR_EXECUTION_BACKEND_TARGET_HOST_JIT);
    CHECK(f, target->abiVersion == TEST_ABI);
    CHECK(f, target->reserved == 0u);
    CHECK(f, operations == ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}
static EZrExecutionBackendStatus compile_pending(
        const SZrExecutionBackendCompileInvocation *invocation, TZrPtr userData,
        SZrExecutionBackendCompiledCode *completedCode,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TestFixture *f = (TestFixture *)userData;
    (void)completedCode;
    (void)diagnostic;
    ++f->compileCalls;
    CHECK(f, invocation != ZR_NULL);
    if (invocation != ZR_NULL) {
        f->invocation = *invocation; /* The callback never retains its pointer. */
        CHECK(f, invocation->ticket.ticketId != 0u);
        CHECK(f, invocation->request.contract.abiVersion == TEST_ABI);
    }
    return ZR_EXECUTION_BACKEND_STATUS_PENDING;
}
static EZrExecutionBackendStatus unregister_maps(
        const SZrExecutionBackendCodeInfo *code, TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TestFixture *f = (TestFixture *)userData;
    (void)diagnostic;
    CHECK(f, code != ZR_NULL && code->codeIdentity == TEST_CODE_ID);
    ++f->unregisterCalls;
    event(f, 'U');
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}
static EZrExecutionBackendStatus retire_code(
        const SZrExecutionBackendCodeInfo *code, TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TestFixture *f = (TestFixture *)userData;
    (void)diagnostic;
    CHECK(f, code != ZR_NULL && code->codeIdentity == TEST_CODE_ID);
    ++f->retireCalls;
    event(f, 'R');
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}
static void destroy_backend(TZrPtr userData) {
    TestFixture *f = (TestFixture *)userData;
    ++f->destroyCalls;
    event(f, 'D'); /* userData and all service storage remain stack-owned. */
}
static SZrExecutionBackendDescriptor descriptor(TestFixture *f) {
    SZrExecutionBackendDescriptor d;
    memset(&d, 0, sizeof(d));
    d.magic = ZR_EXECUTION_BACKEND_MAGIC;
    d.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    d.backendKind = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    d.target.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    d.target.kind = d.backendKind;
    d.target.abiVersion = TEST_ABI;
    d.target.targetTripleHash = 101u;
    d.target.layoutHash = 102u;
    d.target.capabilityHash = 103u;
    d.supportedOperations = ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR;
    d.vtable.queryTarget = query_target;
    d.vtable.compileAsync = compile_pending;
    d.vtable.unregisterMaps = unregister_maps;
    d.vtable.retire = retire_code;
    d.vtable.destroy = destroy_backend;
    d.userData = f;
    return d;
}
static SZrExecutionCompileRequest request(void) {
    SZrExecutionCompileRequest r;
    memset(&r, 0, sizeof(r));
    r.magic = ZR_EXECUTION_BACKEND_MAGIC;
    r.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    r.flags = ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT;
    r.requestedTarget = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    r.generationKey.domainIdentity = 11u;
    r.generationKey.moduleIdentity = 12u;
    r.generationKey.generation = 1u;
    r.contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    r.contract.abiVersion = TEST_ABI;
    r.contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    r.contract.generation = r.generationKey.generation;
    r.contract.targetToken = 13u;
    r.contract.signatureHash = 14u;
    r.contract.layoutHash = 102u;
    r.contract.moduleHash = r.generationKey.moduleIdentity;
    r.immutableIrHash = 15u;
    r.compileInputHash = 16u;
    r.sourceId = 17u;
    r.instructionId = 18u;
    r.requiredOperations = ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR;
    r.requiredMapFlags = ZR_EXECUTION_BACKEND_MAP_ROOTS;
    return r;
}
static SZrExecutionBackendCompiledCode completed_code(TestFixture *f) {
    const SZrExecutionCompileRequest *r = &f->invocation.request;
    SZrExecutionBackendCompiledCode c;
    memset(&c, 0, sizeof(c));
    c.magic = ZR_EXECUTION_BACKEND_MAGIC;
    c.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    c.generationKey = r->generationKey;
    c.contract = r->contract;
    c.immutableIrHash = r->immutableIrHash;
    c.compileInputHash = r->compileInputHash;
    c.codeIdentity = TEST_CODE_ID;
    c.codeSize = 1u; /* A metadata size, with no corresponding executable buffer. */
    c.mapRegistrationFlags = ZR_EXECUTION_BACKEND_MAP_ROOTS;
    c.rootMapHash = 19u;
    return c;
}
static void begin_case(TestFixture *f, const char *name) {
    EZrExecutionBackendStatus s;
    memset(f, 0, sizeof(*f));
    f->name = name;
    s = ZrCore_ExecutionBackendService_Init(&f->service, 20u,
            f->registrations, TEST_CAPACITY, f->jobs, TEST_CAPACITY,
            f->codes, TEST_CAPACITY, ZR_NULL, ZR_NULL, &f->diagnostic);
    f->initialized = (TZrBool)(s == ZR_EXECUTION_BACKEND_STATUS_OK);
    CHECK(f, f->initialized);
}
static EZrExecutionBackendStatus register_backend(
        TestFixture *f, const SZrExecutionBackendDescriptor *d) {
    EZrExecutionBackendStatus s = ZrCore_ExecutionBackendService_Register(
            &f->service, d, &f->registration, &f->diagnostic);
    if (s == ZR_EXECUTION_BACKEND_STATUS_OK) ++f->registered;
    return s;
}
static int setup_registered(TestFixture *f) {
    SZrExecutionBackendDescriptor d = descriptor(f);
    if (!f->initialized) return 0;
    CHECK(f, TEST_ABI != ZR_EXECUTION_CONTRACT_ABI_VERSION);
    CHECK(f, register_backend(f, &d) == ZR_EXECUTION_BACKEND_STATUS_OK);
    return f->registered != 0u;
}
/* Also drain an unexpected COMPILING result before shutdown. This bounded
 * recovery is after each oracle observation and never invokes optional cancel. */
static void finish_case(TestFixture *f, EZrExecutionBackendStatus observed) {
    if (f->initialized) {
        SZrExecutionCompileTicketView view = {0};
        if (f->ticket.ticketId != 0u &&
            ZrCore_ExecutionBackendService_QueryTicket(&f->service, &f->ticket,
                    &view, &f->diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK &&
            view.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            SZrExecutionBackendCompiledCode c = completed_code(f);
            (void)ZrCore_ExecutionBackendService_Complete(
                    &f->service, &f->ticket, &c, &f->diagnostic);
        }
        CHECK(f, ZrCore_ExecutionBackendService_Shutdown(&f->service,
                &f->diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(f, ZrCore_ExecutionBackendService_FinalizeShutdown(&f->service,
                &f->diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(f, f->destroyCalls == f->registered);
        CHECK(f, f->unregisterCalls == f->retireCalls);
        CHECK(f, f->service.backendCallbackInFlight == 0u);
        CHECK(f, f->service.queryTargetInFlight == 0u);
        CHECK(f, f->service.codeCount == 0u);
        CHECK(f, f->service.registrationCount == 0u);
        ZrCore_ExecutionBackendService_Deinit(&f->service);
        CHECK(f, f->service.magic == 0u);
        CHECK(f, f->service.jobs == ZR_NULL && f->service.codes == ZR_NULL);
        CHECK(f, f->service.jobCount == 0u);
        CHECK(f, f->eventCount == f->unregisterCalls + f->retireCalls + f->destroyCalls);
        if (f->registered != 0u) {
            CHECK(f, f->eventCount != 0u && f->events[f->eventCount - 1u] == 'D');
        }
    }
    ++caseCount;
    if (f->failures != 0u) ++failedCases;
    printf("CASE name=%s status=%s pass=%u query=%u compile=%u U=%u R=%u D=%u\n",
            f->name, ZrCore_ExecutionBackend_StatusName(observed),
            (unsigned)(f->failures == 0u), f->queryCalls, f->compileCalls,
            f->unregisterCalls, f->retireCalls, f->destroyCalls);
}
static int enqueue_and_process(TestFixture *f) {
    SZrExecutionCompileRequest r = request();
    unsigned queryBefore = f->queryCalls, compileBefore = f->compileCalls;
    EZrExecutionBackendStatus s = ZrCore_ExecutionBackendService_CompileAsync(
            &f->service, &r, &f->ticket, &f->diagnostic);
    CHECK(f, s == ZR_EXECUTION_BACKEND_STATUS_PENDING);
    if (s != ZR_EXECUTION_BACKEND_STATUS_PENDING) return 0;
    s = ZrCore_ExecutionBackendService_ProcessNext(&f->service, &f->diagnostic);
    CHECK(f, s == ZR_EXECUTION_BACKEND_STATUS_PENDING);
    CHECK(f, f->queryCalls == queryBefore + 1u);
    CHECK(f, f->compileCalls == compileBefore + 1u);
    CHECK(f, f->invocation.ticket.ticketId == f->ticket.ticketId);
    CHECK(f, f->invocation.request.sourceId == r.sourceId);
    CHECK(f, f->invocation.request.compileInputHash == r.compileInputHash);
    return s == ZR_EXECUTION_BACKEND_STATUS_PENDING;
}
static void healthy_case(void) {
    TestFixture f;
    EZrExecutionBackendStatus s = ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED;
    begin_case(&f, "healthy_non17");
    if (setup_registered(&f) && enqueue_and_process(&f)) {
        SZrExecutionBackendCompiledCode c = completed_code(&f);
        SZrExecutionCompileTicketView view = {0};
        TZrUInt32 collected = 0u;
        s = ZrCore_ExecutionBackendService_Complete(
                &f.service, &f.ticket, &c, &f.diagnostic);
        CHECK(&f, s == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(&f, ZrCore_ExecutionBackendService_QueryTicket(&f.service, &f.ticket,
                &view, &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(&f, view.state == ZR_EXECUTION_BACKEND_JOB_READY);
        CHECK(&f, ZrCore_ExecutionBackendService_Publish(&f.service, &f.ticket,
                &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(&f, ZrCore_ExecutionBackendService_InvalidateGeneration(&f.service,
                &f.ticket.generationKey, &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(&f, ZrCore_ExecutionBackendService_CollectRetired(&f.service,
                &collected, &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
        CHECK(&f, collected == 1u);
        CHECK(&f, f.eventCount == 2u && f.events[0] == 'U' && f.events[1] == 'R');
    }
    finish_case(&f, s);
}
/* Diagnostics are public scalar facts, independent of checker internals. */
static void diagnostic_values(TestFixture *f, EZrExecutionBackendStatus status,
        TZrUInt32 expectedState, TZrUInt32 actualState,
        TZrUInt64 expected, TZrUInt64 actual) {
    CHECK(f, f->diagnostic.status == status);
    CHECK(f, f->diagnostic.expectedState == expectedState);
    CHECK(f, f->diagnostic.actualState == actualState);
    CHECK(f, f->diagnostic.expected == expected);
    CHECK(f, f->diagnostic.actual == actual);
}
static void registration_case(unsigned variant) {
    static const char *const names[] = {
        "descriptor_reserved", "target_reserved", "register_flags_priority",
        "register_target_priority", "register_operation_priority"
    };
    TestFixture f;
    SZrExecutionBackendDescriptor d;
    unsigned char before[sizeof(d)];
    EZrExecutionBackendStatus expected = ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH;
    EZrExecutionBackendStatus s = ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED;
    TZrUInt32 expectedState = 0u, actualState = 0u;
    TZrUInt64 expectedValue = 0u, actualValue = 1u;
    begin_case(&f, names[variant]);
    d = descriptor(&f);
    if (variant == 1u) d.target.reserved = 1u; else d.reserved = 1u;
    if (variant == 2u) {
        d.flags = (TZrUInt32)1u << 31u;
        d.target.kind = ZR_EXECUTION_BACKEND_TARGET_EXECBC;
        d.supportedOperations = 0u;
        expected = ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS;
        expectedState = ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_KNOWN_MASK;
        actualState = d.flags;
        actualValue = 0u;
    }
    if (variant == 3u) {
        d.target.kind = ZR_EXECUTION_BACKEND_TARGET_EXECBC;
        d.supportedOperations = 0u;
        expected = ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
        expectedState = (TZrUInt32)d.backendKind;
        actualState = (TZrUInt32)d.target.kind;
        actualValue = 0u;
    }
    if (variant == 4u) {
        d.supportedOperations = 0u;
        expected = ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION;
        expectedState = ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK;
        actualValue = 0u;
    }
    memcpy(before, &d, sizeof(d));
    if (f.initialized) {
        memset(&f.diagnostic, 0xa5, sizeof(f.diagnostic));
        s = register_backend(&f, &d);
        CHECK(&f, s == expected);
        diagnostic_values(&f, expected, expectedState, actualState,
                expectedValue, actualValue);
        CHECK(&f, f.diagnostic.sourceId == 0u && f.diagnostic.instructionId == 0u);
        CHECK(&f, f.service.registrationCount == 0u);
        CHECK(&f, f.registration.registrationIdentity == 0u);
        CHECK(&f, f.queryCalls == 0u && f.compileCalls == 0u);
        CHECK(&f, memcmp(before, &d, sizeof(d)) == 0);
    }
    finish_case(&f, s);
}
static void request_case(unsigned variant) {
    static const char *const names[] = {
        "logical_next", "logical_max", "request_reserved0",
        "request_reserved1", "immutable_priority", "request_schema",
        "request_contract_schema", "logical_zero", "request_flags_priority",
        "request_null_diag_logical", "request_null_diag_reserved"
    };
    TestFixture f;
    SZrExecutionCompileRequest r = request();
    unsigned char before[sizeof(r)], diagnosticBefore[sizeof(f.diagnostic)];
    EZrExecutionBackendStatus expected = ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH;
    EZrExecutionBackendStatus s = ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED;
    TZrUInt32 expectedState = 0u, actualState = 0u;
    TZrUInt64 expectedValue = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    TZrUInt64 actualValue = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION + 1u;
    TZrBool nullDiagnostic = (TZrBool)(variant >= 9u);
    begin_case(&f, names[variant]);
    if (variant == 0u || variant == 9u) r.contract.logicalVersion = (TZrUInt32)actualValue;
    if (variant == 1u) {
        r.contract.logicalVersion = UINT32_MAX;
        actualValue = UINT32_MAX;
    }
    if (variant == 2u || variant == 3u || variant == 10u) {
        if (variant == 3u) r.contract.reserved1 = 1u; else r.contract.reserved0 = 1u;
        expectedValue = 0u;
        actualValue = 1u;
    }
    if (variant == 4u || variant == 8u) {
        r.flags = variant == 8u ? ((TZrUInt32)1u << 31u) : 0u;
        r.contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION + 1u;
        r.contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION + 1u;
        r.contract.reserved0 = 1u;
        expected = variant == 8u ? ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS
                                : ZR_EXECUTION_BACKEND_STATUS_IMMUTABLE_INPUT_REQUIRED;
        expectedState = variant == 8u ? ZR_EXECUTION_BACKEND_REQUEST_FLAG_KNOWN_MASK
                                     : ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT;
        actualState = r.flags;
        expectedValue = 0u;
        actualValue = 0u;
    }
    if (variant == 5u) {
        r.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION + 1u;
        expected = ZR_EXECUTION_BACKEND_STATUS_INVALID_SCHEMA;
        expectedState = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
        actualState = r.schemaVersion;
        expectedValue = 0u;
        actualValue = 0u;
    }
    if (variant == 6u) {
        r.contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION + 1u;
        expectedValue = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
        actualValue = r.contract.schemaVersion;
    }
    if (variant == 7u) {
        r.contract.logicalVersion = 0u;
        actualValue = 0u;
    }
    memcpy(before, &r, sizeof(r));
    if (setup_registered(&f)) {
        memset(&f.diagnostic, 0xa5, sizeof(f.diagnostic));
        memcpy(diagnosticBefore, &f.diagnostic, sizeof(f.diagnostic));
        s = ZrCore_ExecutionBackendService_CompileAsync(&f.service, &r,
                &f.ticket, nullDiagnostic ? ZR_NULL : &f.diagnostic);
        CHECK(&f, s == expected);
        if (nullDiagnostic) {
            CHECK(&f, memcmp(diagnosticBefore, &f.diagnostic, sizeof(f.diagnostic)) == 0);
        } else {
            diagnostic_values(&f, expected, expectedState, actualState,
                    expectedValue, actualValue);
            CHECK(&f, f.diagnostic.sourceId == r.sourceId);
            CHECK(&f, f.diagnostic.instructionId == r.instructionId);
        }
        CHECK(&f, f.service.jobCount == 0u && f.ticket.ticketId == 0u);
        CHECK(&f, f.service.codeCount == 0u);
        CHECK(&f, f.queryCalls == 0u && f.compileCalls == 0u);
        CHECK(&f, memcmp(before, &r, sizeof(r)) == 0);
    }
    finish_case(&f, s);
}
typedef enum ResultMutation {
    RESULT_RESERVED0, RESULT_RESERVED1, RESULT_SCHEMA, RESULT_ABI,
    RESULT_LOGICAL, RESULT_TOKEN, RESULT_SIGNATURE, RESULT_LAYOUT, RESULT_MODULE,
    RESULT_CAPABILITY, RESULT_EFFECT, RESULT_KEY_GENERATION,
    RESULT_CONTRACT_GENERATION, RESULT_NULL_DIAGNOSTIC, RESULT_CONTRACT_PRIORITY
} ResultMutation;
typedef struct ResultCase {
    const char *name;
    ResultMutation mutation;
    TZrUInt64 expected, actual;
    EZrExecutionBackendStatus status;
} ResultCase;
#define CONTRACT_CASE(NAME, FIELD, EXPECTED, ACTUAL) \
    {NAME, FIELD, EXPECTED, ACTUAL, ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH}
static const ResultCase resultCases[] = {
    CONTRACT_CASE("result_reserved0", RESULT_RESERVED0, 0u, 1u),
    CONTRACT_CASE("result_reserved1", RESULT_RESERVED1, 0u, 1u),
    CONTRACT_CASE("result_schema", RESULT_SCHEMA, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION,
            ZR_EXECUTION_CONTRACT_SCHEMA_VERSION + 1u),
    CONTRACT_CASE("result_abi", RESULT_ABI, TEST_ABI, TEST_ABI + 1u),
    CONTRACT_CASE("result_logical", RESULT_LOGICAL, ZR_EXECUTION_CONTRACT_LOGICAL_VERSION,
            ZR_EXECUTION_CONTRACT_LOGICAL_VERSION + 1u),
    CONTRACT_CASE("result_token", RESULT_TOKEN, 13u, 113u),
    CONTRACT_CASE("result_signature", RESULT_SIGNATURE, 14u, 114u),
    CONTRACT_CASE("result_layout", RESULT_LAYOUT, 102u, 202u),
    CONTRACT_CASE("result_module", RESULT_MODULE, 12u, 112u),
    CONTRACT_CASE("result_capability", RESULT_CAPABILITY, 0u, ZR_EXECUTION_CAPABILITY_ARITHMETIC),
    CONTRACT_CASE("result_effect", RESULT_EFFECT, 0u, ZR_EXECUTION_EFFECT_READ_MEMORY),
    {"result_generation_reserved", RESULT_KEY_GENERATION, 1u, 2u,
            ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION},
    {"result_contract_generation", RESULT_CONTRACT_GENERATION, 1u, 2u,
            ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION},
    CONTRACT_CASE("result_null_diag_reserved", RESULT_NULL_DIAGNOSTIC, 0u, 1u),
    CONTRACT_CASE("result_contract_priority", RESULT_CONTRACT_PRIORITY,
            ZR_EXECUTION_CONTRACT_SCHEMA_VERSION, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION + 1u)
};
#undef CONTRACT_CASE
static void mutate_result(SZrExecutionBackendCompiledCode *c, const ResultCase *row) {
    switch (row->mutation) {
        case RESULT_RESERVED0: c->contract.reserved0 = (TZrUInt32)row->actual; break;
        case RESULT_RESERVED1: c->contract.reserved1 = (TZrUInt32)row->actual; break;
        case RESULT_SCHEMA: c->contract.schemaVersion = (TZrUInt32)row->actual; break;
        case RESULT_ABI: c->contract.abiVersion = (TZrUInt32)row->actual; break;
        case RESULT_LOGICAL: c->contract.logicalVersion = (TZrUInt32)row->actual; break;
        case RESULT_TOKEN: c->contract.targetToken = (TZrMetadataToken)row->actual; break;
        case RESULT_SIGNATURE: c->contract.signatureHash = row->actual; break;
        case RESULT_LAYOUT: c->contract.layoutHash = row->actual; break;
        case RESULT_MODULE: c->contract.moduleHash = row->actual; break;
        case RESULT_CAPABILITY: c->contract.requiredCapabilities = (TZrUInt32)row->actual; break;
        case RESULT_EFFECT: c->contract.declaredEffects = (TZrUInt32)row->actual; break;
        case RESULT_KEY_GENERATION:
            c->generationKey.generation = row->actual;
            c->contract.reserved0 = 1u;
            break;
        case RESULT_CONTRACT_GENERATION: c->contract.generation = row->actual; break;
        case RESULT_NULL_DIAGNOSTIC: c->contract.reserved0 = (TZrUInt32)row->actual; break;
        case RESULT_CONTRACT_PRIORITY:
            c->contract.schemaVersion = (TZrUInt32)row->actual;
            c->contract.abiVersion = TEST_ABI + 1u;
            c->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION + 1u;
            c->contract.reserved0 = 1u;
            c->contract.reserved1 = 1u;
            break;
    }
}
/* Read the diagnostic before QueryTicket clears it. The result is not mutable
 * service storage, and must remain byte-identical even when it is rejected. */
static EZrExecutionBackendStatus reject_result(TestFixture *f,
        const SZrExecutionBackendCompiledCode *c, const ResultCase *row) {
    SZrExecutionCompileTicketView view = {0};
    unsigned char before[sizeof(*c)], diagnosticBefore[sizeof(f->diagnostic)];
    unsigned u = f->unregisterCalls, r = f->retireCalls, e = f->eventCount;
    TZrBool nullDiagnostic = (TZrBool)(row->mutation == RESULT_NULL_DIAGNOSTIC);
    EZrExecutionBackendStatus s;
    memcpy(before, c, sizeof(*c));
    memset(&f->diagnostic, 0xa5, sizeof(f->diagnostic));
    memcpy(diagnosticBefore, &f->diagnostic, sizeof(f->diagnostic));
    s = ZrCore_ExecutionBackendService_Complete(&f->service, &f->ticket, c,
            nullDiagnostic ? ZR_NULL : &f->diagnostic);
    CHECK(f, s == row->status);
    if (nullDiagnostic) {
        CHECK(f, memcmp(diagnosticBefore, &f->diagnostic, sizeof(f->diagnostic)) == 0);
    } else {
        diagnostic_values(f, row->status, 0u, 0u, row->expected, row->actual);
        CHECK(f, f->diagnostic.sourceId == f->invocation.request.sourceId);
        CHECK(f, f->diagnostic.instructionId == f->invocation.request.instructionId);
        CHECK(f, f->diagnostic.ticketId == f->ticket.ticketId);
        CHECK(f, f->diagnostic.codeIdentity == c->codeIdentity);
    }
    CHECK(f, f->service.codeCount == 0u);
    CHECK(f, ZrCore_ExecutionBackendService_QueryTicket(&f->service, &f->ticket,
            &view, ZR_NULL) == ZR_EXECUTION_BACKEND_STATUS_OK);
    CHECK(f, view.state == ZR_EXECUTION_BACKEND_JOB_FAILED);
    CHECK(f, view.lastStatus == row->status);
    CHECK(f, f->unregisterCalls == u + 1u && f->retireCalls == r + 1u);
    CHECK(f, f->eventCount == e + 2u);
    if (f->eventCount == e + 2u && e + 1u < sizeof(f->events)) {
        CHECK(f, f->events[e] == 'U' && f->events[e + 1u] == 'R');
    }
    CHECK(f, memcmp(before, c, sizeof(*c)) == 0);
    return s;
}
static void result_case(unsigned variant) {
    const ResultCase *row = &resultCases[variant];
    TestFixture f;
    EZrExecutionBackendStatus s = ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED;
    begin_case(&f, row->name);
    if (setup_registered(&f) && enqueue_and_process(&f)) {
        SZrExecutionBackendCompiledCode c = completed_code(&f);
        mutate_result(&c, row);
        s = reject_result(&f, &c, row);
    }
    finish_case(&f, s);
}
static void recovery_case(void) {
    TestFixture f;
    unsigned round;
    TZrUInt32 collected = 0u;
    EZrExecutionBackendStatus s = ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED;
    begin_case(&f, "failure_then_healthy");
    if (setup_registered(&f)) {
        /* Fill both job slots with terminal failures, then require real reuse.
         * Failed tickets may remain queryable; they must own no code or maps. */
        for (round = 0u; round < (unsigned)TEST_CAPACITY; ++round) {
            if (enqueue_and_process(&f)) {
                SZrExecutionBackendCompiledCode bad = completed_code(&f);
                bad.contract.reserved0 = 1u;
                (void)reject_result(&f, &bad, &resultCases[0]);
            }
        }
        if (enqueue_and_process(&f)) {
            SZrExecutionBackendCompiledCode good = completed_code(&f);
            SZrExecutionCompileTicketView view = {0};
            s = ZrCore_ExecutionBackendService_Complete(
                    &f.service, &f.ticket, &good, &f.diagnostic);
            CHECK(&f, s == ZR_EXECUTION_BACKEND_STATUS_OK);
            CHECK(&f, ZrCore_ExecutionBackendService_Publish(&f.service, &f.ticket,
                    &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
            CHECK(&f, ZrCore_ExecutionBackendService_QueryTicket(&f.service, &f.ticket,
                    &view, &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
            CHECK(&f, view.state == ZR_EXECUTION_BACKEND_JOB_PUBLISHED);
            CHECK(&f, f.service.codeCount == 1u && f.service.jobCount == (TZrUInt32)TEST_CAPACITY);
            CHECK(&f, ZrCore_ExecutionBackendService_InvalidateGeneration(&f.service,
                    &f.ticket.generationKey, &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
            CHECK(&f, ZrCore_ExecutionBackendService_CollectRetired(&f.service,
                    &collected, &f.diagnostic) == ZR_EXECUTION_BACKEND_STATUS_OK);
            CHECK(&f, collected == 1u && f.service.codeCount == 0u);
            CHECK(&f, f.service.backendCallbackInFlight == 0u);
            CHECK(&f, f.service.queryTargetInFlight == 0u);
            CHECK(&f, f.queryCalls == 3u && f.compileCalls == 3u);
            CHECK(&f, f.unregisterCalls == 3u && f.retireCalls == 3u);
        }
    }
    finish_case(&f, s);
}
int main(void) {
    unsigned i;
    /* Original ten cases retain their order and rejection/lifetime oracles. */
    healthy_case();
    registration_case(0u);
    registration_case(1u);
    for (i = 0u; i < 5u; ++i) request_case(i);
    result_case(0u);
    result_case(1u);
    for (i = 2u; i < 5u; ++i) registration_case(i);
    for (i = 5u; i < 11u; ++i) request_case(i);
    for (i = 2u; i < sizeof(resultCases) / sizeof(resultCases[0]); ++i) result_case(i);
    recovery_case();
    printf("SUMMARY cases=%u failed=%u\n", caseCount, failedCases);
    return failedCases == 0u && caseCount == 33u ? 0 : 1;
}
