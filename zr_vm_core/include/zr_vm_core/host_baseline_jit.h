#ifndef ZR_VM_CORE_HOST_BASELINE_JIT_H
#define ZR_VM_CORE_HOST_BASELINE_JIT_H

/*
 * Host baseline JIT boundary.
 *
 * This is a pointer-free validation/lifetime contract.  An optional ORC or
 * JITLink adapter may keep its process-local executable address privately,
 * but the address never enters this contract, an artifact, or a persistent
 * binding.  Keeping this layer in core lets a build without LLVM retain the
 * ordinary AOT/ExecBC route.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)

typedef enum EZrHostJitArchitecture {
    ZR_HOST_JIT_ARCH_NONE = 0,
    ZR_HOST_JIT_ARCH_X86_64 = 1,
    ZR_HOST_JIT_ARCH_AARCH64 = 2
} EZrHostJitArchitecture;

typedef enum EZrHostJitPlatform {
    ZR_HOST_JIT_PLATFORM_NONE = 0,
    ZR_HOST_JIT_PLATFORM_HOST = 1,
    ZR_HOST_JIT_PLATFORM_ANDROID = 2,
    ZR_HOST_JIT_PLATFORM_IOS = 3,
    ZR_HOST_JIT_PLATFORM_WASM = 4
} EZrHostJitPlatform;

typedef enum EZrHostJitEndian {
    ZR_HOST_JIT_ENDIAN_NONE = 0,
    ZR_HOST_JIT_ENDIAN_LITTLE = 1,
    ZR_HOST_JIT_ENDIAN_BIG = 2
} EZrHostJitEndian;

typedef enum EZrHostJitStatus {
    ZR_HOST_JIT_STATUS_OK = 0,
    ZR_HOST_JIT_STATUS_INVALID_ARGUMENT,
    ZR_HOST_JIT_STATUS_SCHEMA_MISMATCH,
    ZR_HOST_JIT_STATUS_TARGET_UNSUPPORTED,
    ZR_HOST_JIT_STATUS_TARGET_MISMATCH,
    ZR_HOST_JIT_STATUS_ABI_MISMATCH,
    ZR_HOST_JIT_STATUS_LAYOUT_MISMATCH,
    ZR_HOST_JIT_STATUS_INVALID_FLAGS,
    ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN,
    ZR_HOST_JIT_STATUS_IMPORT_INVALID,
    ZR_HOST_JIT_STATUS_OPERATION_UNSUPPORTED,
    ZR_HOST_JIT_STATUS_REGISTRATION_INCOMPLETE,
    ZR_HOST_JIT_STATUS_WX_REQUIRED,
    ZR_HOST_JIT_STATUS_CODE_INVALID,
    ZR_HOST_JIT_STATUS_CAPACITY,
    ZR_HOST_JIT_STATUS_NOT_PREPARED,
    ZR_HOST_JIT_STATUS_STALE_HANDLE,
    ZR_HOST_JIT_STATUS_ACTIVE_LEASE,
    ZR_HOST_JIT_STATUS_INVALID_STATE,
    ZR_HOST_JIT_STATUS_OVERFLOW
} EZrHostJitStatus;

typedef struct SZrHostJitDiagnostic {
    EZrHostJitStatus status;
    TZrUInt32 sourceIndex;
    TZrUInt32 expected;
    TZrUInt32 actual;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrHostJitDiagnostic;

typedef struct SZrHostJitTargetContract {
    TZrUInt32 schemaVersion;
    EZrHostJitArchitecture architecture;
    EZrHostJitPlatform platform;
    TZrUInt32 pointerSize;
    EZrHostJitEndian endianness;
    TZrUInt32 abiVersion;
    TZrUInt64 targetTripleHash;
    TZrUInt64 layoutHash;
} SZrHostJitTargetContract;

#define ZR_HOST_JIT_OPTION_FLAG_ENABLE ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_OPTION_FLAG_KNOWN_MASK \
    (ZR_HOST_JIT_OPTION_FLAG_ENABLE | ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK)

typedef struct SZrHostJitOptions {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    SZrHostJitTargetContract target;
    TZrUInt32 codeCacheBytes;
    TZrUInt32 maxImports;
} SZrHostJitOptions;

typedef enum EZrHostJitImportKind {
    ZR_HOST_JIT_IMPORT_NONE = 0,
    ZR_HOST_JIT_IMPORT_RUNTIME = 1,
    ZR_HOST_JIT_IMPORT_NATIVE = 2
} EZrHostJitImportKind;

typedef struct SZrHostJitImport {
    TZrUInt64 symbolId;
    TZrUInt64 signatureHash;
    EZrHostJitImportKind kind;
    TZrUInt32 reserved;
} SZrHostJitImport;

typedef struct SZrHostJitImportManifest {
    TZrUInt32 schemaVersion;
    TZrUInt32 count;
    const SZrHostJitImport *imports;
    TZrUInt64 allowedSymbolHash;
} SZrHostJitImportManifest;

#define ZR_HOST_JIT_OPERATION_TYPED_SCALAR ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_OPERATION_CONTROL ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_OPERATION_DIRECT_CALL ((TZrUInt32)1u << 2u)
#define ZR_HOST_JIT_OPERATION_SIMPLE_MEMBER ((TZrUInt32)1u << 3u)
#define ZR_HOST_JIT_OPERATION_ARRAY ((TZrUInt32)1u << 4u)
#define ZR_HOST_JIT_OPERATION_KNOWN_MASK \
    (ZR_HOST_JIT_OPERATION_TYPED_SCALAR | ZR_HOST_JIT_OPERATION_CONTROL | \
     ZR_HOST_JIT_OPERATION_DIRECT_CALL | ZR_HOST_JIT_OPERATION_SIMPLE_MEMBER | \
     ZR_HOST_JIT_OPERATION_ARRAY)

#define ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_PUBLICATION_FLAG_WX ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK \
    (ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE | ZR_HOST_JIT_PUBLICATION_FLAG_WX)

#define ZR_HOST_JIT_REGISTRATION_ROOTS ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_REGISTRATION_UNWIND ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_REGISTRATION_DEBUG ((TZrUInt32)1u << 2u)
#define ZR_HOST_JIT_REGISTRATION_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_HOST_JIT_REGISTRATION_KNOWN_MASK \
    (ZR_HOST_JIT_REGISTRATION_ROOTS | ZR_HOST_JIT_REGISTRATION_UNWIND | \
     ZR_HOST_JIT_REGISTRATION_DEBUG | ZR_HOST_JIT_REGISTRATION_DEOPT)

typedef struct SZrHostJitPublicationFacts {
    SZrHostJitTargetContract target;
    TZrUInt32 flags;
    TZrUInt32 registrationFlags;
    TZrUInt32 operationMask;
    TZrUInt32 reserved;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 abiHash;
    TZrUInt32 requiredCapabilities;
    TZrUInt32 declaredEffects;
    TZrUInt64 gcMapHash;
    TZrUInt64 unwindMapHash;
    TZrUInt64 debugMapHash;
    TZrUInt64 deoptMapHash;
    TZrUInt32 codeSize;
    TZrUInt64 codeIdentity;
    const SZrHostJitImportManifest *imports;
} SZrHostJitPublicationFacts;

typedef enum EZrHostJitCodeState {
    ZR_HOST_JIT_CODE_FREE = 0,
    ZR_HOST_JIT_CODE_PREPARED,
    ZR_HOST_JIT_CODE_PUBLISHED,
    ZR_HOST_JIT_CODE_RETIRED
} EZrHostJitCodeState;

typedef struct SZrHostJitCodeRecord {
    TZrUInt64 codeIdentity;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 abiHash;
    TZrUInt32 state;
    volatile TZrUInt32 leaseCount;
} SZrHostJitCodeRecord;

typedef struct SZrHostJitCodeManager {
    volatile TZrUInt32 lock;
    SZrHostJitCodeRecord *active;
    SZrHostJitCodeRecord *records;
    TZrUInt32 capacity;
    TZrUInt32 count;
} SZrHostJitCodeManager;

typedef struct SZrHostJitCodeHandle {
    SZrHostJitCodeRecord *record;
    TZrUInt64 codeIdentity;
    TZrBool leased;
} SZrHostJitCodeHandle;

typedef struct SZrHostJitCodeView {
    TZrUInt64 codeIdentity;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 abiHash;
    EZrHostJitCodeState state;
    TZrUInt32 leaseCount;
} SZrHostJitCodeView;

ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidateTarget(
        const SZrHostJitTargetContract *target,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidateOptions(
        const SZrHostJitOptions *options,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidateImports(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidatePublication(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_HostJit_SupportsOperation(
        TZrUInt32 operation,
        TZrUInt32 operationMask);

ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_CodeManager_Init(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeRecord *records,
        TZrUInt32 capacity,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_HostJit_CodeManager_Deinit(
        SZrHostJitCodeManager *manager);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Prepare(
        SZrHostJitCodeManager *manager,
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *outHandle,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Publish(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *prepared,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_AcquireActive(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *outHandle,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Evict(
        SZrHostJitCodeManager *manager,
        TZrUInt64 codeIdentity,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Resolve(
        const SZrHostJitCodeManager *manager,
        const SZrHostJitCodeHandle *handle,
        SZrHostJitCodeView *outView,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Release(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *handle,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_CollectRetired(
        SZrHostJitCodeManager *manager,
        TZrUInt32 *outCollected,
        SZrHostJitDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_HostJit_StatusName(EZrHostJitStatus status);

#ifdef __cplusplus
}
#endif

#endif
