#ifndef ZR_VM_CORE_NATIVE_CALL_CONTRACT_H
#define ZR_VM_CORE_NATIVE_CALL_CONTRACT_H

/*
 * Runtime half of the native-call contract.
 *
 * The structures named *Plan in this API are deliberately made only from
 * fixed-width scalar fields and fixed-size arrays.  They are suitable for a
 * cache/artifact record and never contain a function pointer, managed
 * pointer, or an address into a moving heap.  Pointers occur only in the
 * short-lived lease and marshal request structures below.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_common/zr_ffi_contract.h"

struct SZrState;
typedef struct SZrState SZrState;
struct SZrGlobalState;
struct SZrRawObject;
struct SZrTypeValue;

#define ZR_NATIVE_CALL_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_NATIVE_CALL_MAX_PARAMETERS ZR_FFI_CONTRACT_MAX_PARAMETERS
#define ZR_NATIVE_CALL_MAX_ROOTS ((TZrUInt32)64u)
#define ZR_NATIVE_CALL_MAX_PINNED_VALUES ((TZrUInt32)64u)
#define ZR_NATIVE_CALL_PLAN_MAGIC ((TZrUInt32)0x314c504eu) /* NPL1 */

typedef enum EZrNativeCallStatus {
    ZR_NATIVE_CALL_STATUS_OK = 0,
    ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
    ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
    ZR_NATIVE_CALL_STATUS_INVALID_PLAN,
    ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
    ZR_NATIVE_CALL_STATUS_SIGNATURE_MISMATCH,
    ZR_NATIVE_CALL_STATUS_LAYOUT_MISMATCH,
    ZR_NATIVE_CALL_STATUS_PARAMETER_LIMIT,
    ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE,
    ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
    ZR_NATIVE_CALL_STATUS_ALIGNMENT,
    ZR_NATIVE_CALL_STATUS_BUFFER_TOO_SMALL,
    ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT,
    ZR_NATIVE_CALL_STATUS_WRITEBACK_REQUIRED,
    ZR_NATIVE_CALL_STATUS_ATTACH_REQUIRED,
    ZR_NATIVE_CALL_STATUS_ATTACH_FAILED,
    ZR_NATIVE_CALL_STATUS_THREAD_POLICY,
    ZR_NATIVE_CALL_STATUS_NATIVE_ENTER_FAILED,
    ZR_NATIVE_CALL_STATUS_PIN_FAILED,
    ZR_NATIVE_CALL_STATUS_ROOT_FAILED,
    ZR_NATIVE_CALL_STATUS_NOT_ACTIVE,
    ZR_NATIVE_CALL_STATUS_ALREADY_ACTIVE,
    ZR_NATIVE_CALL_STATUS_CALLBACK_UNRESOLVED,
    ZR_NATIVE_CALL_STATUS_CALLBACK_REJECTED,
    ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
    ZR_NATIVE_CALL_STATUS_CALLBACK_NOT_REGISTERED,
    ZR_NATIVE_CALL_STATUS_CALLBACK_ALREADY_CLOSING,
    ZR_NATIVE_CALL_STATUS_EXCEPTION_TRANSLATION,
    ZR_NATIVE_CALL_STATUS_INVOKE_UNRESOLVED,
    ZR_NATIVE_CALL_STATUS_INVOKE_FAILED,
    ZR_NATIVE_CALL_STATUS_BUDGET,
    ZR_NATIVE_CALL_STATUS_INTERNAL
} EZrNativeCallStatus;

typedef enum EZrNativeCallPhase {
    ZR_NATIVE_CALL_PHASE_NONE = 0,
    ZR_NATIVE_CALL_PHASE_VALIDATE,
    ZR_NATIVE_CALL_PHASE_ATTACH,
    ZR_NATIVE_CALL_PHASE_PUBLISH_STATE,
    ZR_NATIVE_CALL_PHASE_PIN,
    ZR_NATIVE_CALL_PHASE_ROOT,
    ZR_NATIVE_CALL_PHASE_INVOKE,
    ZR_NATIVE_CALL_PHASE_TRANSLATE_EXCEPTION,
    ZR_NATIVE_CALL_PHASE_WRITEBACK,
    ZR_NATIVE_CALL_PHASE_CLEANUP
} EZrNativeCallPhase;

/* AUTO is an input-only override.  Plans contain a concrete class. */
typedef enum EZrNativeCallAbiClass {
    ZR_NATIVE_CALL_ABI_CLASS_AUTO = 0,
    ZR_NATIVE_CALL_ABI_CLASS_SCALAR,
    ZR_NATIVE_CALL_ABI_CLASS_INLINE_STRUCT,
    ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW,
    ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY,
    ZR_NATIVE_CALL_ABI_CLASS_REF_OUT,
    ZR_NATIVE_CALL_ABI_CLASS_VARARGS,
    ZR_NATIVE_CALL_ABI_CLASS_CALLBACK,
    ZR_NATIVE_CALL_ABI_CLASS_UNSUPPORTED
} EZrNativeCallAbiClass;

