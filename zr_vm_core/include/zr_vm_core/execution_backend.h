#ifndef ZR_VM_CORE_EXECUTION_BACKEND_H
#define ZR_VM_CORE_EXECUTION_BACKEND_H

/*
 * Runtime execution-backend service contract (SSA plan 10.01).
 *
 * The compile request, generation key, and code information are immutable
 * scalar witnesses.  They may be copied into a queue or a diagnostic record;
 * they deliberately contain no VM object, AST, executable address, or
 * callback pointer.  Callback pointers and the service/record pointers below
 * are process-local runtime state only and must never be written to an
 * artifact, CallBinding row, or hot-update package.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_EXECUTION_BACKEND_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXECUTION_BACKEND_MAGIC ((TZrUInt32)0x314b4245u) /* EBK1 */
#define ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY ((TZrUInt32)16u)

/* Operations are a capability mask; every requested bit must be supported. */
#define ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_OPERATION_CONTROL ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_OPERATION_DIRECT_CALL ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_BACKEND_OPERATION_SIMPLE_MEMBER ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_BACKEND_OPERATION_ARRAY ((TZrUInt32)1u << 4u)
#define ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR | \
     ZR_EXECUTION_BACKEND_OPERATION_CONTROL | \
     ZR_EXECUTION_BACKEND_OPERATION_DIRECT_CALL | \
     ZR_EXECUTION_BACKEND_OPERATION_SIMPLE_MEMBER | \
     ZR_EXECUTION_BACKEND_OPERATION_ARRAY)

/* Metadata registrations required before a code record can be published. */
#define ZR_EXECUTION_BACKEND_MAP_ROOTS ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_MAP_EH ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_MAP_DEBUG ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_BACKEND_MAP_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_BACKEND_MAP_IMPORTS ((TZrUInt32)1u << 4u)
#define ZR_EXECUTION_BACKEND_MAP_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_MAP_ROOTS | ZR_EXECUTION_BACKEND_MAP_EH | \
     ZR_EXECUTION_BACKEND_MAP_DEBUG | ZR_EXECUTION_BACKEND_MAP_DEOPT | \
     ZR_EXECUTION_BACKEND_MAP_IMPORTS)

#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_EXECBC_FALLBACK ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_AOT_FALLBACK ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_REQUIRE_MACHINE_CODE ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT | \
     ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_EXECBC_FALLBACK | \
     ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_AOT_FALLBACK | \
     ZR_EXECUTION_BACKEND_REQUEST_FLAG_REQUIRE_MACHINE_CODE)

#define ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_ASYNC ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_RUNTIME_ONLY ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_ASYNC | \
     ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_RUNTIME_ONLY)

typedef enum EZrExecutionBackendTargetKind {
    ZR_EXECUTION_BACKEND_TARGET_NONE = 0,
    ZR_EXECUTION_BACKEND_TARGET_EXECBC = 1,
    ZR_EXECUTION_BACKEND_TARGET_AOT = 2,
    ZR_EXECUTION_BACKEND_TARGET_HOST_JIT = 3,
    ZR_EXECUTION_BACKEND_TARGET_COUNT
} EZrExecutionBackendTargetKind;

typedef enum EZrExecutionBackendFallback {
    ZR_EXECUTION_BACKEND_FALLBACK_NONE = 0,
    ZR_EXECUTION_BACKEND_FALLBACK_EXECBC = 1,
    ZR_EXECUTION_BACKEND_FALLBACK_AOT = 2
} EZrExecutionBackendFallback;

typedef enum EZrExecutionBackendStatus {
    ZR_EXECUTION_BACKEND_STATUS_OK = 0,
    /* A queued or backend-pending operation is a normal non-blocking result. */
    ZR_EXECUTION_BACKEND_STATUS_PENDING,
    ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC,
    ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_MAGIC,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_SCHEMA,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS,
    ZR_EXECUTION_BACKEND_STATUS_CAPACITY,
    ZR_EXECUTION_BACKEND_STATUS_ALREADY_REGISTERED,
    ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED,
    ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE,
    ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET,
    ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION,
    ZR_EXECUTION_BACKEND_STATUS_IMMUTABLE_INPUT_REQUIRED,
    ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH,
    ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION,
    ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE,
    ZR_EXECUTION_BACKEND_STATUS_CANCELLED,
    ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED,
    ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID,
    ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND,
    ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_PUBLISHED,
    ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE,
    ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE,
    ZR_EXECUTION_BACKEND_STATUS_RETIRE_FAILED,
    ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN,
    ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT,
    ZR_EXECUTION_BACKEND_STATUS_RESUME_UNAVAILABLE,
    ZR_EXECUTION_BACKEND_STATUS_RESUME_FAILED,
    ZR_EXECUTION_BACKEND_STATUS_OVERFLOW,
    ZR_EXECUTION_BACKEND_STATUS_COUNT
} EZrExecutionBackendStatus;

