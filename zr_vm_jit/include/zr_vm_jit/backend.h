#ifndef ZR_VM_JIT_BACKEND_H
#define ZR_VM_JIT_BACKEND_H

/*
 * Optional host baseline-JIT facade.
 *
 * The implementation is C++ so that an LLVM ORC/JITLink provider can be
 * plugged in without making the core C11 target depend on LLVM.  Everything
 * crossing this header is a value contract or a process-local opaque handle:
 * executable addresses are never persisted in an artifact, a binding row, or
 * a cache key.  A build without an ORC provider remains usable and reports an
 * explicit BACKEND_UNAVAILABLE/fallback result.
 */

#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_core/execution_backend.h"
#include "zr_vm_core/host_baseline_jit.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_JIT_HOST_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_JIT_HOST_MAGIC ((TZrUInt32)0x314a485au) /* ZHJ1 */

#define ZR_JIT_HOST_OPTION_ALLOW_FALLBACK ((TZrUInt32)1u << 0u)
#define ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE ((TZrUInt32)1u << 1u)
#define ZR_JIT_HOST_OPTION_KNOWN_MASK \
    (ZR_JIT_HOST_OPTION_ALLOW_FALLBACK | ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE)

#define ZR_JIT_HOST_MAP_ROOTS ((TZrUInt32)1u << 0u)
#define ZR_JIT_HOST_MAP_UNWIND ((TZrUInt32)1u << 1u)
#define ZR_JIT_HOST_MAP_DEBUG ((TZrUInt32)1u << 2u)
#define ZR_JIT_HOST_MAP_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_JIT_HOST_MAP_KNOWN_MASK \
    (ZR_JIT_HOST_MAP_ROOTS | ZR_JIT_HOST_MAP_UNWIND | \
     ZR_JIT_HOST_MAP_DEBUG | ZR_JIT_HOST_MAP_DEOPT)

typedef enum EZrJitHostStatus {
    ZR_JIT_HOST_STATUS_OK = 0,
    ZR_JIT_HOST_STATUS_PENDING,
    ZR_JIT_HOST_STATUS_DISABLED,
    ZR_JIT_HOST_STATUS_ALREADY_REGISTERED,
    ZR_JIT_HOST_STATUS_NOT_REGISTERED,
    ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE,
    ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
    ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH,
    ZR_JIT_HOST_STATUS_INVALID_FLAGS,
    ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED,
    ZR_JIT_HOST_STATUS_TARGET_MISMATCH,
    ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN,
    ZR_JIT_HOST_STATUS_IMPORT_INVALID,
    ZR_JIT_HOST_STATUS_OPERATION_UNSUPPORTED,
    ZR_JIT_HOST_STATUS_STATE_MAP_INVALID,
    ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
    ZR_JIT_HOST_STATUS_WX_REQUIRED,
    ZR_JIT_HOST_STATUS_CODE_INVALID,
    ZR_JIT_HOST_STATUS_CAPACITY,
    ZR_JIT_HOST_STATUS_STALE_HANDLE,
    ZR_JIT_HOST_STATUS_ACTIVE_LEASE,
    ZR_JIT_HOST_STATUS_INVALID_STATE,
    ZR_JIT_HOST_STATUS_OVERFLOW,
    ZR_JIT_HOST_STATUS_FALLBACK_EXECBC,
    ZR_JIT_HOST_STATUS_FALLBACK_AOT,
    ZR_JIT_HOST_STATUS_COUNT
} EZrJitHostStatus;

/* Short aliases make the optional facade easy to consume from adapters that
 * already use the plan's EZrJitStatus spelling. */
typedef EZrJitHostStatus EZrJitStatus;
#define ZR_JIT_STATUS_OK ZR_JIT_HOST_STATUS_OK
#define ZR_JIT_STATUS_BACKEND_UNAVAILABLE ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE
#define ZR_JIT_STATUS_INVALID_ARGUMENT ZR_JIT_HOST_STATUS_INVALID_ARGUMENT
#define ZR_JIT_STATUS_TARGET_UNSUPPORTED ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED
#define ZR_JIT_STATUS_TARGET_MISMATCH ZR_JIT_HOST_STATUS_TARGET_MISMATCH
#define ZR_JIT_STATUS_IMPORT_FORBIDDEN ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN
#define ZR_JIT_STATUS_STATE_MAP_INVALID ZR_JIT_HOST_STATUS_STATE_MAP_INVALID

typedef enum EZrJitHostState {
    ZR_JIT_HOST_STATE_UNINITIALIZED = 0,
    ZR_JIT_HOST_STATE_REGISTERED,
    ZR_JIT_HOST_STATE_SHUTTING_DOWN,
    ZR_JIT_HOST_STATE_SHUTDOWN
} EZrJitHostState;