typedef enum EZrNativeCallMarshalKind {
    ZR_NATIVE_CALL_MARSHAL_DIRECT = 0,
    ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT,
    ZR_NATIVE_CALL_MARSHAL_COPY,
    ZR_NATIVE_CALL_MARSHAL_REGISTERED,
    ZR_NATIVE_CALL_MARSHAL_BRIDGE,
    ZR_NATIVE_CALL_MARSHAL_REJECT
} EZrNativeCallMarshalKind;

#define ZR_NATIVE_CALL_PLAN_FLAG_NONE ((TZrUInt32)0u)
#define ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE ((TZrUInt32)1u << 0u)
#define ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH ((TZrUInt32)1u << 1u)
#define ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN ((TZrUInt32)1u << 2u)
#define ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT ((TZrUInt32)1u << 3u)
#define ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK ((TZrUInt32)1u << 4u)
#define ZR_NATIVE_CALL_PLAN_FLAG_HAS_REF_OUT ((TZrUInt32)1u << 5u)
#define ZR_NATIVE_CALL_PLAN_FLAG_HAS_VARARGS ((TZrUInt32)1u << 6u)
#define ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_WRITEBACK ((TZrUInt32)1u << 7u)
#define ZR_NATIVE_CALL_PLAN_FLAG_CALLBACK_RELOAD ((TZrUInt32)1u << 8u)
#define ZR_NATIVE_CALL_PLAN_FLAG_KNOWN_MASK \
    (ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE | \
     ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH | \
     ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN | \
     ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT | \
     ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK | \
     ZR_NATIVE_CALL_PLAN_FLAG_HAS_REF_OUT | \
     ZR_NATIVE_CALL_PLAN_FLAG_HAS_VARARGS | \
     ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_WRITEBACK | \
     ZR_NATIVE_CALL_PLAN_FLAG_CALLBACK_RELOAD)