typedef enum EZrExecutionBackendJobState {
    ZR_EXECUTION_BACKEND_JOB_FREE = 0,
    ZR_EXECUTION_BACKEND_JOB_QUEUED,
    ZR_EXECUTION_BACKEND_JOB_COMPILING,
    ZR_EXECUTION_BACKEND_JOB_READY,
    ZR_EXECUTION_BACKEND_JOB_FAILED,
    ZR_EXECUTION_BACKEND_JOB_CANCELLED,
    ZR_EXECUTION_BACKEND_JOB_PUBLISHED
} EZrExecutionBackendJobState;

typedef enum EZrExecutionBackendCodeState {
    ZR_EXECUTION_BACKEND_CODE_FREE = 0,
    ZR_EXECUTION_BACKEND_CODE_READY,
    ZR_EXECUTION_BACKEND_CODE_PUBLISHED,
    ZR_EXECUTION_BACKEND_CODE_RETIRED,
    ZR_EXECUTION_BACKEND_CODE_RECLAIMING,
    ZR_EXECUTION_BACKEND_CODE_RETIRE_FAILED
} EZrExecutionBackendCodeState;

typedef enum EZrExecutionBackendMapKind {
    ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS = 0,
    ZR_EXECUTION_BACKEND_MAP_KIND_EH,
    ZR_EXECUTION_BACKEND_MAP_KIND_DEBUG,
    ZR_EXECUTION_BACKEND_MAP_KIND_DEOPT,
    ZR_EXECUTION_BACKEND_MAP_KIND_IMPORTS,
    ZR_EXECUTION_BACKEND_MAP_KIND_COUNT
} EZrExecutionBackendMapKind;

typedef struct SZrExecutionGenerationKey {
    /* All four fields are required for an invalidation namespace. */
    TZrUInt64 domainIdentity;
    TZrUInt64 moduleIdentity;
    TZrUInt64 generation;
    TZrUInt64 backendRegistrationIdentity;
} SZrExecutionGenerationKey;

/* Kept opaque here so core does not need to include parser/ExecIR storage. */
struct SZrExecIrResumeRequest;

typedef struct SZrExecutionBackendTarget {
    TZrUInt32 schemaVersion;
    EZrExecutionBackendTargetKind kind;
    TZrUInt32 abiVersion;
    TZrUInt32 reserved;
    TZrUInt64 targetTripleHash;
    TZrUInt64 layoutHash;
    TZrUInt64 capabilityHash;
} SZrExecutionBackendTarget;

/* This diagnostic is scalar so it remains valid after a worker returns. */
typedef struct SZrExecutionBackendDiagnostic {
    EZrExecutionBackendStatus status;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt32 expectedState;
    TZrUInt32 actualState;
    TZrUInt64 ticketId;
    TZrUInt64 codeIdentity;
    TZrUInt64 expected;
    TZrUInt64 actual;
    SZrExecutionGenerationKey expectedKey;
    SZrExecutionGenerationKey actualKey;
} SZrExecutionBackendDiagnostic;

/* Immutable scalar input copied into a worker job. */
typedef struct SZrExecutionCompileRequest {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    EZrExecutionBackendTargetKind requestedTarget;
    SZrExecutionGenerationKey generationKey;
    SZrExecutionContract contract;
    TZrUInt64 immutableIrHash;
    TZrUInt64 compileInputHash;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt32 requiredOperations;
    TZrUInt32 requiredMapFlags;
} SZrExecutionCompileRequest;

typedef struct SZrExecutionCompileTicket {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt64 serviceIdentity;
    TZrUInt64 ticketId;
    SZrExecutionGenerationKey generationKey;
    EZrExecutionBackendTargetKind target;
    EZrExecutionBackendFallback fallback;
    EZrExecutionBackendJobState state;
} SZrExecutionCompileTicket;

typedef struct SZrExecutionBackendCompileInvocation {
    SZrExecutionCompileTicket ticket;
    SZrExecutionCompileRequest request;
} SZrExecutionBackendCompileInvocation;

/* Runtime-only code metadata. No executable address is stored here. */
typedef struct SZrExecutionBackendCodeInfo {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    SZrExecutionGenerationKey generationKey;
    SZrExecutionContract contract;
    TZrUInt64 immutableIrHash;
    TZrUInt64 compileInputHash;
    TZrUInt64 codeIdentity;
    TZrUInt32 codeSize;
    TZrUInt32 mapRegistrationFlags;
    TZrUInt64 rootMapHash;
    TZrUInt64 ehMapHash;
    TZrUInt64 debugMapHash;
    TZrUInt64 deoptMapHash;
    TZrUInt64 runtimeImportsHash;
} SZrExecutionBackendCodeInfo;