typedef struct SZrJitHostDiagnostic {
    EZrJitHostStatus status;
    EZrHostJitStatus coreStatus;
    TZrUInt32 sourceIndex;
    TZrUInt32 operation;
    TZrUInt32 expected;
    TZrUInt32 actual;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrJitHostDiagnostic;

/* A pointer-free witness for the four registrations required before an entry
 * can be published.  The counts are deliberately explicit: a non-zero hash
 * alone must not be accepted as proof that a registration happened. */
typedef struct SZrJitStateMapFacts {
    TZrUInt32 schemaVersion;
    TZrUInt32 registrationFlags;
    TZrUInt32 rootEntryCount;
    TZrUInt32 unwindEntryCount;
    TZrUInt32 debugEntryCount;
    TZrUInt32 deoptEntryCount;
    TZrUInt64 rootMapHash;
    TZrUInt64 unwindMapHash;
    TZrUInt64 debugMapHash;
    TZrUInt64 deoptMapHash;
    TZrUInt64 frameLayoutHash;
} SZrJitStateMapFacts;

typedef struct SZrJitHostCompileRequest {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 operationMask;
    SZrHostJitPublicationFacts publication;
    SZrJitStateMapFacts stateMaps;
    TZrUInt32 sourceIndex;
    TZrUInt32 reserved;
} SZrJitHostCompileRequest;

typedef struct SZrJitHostBackendInfo {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrJitHostState state;
    TZrBool machineCodeAvailable;
    TZrBool fallbackAvailable;
    TZrUInt16 reserved;
    SZrHostJitTargetContract target;
    TZrUInt32 supportedOperations;
    TZrUInt32 codeRecordCapacity;
    TZrUInt64 backendIdentity;
} SZrJitHostBackendInfo;

ZR_API void ZrJit_Host_DiagnosticInit(SZrJitHostDiagnostic *diagnostic);
ZR_API const TZrChar *ZrJit_Host_StatusName(EZrJitHostStatus status);
ZR_API EZrJitHostStatus ZrJit_Host_ValidateStateMaps(
        const SZrJitStateMapFacts *facts,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_ValidateCompileRequest(
        const SZrJitHostCompileRequest *request,
        SZrJitHostDiagnostic *diagnostic);

/* The short registration API from the host-baseline plan.  It only registers
 * a process-local adapter; it does not claim that LLVM/ORC is installed. */
ZR_API TZrBool ZrJit_Host_Register(const SZrHostJitOptions *options,
        SZrExecIrDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_TryShutdown(
        SZrJitHostDiagnostic *diagnostic);
ZR_API void ZrJit_Host_Shutdown(void);
ZR_API TZrBool ZrJit_Host_IsRegistered(void);
ZR_API TZrBool ZrJit_Host_IsMachineCodeAvailable(void);
ZR_API EZrJitHostStatus ZrJit_Host_QueryInfo(
        SZrJitHostBackendInfo *info,
        SZrJitHostDiagnostic *diagnostic);

/* The descriptor is runtime-only and can be copied into the core backend
 * service.  Its callbacks never search arbitrary host symbols. */
ZR_API TZrBool ZrJit_Host_GetDescriptor(
        SZrExecutionBackendDescriptor *descriptor,
        SZrJitHostDiagnostic *diagnostic);

ZR_API EZrJitHostStatus ZrJit_Host_ValidateImport(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrJitHostDiagnostic *diagnostic);

/* Contract/lifetime operations delegate to the core-owned checked manager.
 * They are available even when machine-code generation is unavailable so a
 * caller can test publication and lease ordering without a fake entry. */
ZR_API EZrJitHostStatus ZrJit_Host_Prepare(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_PrepareWithMaps(
        const SZrHostJitPublicationFacts *facts,
        const SZrJitStateMapFacts *stateMaps,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_Publish(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_Acquire(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_Release(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_Evict(
        TZrUInt64 codeIdentity,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_Collect(
        TZrUInt32 *outCollected,
        SZrJitHostDiagnostic *diagnostic);
ZR_API EZrJitHostStatus ZrJit_Host_LookupEntry(
        const SZrHostJitCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrJitHostDiagnostic *diagnostic);

/* Compile is intentionally explicit about an unavailable ORC provider.  It
 * never manufactures a function pointer or installs a partial code record. */
ZR_API EZrJitHostStatus ZrJit_Host_Compile(
        const SZrJitHostCompileRequest *request,
        SZrHostJitCodeHandle *outHandle,
        SZrJitHostDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_JIT_BACKEND_H */