typedef struct SZrNativeCallDiagnostic {
    EZrNativeCallStatus status;
    EZrNativeCallPhase phase;
    TZrUInt32 parameterIndex;
    TZrUInt32 operationIndex;
    /* Import symbol/module identities are 64-bit hashes in the canonical
     * contract.  Keep the complete value here so two distinct imports cannot
     * collapse to the same diagnostic location at the native boundary. */
    TZrUInt64 sourceId;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrNativeCallDiagnostic;

typedef struct SZrNativeCallMarshalOp {
    EZrNativeCallAbiClass abiClass;
    EZrNativeCallMarshalKind marshalKind;
    EZrFfiDirection direction;
    EZrFfiParameterOwnership ownership;
    TZrUInt32 parameterIndex;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlignment;
    TZrUInt32 flags;
    TZrUInt64 canonicalTypeHash;
    TZrUInt64 layoutHash;
} SZrNativeCallMarshalOp;

/* Per-argument obligations recorded in the pointer-free plan.  Consumers
 * must satisfy these obligations through a live lease before invoking a
 * native target; they are intentionally separate from the marshal kind so a
 * backend cannot accidentally infer lifetime from a copy/direct decision. */
#define ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN ((TZrUInt32)1u << 0u)
#define ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT ((TZrUInt32)1u << 1u)
#define ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK ((TZrUInt32)1u << 2u)
#define ZR_NATIVE_CALL_MARSHAL_FLAG_NULLABLE ((TZrUInt32)1u << 3u)
#define ZR_NATIVE_CALL_MARSHAL_FLAG_KNOWN_MASK \
    (ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN | \
     ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT | \
     ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK | \
     ZR_NATIVE_CALL_MARSHAL_FLAG_NULLABLE)

/*
 * This is the cacheable representation.  Keep this type pointer-free:
 * callbackId is an identity used to resolve a process-local address at
 * invocation time, never the address itself.
 */
typedef struct SZrNativeCallPlan {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 parameterCount;
    TZrUInt32 marshalOpCount;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    /* Fixed-width source identity copied from the import contract.  Keeping
     * it in the cacheable plan lets marshal/lease failures retain the same
     * diagnostic location after Prepare has returned. */
    TZrUInt64 sourceId;
    /* Capability requirements are retained so a cached plan cannot silently
     * bypass the provider's capability gate on a later invocation. */
    TZrUInt64 requiredCapabilities;
    TZrUInt64 callbackId;
    TZrUInt64 targetAbiHash;
    /* Calling convention and charset are part of the resolved ABI witness;
     * keeping them in the pointer-free plan lets C/libffi/AOT consumers
     * select the same frame and string conversion rules without consulting
     * the original contract again. */
    EZrFfiAbi abi;
    EZrFfiCharset charset;
    TZrUInt32 targetPointerSize;
    EZrFfiTargetEndianness targetEndianness;
    EZrFfiErrorPolicy exceptionPolicy;
    EZrFfiCleanupPolicy cleanupPolicy;
    EZrFfiCallbackLifetime callbackLifetime;
    EZrFfiCallbackThreadPolicy callbackThreadPolicy;
    EZrFfiCallbackExceptionPolicy callbackExceptionPolicy;
    EZrGcNativeSafepointMode nativeSafepointMode;
    TZrUInt32 flags;
    TZrUInt32 pinCount;
    TZrUInt32 rootCount;
    TZrBool isVariadic;
    TZrBool directCompatible;
    TZrBool requiresBridge;
    TZrBool valid;
    TZrUInt8 reserved0;
    TZrUInt64 planHash;
    SZrNativeCallMarshalOp marshalOps[ZR_NATIVE_CALL_MAX_PARAMETERS];
} SZrNativeCallPlan;

/* Request-side pointers are borrowed for Prepare and never copied to Plan. */
typedef struct SZrNativeCallRequest {
    const SZrNativeImportContract *contract;
    TZrUInt64 expectedSignatureHash;
    TZrUInt64 expectedLayoutHash;
    TZrUInt64 callbackId;
    TZrUInt32 flags;
    TZrUInt32 targetPointerSize;
    EZrFfiTargetEndianness targetEndianness;
    TZrUInt64 targetAbiHash;
    EZrNativeCallAbiClass parameterClasses[ZR_NATIVE_CALL_MAX_PARAMETERS];
    TZrBool nativeAddressStable;
    TZrBool allowDirect;
    TZrBool hasTargetEnvironment;
    TZrBool reserved0;
} SZrNativeCallRequest;

#define ZR_NATIVE_CALL_REQUEST_FLAG_DISALLOW_DIRECT ((TZrUInt32)1u << 0u)
#define ZR_NATIVE_CALL_REQUEST_FLAG_BLOCKING_DETACHED ((TZrUInt32)1u << 1u)
#define ZR_NATIVE_CALL_REQUEST_FLAG_NO_SAFEPOINT_CRITICAL ((TZrUInt32)1u << 2u)
#define ZR_NATIVE_CALL_REQUEST_FLAG_KNOWN_MASK \
    (ZR_NATIVE_CALL_REQUEST_FLAG_DISALLOW_DIRECT | \
     ZR_NATIVE_CALL_REQUEST_FLAG_BLOCKING_DETACHED | \
     ZR_NATIVE_CALL_REQUEST_FLAG_NO_SAFEPOINT_CRITICAL)

typedef struct SZrNativeCallMarshalRequest {
    const void *source;
    TZrSize sourceSize;
    void *destination;
    TZrSize destinationCapacity;
    /* For REF/OUT this records that the caller will perform the explicit
     * write-back step.  Marshalling itself remains usable with false so a
     * bridge may defer write-back until its native call has completed; the
     * result's requiresWriteback bit is authoritative. */
    TZrBool writeBack;
    TZrBool sourceInitialized;
    TZrUInt16 reserved0;
} SZrNativeCallMarshalRequest;

typedef struct SZrNativeCallMarshalResult {
    void *pointer;
    TZrSize byteSize;
    TZrBool usedTemporary;
    TZrBool requiresWriteback;
} SZrNativeCallMarshalResult;

/* Ephemeral state used only while a call is active.  Callers must zero
 * initialize this record before the first LeaseBegin (or reuse it only after
 * LeaseEnd); the active bit is intentionally what prevents a second begin
 * from orphaning resources.  Invokers must call the pin/root API once for
 * each required operation, including optional or non-GC values; the
 * obligation counters make those successful no-op acknowledgements explicit
 * without inventing a GC handle. */
typedef struct SZrNativeCallLease {
    struct SZrState *state;
    EZrGcNativeSafepointMode nativeSafepointMode;
    TZrUInt32 pinCount;
    TZrUInt32 rootCount;
    /* Number of successful pin/root API obligations acknowledged by the
     * invoker.  These are separate from pinCount/rootCount, which count only
     * concrete GC records needed for reverse cleanup: a native pointer or a
     * nullable null can legitimately be a no-op while still satisfying the
     * plan's required operation. */
    TZrUInt32 pinObligationCount;
    TZrUInt32 rootObligationCount;
    TZrUInt64 sourceId;
    TZrUInt32 requiredPinCount;
    TZrUInt32 requiredRootCount;
    TZrBool nativeEntered;
    TZrBool attachedByCall;
    TZrBool active;
    TZrBool reserved0;
    SZrGcNativeCallPin pins[ZR_NATIVE_CALL_MAX_PINNED_VALUES];
    SZrGcRootHandle roots[ZR_NATIVE_CALL_MAX_ROOTS];
} SZrNativeCallLease;

/* Pointer-free callback registry slot.  The callback address is resolved
 * separately and is intentionally absent from this type.  A slot is a
 * zero-initialized value before Register; Register/Enter/Leave may then be
 * called concurrently. */
typedef struct ZR_STRUCT_ALIGN SZrNativeCallbackSlot {
    volatile TZrUInt32 state;
    volatile TZrUInt32 inFlight;
    volatile TZrUInt32 lock;
    TZrUInt32 reserved0;
    TZrUInt64 callbackId;
    TZrUInt64 generation;
} SZrNativeCallbackSlot;

typedef enum EZrNativeCallbackSlotState {
    ZR_NATIVE_CALLBACK_SLOT_UNREGISTERED = 0,
    ZR_NATIVE_CALLBACK_SLOT_ACTIVE = 1,
    ZR_NATIVE_CALLBACK_SLOT_CLOSING = 2
} EZrNativeCallbackSlotState;

typedef EZrNativeCallStatus (*FZrNativeCallResolvedInvoker)(
        struct SZrState *state,
        const SZrNativeCallPlan *plan,
        SZrNativeCallLease *lease,
        TZrPtr userData,
        SZrNativeCallDiagnostic *diagnostic);

ZR_CORE_API void ZrCore_NativeCall_DiagnosticClear(
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_NativeCall_StatusName(
        EZrNativeCallStatus status);
ZR_CORE_API TZrBool ZrCore_NativeCall_ValidatePlan(
        const SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCall_Prepare(
        const SZrNativeCallRequest *request,
        SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrUInt64 ZrCore_NativeCall_ComputeLayoutHash(
        const SZrNativeImportContract *contract);

ZR_CORE_API TZrBool ZrCore_NativeCall_LeaseBegin(
        struct SZrState *state,
        const SZrNativeCallPlan *plan,
        SZrNativeCallLease *lease,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCall_LeasePinValue(
        SZrNativeCallLease *lease,
        const struct SZrTypeValue *value,
        SZrNativeCallDiagnostic *diagnostic);
/* Pin a raw managed object for the call duration.  This is the object
 * counterpart to LeasePinValue; both operations append an owned pin to the
 * bounded lease and are released by LeaseEnd. */
ZR_CORE_API TZrBool ZrCore_NativeCall_LeasePinObject(
        SZrNativeCallLease *lease,
        struct SZrRawObject *object,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCall_LeaseRootObject(
        SZrNativeCallLease *lease,
        struct SZrRawObject *object,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_NativeCall_LeaseEnd(
        SZrNativeCallLease *lease,
        SZrNativeCallDiagnostic *diagnostic);

ZR_CORE_API TZrBool ZrCore_NativeCall_Attach(
        struct SZrState *ownerState,
        struct SZrState *mutatorState,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_NativeCall_Detach(struct SZrState *mutatorState);

ZR_CORE_API TZrBool ZrCore_NativeCall_MarshalArgument(
        const SZrNativeCallPlan *plan,
        TZrUInt32 parameterIndex,
        const SZrNativeCallMarshalRequest *request,
        SZrNativeCallMarshalResult *result,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCall_WriteBackArgument(
        const SZrNativeCallPlan *plan,
        TZrUInt32 parameterIndex,
        const void *temporary,
        TZrSize temporarySize,
        void *destination,
        TZrSize destinationCapacity,
        SZrNativeCallDiagnostic *diagnostic);

/* Resolve the process-local callback/address at the call site.  The
 * resolver is deliberately passed per invocation so a reloaded module can
 * never leave a stale address in a cached plan. */
ZR_CORE_API TZrBool ZrCore_NativeCall_InvokeResolved(
        struct SZrState *state,
        const SZrNativeCallPlan *plan,
        FZrNativeCallResolvedInvoker invoker,
        TZrPtr userData,
        SZrNativeCallDiagnostic *diagnostic);

ZR_CORE_API TZrBool ZrCore_NativeCallback_Register(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        TZrUInt64 generation,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCallback_Enter(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_NativeCallback_Leave(
        SZrNativeCallbackSlot *slot);
ZR_CORE_API TZrBool ZrCore_NativeCallback_BeginUnregister(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCallback_FinishUnregister(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        SZrNativeCallDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_NativeCallback_Unregister(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        TZrUInt32 maxSpins,
        SZrNativeCallDiagnostic *diagnostic);

#endif /* ZR_VM_CORE_NATIVE_CALL_CONTRACT_H */