typedef SZrExecutionBackendCodeInfo SZrExecutionBackendCompiledCode;

typedef struct SZrExecutionCompileTicketView {
    TZrUInt64 ticketId;
    EZrExecutionBackendJobState state;
    EZrExecutionBackendTargetKind target;
    EZrExecutionBackendFallback fallback;
    EZrExecutionBackendStatus lastStatus;
    TZrUInt64 codeIdentity;
    SZrExecutionGenerationKey generationKey;
} SZrExecutionCompileTicketView;

typedef struct SZrExecutionCodeView {
    SZrExecutionBackendCodeInfo info;
    EZrExecutionBackendCodeState state;
    TZrUInt32 leaseCount;
    TZrUInt32 dependencyLeaseCount;
} SZrExecutionCodeView;

/* A lease is the only supported way to ask a backend for an entry address. */
typedef struct SZrExecutionCodeHandle {
    TZrUInt32 magic;
    TZrUInt32 slotIndex;
    TZrUInt64 serviceIdentity;
    TZrUInt64 codeIdentity;
    SZrExecutionGenerationKey generationKey;
    TZrBool leased;
    TZrBool dependencyLeased;
    TZrUInt16 reserved;
} SZrExecutionCodeHandle;

typedef struct SZrExecutionBackendRegistration {
    TZrUInt64 serviceIdentity;
    TZrUInt64 registrationIdentity;
    EZrExecutionBackendTargetKind backendKind;
} SZrExecutionBackendRegistration;

typedef struct SZrExecutionBackendService SZrExecutionBackendService;
typedef struct SZrExecutionBackendDescriptor SZrExecutionBackendDescriptor;

typedef EZrExecutionBackendStatus (*FZrExecutionBackendQueryTarget)(
        const SZrExecutionBackendTarget *target,
        TZrUInt32 requiredOperations,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef EZrExecutionBackendStatus (*FZrExecutionBackendCompileAsync)(
        const SZrExecutionBackendCompileInvocation *invocation,
        TZrPtr userData,
        SZrExecutionBackendCompiledCode *completedCode,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef EZrExecutionBackendStatus (*FZrExecutionBackendCancelCompile)(
        const SZrExecutionCompileTicket *ticket,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef EZrExecutionBackendStatus (*FZrExecutionBackendLookupEntry)(
        const SZrExecutionBackendCodeInfo *code,
        TZrNativePtr *entryAddress,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef EZrExecutionBackendStatus (*FZrExecutionBackendQueryMap)(
        const SZrExecutionBackendCodeInfo *code,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef EZrExecutionBackendStatus (*FZrExecutionBackendUnregisterMaps)(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef EZrExecutionBackendStatus (*FZrExecutionBackendRetire)(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
typedef void (*FZrExecutionBackendDestroy)(TZrPtr userData);

typedef struct SZrExecutionBackendVTable {
    FZrExecutionBackendQueryTarget queryTarget;
    FZrExecutionBackendCompileAsync compileAsync;
    FZrExecutionBackendCancelCompile cancelCompile;
    FZrExecutionBackendLookupEntry lookupEntry;
    FZrExecutionBackendQueryMap queryMap;
    FZrExecutionBackendUnregisterMaps unregisterMaps;
    FZrExecutionBackendRetire retire;
    FZrExecutionBackendDestroy destroy;
} SZrExecutionBackendVTable;

struct SZrExecutionBackendDescriptor {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrExecutionBackendTargetKind backendKind;
    TZrUInt32 flags;
    SZrExecutionBackendTarget target;
    TZrUInt32 supportedOperations;
    TZrUInt32 reserved;
    SZrExecutionBackendVTable vtable;
    TZrPtr userData;
};

/* Caller-owned arrays make capacity/OOM behaviour deterministic and avoid
 * hidden allocations in the frame-thread path. These pointers are runtime
 * state and are never copied into persistent contracts. */
typedef struct SZrExecutionBackendRegistrationRecord {
    TZrUInt64 registrationIdentity;
    TZrBool active;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    SZrExecutionBackendDescriptor descriptor;
} SZrExecutionBackendRegistrationRecord;

typedef struct SZrExecutionCompileJob {
    SZrExecutionCompileTicket ticket;
    SZrExecutionCompileRequest request;
    SZrExecutionBackendCompiledCode result;
    EZrExecutionBackendStatus lastStatus;
    TZrUInt32 registrationSlot;
    TZrUInt32 codeSlot;
    TZrBool cancelRequested;
    TZrBool resultValid;
    TZrUInt16 reserved;
} SZrExecutionCompileJob;

typedef struct SZrExecutionCodeRecord {
    SZrExecutionBackendCodeInfo info;
    EZrExecutionBackendCodeState state;
    TZrUInt32 registrationSlot;
    TZrUInt32 jobSlot;
    TZrUInt32 leaseCount;
    TZrUInt32 dependencyLeaseCount;
    TZrBool mapsRegistered;
    TZrBool reserved0;
    TZrUInt16 reserved1;
} SZrExecutionCodeRecord;

typedef TZrBool (*FZrExecutionBackendResumeInterpreter)(
        const struct SZrExecIrResumeRequest *request,
        TZrPtr userData,
        SZrExecIrDiagnostic *diagnostic);

struct SZrExecutionBackendService {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    volatile TZrUInt32 lock;
    TZrUInt64 serviceIdentity;
    TZrUInt64 nextRegistrationIdentity;
    TZrUInt64 nextTicketId;
    SZrExecutionBackendRegistrationRecord *registrations;
    TZrUInt32 registrationCapacity;
    TZrUInt32 registrationCount;
    SZrExecutionCompileJob *jobs;
    TZrUInt32 jobCapacity;
    TZrUInt32 jobCount;
    SZrExecutionCodeRecord *codes;
    TZrUInt32 codeCapacity;
    TZrUInt32 codeCount;
    FZrExecutionBackendResumeInterpreter resumeInterpreter;
    TZrPtr resumeUserData;
    TZrBool shuttingDown;
    TZrBool destroyed;
    TZrBool shutdownDestroyCalled;
    TZrUInt8 reserved;
    /* queryTarget runs outside the lock but still pins its registration. */
    volatile TZrUInt32 queryTargetInFlight;
    /* Every callback that owns a copied descriptor/userData pair is counted;
     * shutdown/finalization cannot destroy a registration while it is live. */
    volatile TZrUInt32 backendCallbackInFlight;
    /* Tombstones make InvalidateGeneration a prevention boundary, not only a
     * best-effort sweep of records that happened to exist at call time. */
    SZrExecutionGenerationKey invalidatedKeys[ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY];
    TZrUInt32 invalidatedKeyCount;
    TZrUInt32 reservedInvalidation;
};

/* Diagnostics and scalar key helpers. */
ZR_CORE_API void ZrCore_ExecutionBackend_DiagnosticClear(
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_GenerationKeyIsValid(
        const SZrExecutionGenerationKey *key);
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_GenerationKeyEqual(
        const SZrExecutionGenerationKey *left,
        const SZrExecutionGenerationKey *right);
ZR_CORE_API const TZrChar *ZrCore_ExecutionBackend_StatusName(
        EZrExecutionBackendStatus status);

/* Explicit service instance API. */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Init(
        SZrExecutionBackendService *service,
        TZrUInt64 serviceIdentity,
        SZrExecutionBackendRegistrationRecord *registrations,
        TZrUInt32 registrationCapacity,
        SZrExecutionCompileJob *jobs,
        TZrUInt32 jobCapacity,
        SZrExecutionCodeRecord *codes,
        TZrUInt32 codeCapacity,
        FZrExecutionBackendResumeInterpreter resumeInterpreter,
        TZrPtr resumeUserData,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_ExecutionBackendService_Deinit(
        SZrExecutionBackendService *service);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Register(
        SZrExecutionBackendService *service,
        const SZrExecutionBackendDescriptor *descriptor,
        SZrExecutionBackendRegistration *registration,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Unregister(
        SZrExecutionBackendService *service,
        TZrUInt64 registrationIdentity,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_CompileAsync(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ProcessNext(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Complete(
        SZrExecutionBackendService *service,
        SZrExecutionCompileTicket *ticket,
        const SZrExecutionBackendCompiledCode *code,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Publish(
        SZrExecutionBackendService *service,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryTicket(
        const SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionCompileTicketView *view,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Cancel(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_InvalidateGeneration(
        SZrExecutionBackendService *service,
        const SZrExecutionGenerationKey *key,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_AcquireCode(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_AcquireDependencyLease(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ReleaseDependencyLease(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ReleaseCode(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryCode(
        const SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        SZrExecutionCodeView *view,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_LookupEntry(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryMap(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_CollectRetired(
        SZrExecutionBackendService *service,
        TZrUInt32 *outCollected,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Shutdown(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_FinalizeShutdown(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecutionBackendService_ResumeInterpreter(
        SZrExecutionBackendService *service,
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic);

/* Optional process-local default facade matching the plan's short API names. */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_SetDefaultService(
        SZrExecutionBackendService *service);
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_Register(
        const SZrExecutionBackendDescriptor *backend);
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_CompileAsync(
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket);
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_InvalidateGeneration(
        const SZrExecutionGenerationKey *key);
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_ResumeInterpreter(
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_EXECUTION_BACKEND_H */
