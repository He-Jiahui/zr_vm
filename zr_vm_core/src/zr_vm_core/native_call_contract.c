#include "zr_vm_core/native_call_contract.h"

#include "zr_vm_core/state.h"

#include "zr_vm_common/zr_io_conf.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static void native_call_set_diagnostic(
        SZrNativeCallDiagnostic *diagnostic,
        EZrNativeCallStatus status,
        EZrNativeCallPhase phase,
        TZrUInt32 parameterIndex,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    TZrUInt64 sourceId;

    if (diagnostic == ZR_NULL) {
        return;
    }
    /* Prepare seeds sourceId from the import contract.  Preserve that
     * identity while replacing the status so a later ABI/class failure still
     * points at the originating module instead of becoming an anonymous
     * error.  Other entry points clear the diagnostic first and therefore
     * naturally retain zero. */
    sourceId = diagnostic->sourceId;
    diagnostic->status = status;
    diagnostic->phase = phase;
    diagnostic->parameterIndex = parameterIndex;
    diagnostic->operationIndex = parameterIndex;
    diagnostic->sourceId = sourceId;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

/* Update only the terminal status/phase after a resolver or invoker has
 * supplied richer source/operation/hash details.  Replacing the complete
 * diagnostic here would make an ABI failure look unlocated. */
static void native_call_set_terminal_status(
        SZrNativeCallDiagnostic *diagnostic,
        EZrNativeCallStatus status,
        EZrNativeCallPhase fallbackPhase) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->status = status;
    if (diagnostic->phase == ZR_NATIVE_CALL_PHASE_NONE) {
        diagnostic->phase = fallbackPhase;
    }
}

void ZrCore_NativeCall_DiagnosticClear(
        SZrNativeCallDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_NATIVE_CALL_STATUS_OK;
        diagnostic->phase = ZR_NATIVE_CALL_PHASE_NONE;
        diagnostic->parameterIndex = UINT32_MAX;
        diagnostic->operationIndex = UINT32_MAX;
    }
}

const TZrChar *ZrCore_NativeCall_StatusName(
        EZrNativeCallStatus status) {
    switch (status) {
        case ZR_NATIVE_CALL_STATUS_OK: return "ok";
        case ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT: return "invalid-contract";
        case ZR_NATIVE_CALL_STATUS_INVALID_PLAN: return "invalid-plan";
        case ZR_NATIVE_CALL_STATUS_ABI_MISMATCH: return "abi-mismatch";
        case ZR_NATIVE_CALL_STATUS_SIGNATURE_MISMATCH: return "signature-mismatch";
        case ZR_NATIVE_CALL_STATUS_LAYOUT_MISMATCH: return "layout-mismatch";
        case ZR_NATIVE_CALL_STATUS_PARAMETER_LIMIT: return "parameter-limit";
        case ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE: return "unsupported-type";
        case ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING: return "unsupported-marshalling";
        case ZR_NATIVE_CALL_STATUS_ALIGNMENT: return "alignment";
        case ZR_NATIVE_CALL_STATUS_BUFFER_TOO_SMALL: return "buffer-too-small";
        case ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT: return "null-argument";
        case ZR_NATIVE_CALL_STATUS_WRITEBACK_REQUIRED: return "writeback-required";
        case ZR_NATIVE_CALL_STATUS_ATTACH_REQUIRED: return "attach-required";
        case ZR_NATIVE_CALL_STATUS_ATTACH_FAILED: return "attach-failed";
        case ZR_NATIVE_CALL_STATUS_THREAD_POLICY: return "thread-policy";
        case ZR_NATIVE_CALL_STATUS_NATIVE_ENTER_FAILED: return "native-enter-failed";
        case ZR_NATIVE_CALL_STATUS_PIN_FAILED: return "pin-failed";
        case ZR_NATIVE_CALL_STATUS_ROOT_FAILED: return "root-failed";
        case ZR_NATIVE_CALL_STATUS_NOT_ACTIVE: return "not-active";
        case ZR_NATIVE_CALL_STATUS_ALREADY_ACTIVE: return "already-active";
        case ZR_NATIVE_CALL_STATUS_CALLBACK_UNRESOLVED: return "callback-unresolved";
        case ZR_NATIVE_CALL_STATUS_CALLBACK_REJECTED: return "callback-rejected";
        case ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT: return "callback-in-flight";
        case ZR_NATIVE_CALL_STATUS_CALLBACK_NOT_REGISTERED: return "callback-not-registered";
        case ZR_NATIVE_CALL_STATUS_CALLBACK_ALREADY_CLOSING: return "callback-already-closing";
        case ZR_NATIVE_CALL_STATUS_EXCEPTION_TRANSLATION: return "exception-translation";
        case ZR_NATIVE_CALL_STATUS_INVOKE_UNRESOLVED: return "invoke-unresolved";
        case ZR_NATIVE_CALL_STATUS_INVOKE_FAILED: return "invoke-failed";
        case ZR_NATIVE_CALL_STATUS_BUDGET: return "budget";
        case ZR_NATIVE_CALL_STATUS_INTERNAL: return "internal";
        default: return "unknown";
    }
}

static TZrUInt64 native_call_hash_byte(TZrUInt64 hash, TZrUInt8 value) {
    return (hash ^ value) * UINT64_C(1099511628211);
}

static TZrUInt64 native_call_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; index++) {
        hash = native_call_hash_byte(hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static TZrUInt64 native_call_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; index++) {
        hash = native_call_hash_byte(hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static TZrUInt64 native_call_plan_hash(const SZrNativeCallPlan *plan) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;

    if (plan == ZR_NULL) {
        return 0u;
    }
    hash = native_call_hash_u32(hash, plan->magic);
    hash = native_call_hash_u32(hash, plan->schemaVersion);
    hash = native_call_hash_u32(hash, plan->parameterCount);
    hash = native_call_hash_u32(hash, plan->marshalOpCount);
    hash = native_call_hash_u64(hash, plan->signatureHash);
    hash = native_call_hash_u64(hash, plan->layoutHash);
    hash = native_call_hash_u64(hash, plan->sourceId);
    hash = native_call_hash_u64(hash, plan->requiredCapabilities);
    hash = native_call_hash_u64(hash, plan->callbackId);
    hash = native_call_hash_u64(hash, plan->targetAbiHash);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->abi);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->charset);
    hash = native_call_hash_u32(hash, plan->targetPointerSize);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->targetEndianness);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->exceptionPolicy);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->cleanupPolicy);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->callbackLifetime);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->callbackThreadPolicy);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->callbackExceptionPolicy);
    hash = native_call_hash_u32(hash, (TZrUInt32)plan->nativeSafepointMode);
    hash = native_call_hash_u32(hash, plan->flags);
    hash = native_call_hash_u32(hash, plan->pinCount);
    hash = native_call_hash_u32(hash, plan->rootCount);
    hash = native_call_hash_u32(hash, plan->isVariadic ? 1u : 0u);
    hash = native_call_hash_u32(hash, plan->directCompatible ? 1u : 0u);
    hash = native_call_hash_u32(hash, plan->requiresBridge ? 1u : 0u);
    for (index = 0u; index < plan->marshalOpCount; index++) {
        const SZrNativeCallMarshalOp *op = &plan->marshalOps[index];
        hash = native_call_hash_u32(hash, (TZrUInt32)op->abiClass);
        hash = native_call_hash_u32(hash, (TZrUInt32)op->marshalKind);
        hash = native_call_hash_u32(hash, (TZrUInt32)op->direction);
        hash = native_call_hash_u32(hash, (TZrUInt32)op->ownership);
        hash = native_call_hash_u32(hash, op->parameterIndex);
        hash = native_call_hash_u32(hash, op->byteSize);
        hash = native_call_hash_u32(hash, op->byteAlignment);
        hash = native_call_hash_u32(hash, op->flags);
        hash = native_call_hash_u64(hash, op->canonicalTypeHash);
        hash = native_call_hash_u64(hash, op->layoutHash);
    }
    return hash;
}

static TZrBool native_call_enum_ranges_valid(const SZrNativeCallPlan *plan) {
    TZrUInt32 index;
    if (plan == ZR_NULL ||
        (TZrUInt32)plan->nativeSafepointMode >
                (TZrUInt32)ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL ||
        (TZrUInt32)plan->abi > (TZrUInt32)ZR_FFI_CONTRACT_ABI_STDCALL ||
        (TZrUInt32)plan->charset > (TZrUInt32)ZR_FFI_CONTRACT_CHARSET_ANSI ||
        (TZrUInt32)plan->targetEndianness >
                (TZrUInt32)ZR_FFI_CONTRACT_ENDIAN_BIG ||
        (TZrUInt32)plan->exceptionPolicy >
                (TZrUInt32)ZR_FFI_CONTRACT_ERROR_THROWS ||
        (TZrUInt32)plan->cleanupPolicy >
                (TZrUInt32)ZR_FFI_CONTRACT_CLEANUP_REGISTERED ||
        (TZrUInt32)plan->callbackLifetime >
                (TZrUInt32)ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC ||
        (TZrUInt32)plan->callbackThreadPolicy >
                (TZrUInt32)ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN ||
        (TZrUInt32)plan->callbackExceptionPolicy >
                (TZrUInt32)ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ERROR_RESULT) {
        return ZR_FALSE;
    }
    for (index = 0u; index < plan->marshalOpCount; index++) {
        const SZrNativeCallMarshalOp *op = &plan->marshalOps[index];
        if ((TZrUInt32)op->abiClass >
                    (TZrUInt32)ZR_NATIVE_CALL_ABI_CLASS_UNSUPPORTED ||
            (TZrUInt32)op->marshalKind >
                    (TZrUInt32)ZR_NATIVE_CALL_MARSHAL_REJECT ||
            (TZrUInt32)op->direction >
                    (TZrUInt32)ZR_FFI_CONTRACT_DIRECTION_OUT ||
            (TZrUInt32)op->ownership >
                    (TZrUInt32)ZR_FFI_CONTRACT_OWNERSHIP_PINNED ||
            op->parameterIndex != index || op->byteSize == 0u ||
            op->byteAlignment == 0u ||
            (op->byteAlignment & (op->byteAlignment - 1u)) != 0u ||
            op->canonicalTypeHash == 0u ||
            op->layoutHash == 0u ||
            (op->flags & ~((TZrUInt32)ZR_NATIVE_CALL_MARSHAL_FLAG_KNOWN_MASK)) !=
                    0u) {
            return ZR_FALSE;
        }
        if ((op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) !=
                    ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN) != 0u) ||
            ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK) != 0u &&
             op->direction == ZR_FFI_CONTRACT_DIRECTION_IN) ||
            ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) != 0u &&
             (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_DIRECT ||
              op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT)) ||
            ((op->marshalKind == ZR_NATIVE_CALL_MARSHAL_DIRECT ||
              op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) &&
             (op->ownership == ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER ||
              op->ownership == ZR_FFI_CONTRACT_OWNERSHIP_SHARED))) {
            return ZR_FALSE;
        }
        switch (op->abiClass) {
            case ZR_NATIVE_CALL_ABI_CLASS_SCALAR:
            case ZR_NATIVE_CALL_ABI_CLASS_INLINE_STRUCT:
                if (op->direction != ZR_FFI_CONTRACT_DIRECTION_IN) {
                    return ZR_FALSE;
                }
                if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_REGISTERED ||
                    op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT ||
                    op->ownership == ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER ||
                    op->ownership == ZR_FFI_CONTRACT_OWNERSHIP_SHARED ||
                    (op->flags & (ZR_NATIVE_CALL_MARSHAL_FLAG_NULLABLE |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK)) !=
                            0u) {
                    return ZR_FALSE;
                }
                break;
            case ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW:
            case ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY:
                if (op->direction != ZR_FFI_CONTRACT_DIRECTION_IN ||
                    (op->marshalKind != ZR_NATIVE_CALL_MARSHAL_COPY &&
                     op->marshalKind != ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT &&
                     op->marshalKind != ZR_NATIVE_CALL_MARSHAL_REGISTERED)) {
                    return ZR_FALSE;
                }
                if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) {
                    if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN) ==
                            0u ||
                        (op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) !=
                                0u) {
                        return ZR_FALSE;
                    }
                } else if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) ==
                                   0u ||
                           (op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN) !=
                                   0u) {
                    return ZR_FALSE;
                }
                break;
            case ZR_NATIVE_CALL_ABI_CLASS_REF_OUT:
                if (op->direction == ZR_FFI_CONTRACT_DIRECTION_IN ||
                    op->marshalKind != ZR_NATIVE_CALL_MARSHAL_COPY ||
                    (op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_NULLABLE) != 0u ||
                    (op->flags & (ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK)) !=
                            (ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT |
                             ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_NATIVE_CALL_ABI_CLASS_VARARGS:
                if (op->direction != ZR_FFI_CONTRACT_DIRECTION_IN ||
                    op->marshalKind != ZR_NATIVE_CALL_MARSHAL_BRIDGE ||
                    ((op->ownership == ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER ||
                      op->ownership == ZR_FFI_CONTRACT_OWNERSHIP_SHARED) &&
                     (op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) ==
                             0u) ||
                    (op->flags & (ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK)) !=
                            0u) {
                    return ZR_FALSE;
                }
                break;
            case ZR_NATIVE_CALL_ABI_CLASS_CALLBACK:
                if (op->direction != ZR_FFI_CONTRACT_DIRECTION_IN ||
                    (op->marshalKind != ZR_NATIVE_CALL_MARSHAL_BRIDGE &&
                     op->marshalKind != ZR_NATIVE_CALL_MARSHAL_REGISTERED) ||
                    (op->flags & (ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK |
                                  ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT)) !=
                            ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) {
                    return ZR_FALSE;
                }
                break;
            default:
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCall_ValidatePlan(
        const SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 pinCount = 0u;
    TZrUInt32 rootCount = 0u;
    TZrBool hasCallback = ZR_FALSE;
    TZrBool hasRefOut = ZR_FALSE;
    TZrBool hasVarargs = ZR_FALSE;
    TZrBool requiresBridge = ZR_FALSE;
    TZrBool directCompatible = ZR_TRUE;
    TZrUInt64 expectedPlanHash;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (plan == ZR_NULL) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = plan->sourceId;
    }
    if (plan->magic != ZR_NATIVE_CALL_PLAN_MAGIC ||
        plan->schemaVersion != ZR_NATIVE_CALL_CONTRACT_SCHEMA_VERSION ||
        plan->valid != ZR_TRUE || plan->directCompatible > ZR_TRUE ||
        plan->requiresBridge > ZR_TRUE ||
        plan->parameterCount > ZR_NATIVE_CALL_MAX_PARAMETERS ||
        plan->marshalOpCount != plan->parameterCount ||
        plan->flags & ~((TZrUInt32)ZR_NATIVE_CALL_PLAN_FLAG_KNOWN_MASK) ||
        (plan->targetPointerSize != 4u && plan->targetPointerSize != 8u) ||
        plan->isVariadic > ZR_TRUE ||
        plan->reserved0 != 0u ||
        plan->signatureHash == 0u || plan->layoutHash == 0u ||
        plan->sourceId == 0u ||
        (plan->requiredCapabilities &
         ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME) == 0u ||
        plan->exceptionPolicy == ZR_FFI_CONTRACT_ERROR_THROWS ||
        plan->cleanupPolicy == ZR_FFI_CONTRACT_CLEANUP_REGISTERED ||
        plan->targetAbiHash == 0u || !native_call_enum_ranges_valid(plan)) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_PLAN,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < plan->marshalOpCount; index++) {
        const SZrNativeCallMarshalOp *op = &plan->marshalOps[index];
        if (op->abiClass == ZR_NATIVE_CALL_ABI_CLASS_UNSUPPORTED ||
            op->abiClass == ZR_NATIVE_CALL_ABI_CLASS_AUTO ||
            op->marshalKind == ZR_NATIVE_CALL_MARSHAL_REJECT) {
            native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE,
                                       ZR_NATIVE_CALL_PHASE_VALIDATE, index, 0u, 0u);
            return ZR_FALSE;
        }
        if (op->abiClass == ZR_NATIVE_CALL_ABI_CLASS_CALLBACK) {
            hasCallback = ZR_TRUE;
        }
        if (op->abiClass == ZR_NATIVE_CALL_ABI_CLASS_REF_OUT) {
            hasRefOut = ZR_TRUE;
        }
        if (op->abiClass == ZR_NATIVE_CALL_ABI_CLASS_VARARGS) {
            hasVarargs = ZR_TRUE;
        }
        if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) != 0u) {
            rootCount++;
        }
        if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN) != 0u) {
            pinCount++;
        }
        if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK) != 0u) {
            hasRefOut = ZR_TRUE;
        }
        if (op->marshalKind != ZR_NATIVE_CALL_MARSHAL_DIRECT &&
            op->marshalKind != ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) {
            directCompatible = ZR_FALSE;
        }
        if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY ||
            op->marshalKind == ZR_NATIVE_CALL_MARSHAL_BRIDGE ||
            op->marshalKind == ZR_NATIVE_CALL_MARSHAL_REGISTERED) {
            requiresBridge = ZR_TRUE;
        }
    }
    /* A variadic call has an untyped tail even when its fixed prefix happens
     * to contain only direct operations.  Keep the plan on the bridge lane;
     * this also makes a zero-parameter variadic plan conservative. */
    if (plan->isVariadic) {
        directCompatible = ZR_FALSE;
        requiresBridge = ZR_TRUE;
    }
    if (hasCallback &&
        (plan->callbackLifetime != ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL ||
         plan->callbackThreadPolicy == ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE ||
         plan->callbackExceptionPolicy ==
                 ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE)) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_PLAN,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 1u, 0u);
        return ZR_FALSE;
    }
    expectedPlanHash = native_call_plan_hash(plan);
    if (plan->pinCount != pinCount || plan->rootCount != rootCount ||
        (plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK) !=
                (hasCallback ? ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK : 0u) ||
        (plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_HAS_REF_OUT) !=
                (hasRefOut ? ZR_NATIVE_CALL_PLAN_FLAG_HAS_REF_OUT : 0u) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_HAS_VARARGS) != 0u) !=
                (hasVarargs || plan->isVariadic) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH) != 0u) !=
                (hasCallback &&
                 plan->callbackThreadPolicy ==
                         ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN) != 0u) !=
                (pinCount != 0u) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT) != 0u) !=
                (rootCount != 0u) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_WRITEBACK) != 0u) !=
                (hasRefOut != ZR_FALSE) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_CALLBACK_RELOAD) != 0u) !=
                (hasCallback != ZR_FALSE) ||
        ((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE) != 0u) !=
                requiresBridge ||
        (hasVarargs && plan->isVariadic == ZR_FALSE) ||
        ((plan->nativeSafepointMode ==
                  ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED ||
          plan->nativeSafepointMode ==
                  ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) &&
         (pinCount != 0u || rootCount != 0u ||
          (plan->flags & (ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK |
                          ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH)) != 0u)) ||
        plan->requiresBridge != requiresBridge ||
        plan->directCompatible != directCompatible ||
        (hasCallback && plan->callbackId == 0u) ||
        (!hasCallback && plan->callbackId != 0u) ||
        plan->planHash != expectedPlanHash) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_PLAN,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   expectedPlanHash, plan->planHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool native_call_contract_type_is_scalar(EZrFfiTypeKind kind) {
    return (TZrBool)((kind >= ZR_FFI_CONTRACT_TYPE_BOOL &&
                      kind <= ZR_FFI_CONTRACT_TYPE_ISIZE) ||
                     kind == ZR_FFI_CONTRACT_TYPE_ENUM);
}

static TZrBool native_call_type_abi_size_valid(
        EZrFfiTypeKind kind,
        TZrUInt32 size,
        TZrUInt32 targetPointerSize,
        TZrUInt32 *expectedSize) {
    TZrUInt32 expected = 0u;

    switch (kind) {
        case ZR_FFI_CONTRACT_TYPE_BOOL:
        case ZR_FFI_CONTRACT_TYPE_I8:
        case ZR_FFI_CONTRACT_TYPE_U8:
            expected = 1u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I16:
        case ZR_FFI_CONTRACT_TYPE_U16:
            expected = 2u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I32:
        case ZR_FFI_CONTRACT_TYPE_U32:
        case ZR_FFI_CONTRACT_TYPE_F32:
            expected = 4u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I64:
        case ZR_FFI_CONTRACT_TYPE_U64:
        case ZR_FFI_CONTRACT_TYPE_F64:
            /* Some 32-bit ABIs align an eight-byte scalar to four bytes,
             * but its representation is still exactly eight bytes. */
            expected = 8u;
            break;
        case ZR_FFI_CONTRACT_TYPE_USIZE:
        case ZR_FFI_CONTRACT_TYPE_ISIZE:
        case ZR_FFI_CONTRACT_TYPE_POINTER:
        case ZR_FFI_CONTRACT_TYPE_CALLBACK:
            expected = targetPointerSize;
            break;
        case ZR_FFI_CONTRACT_TYPE_ENUM:
            if (size == 1u || size == 2u || size == 4u || size == 8u) {
                expected = size;
            }
            break;
        default:
            /* Aggregate sizes are described by their layout contract. */
            expected = size;
            break;
    }
    if (expectedSize != ZR_NULL) {
        *expectedSize = expected;
    }
    return (TZrBool)(expected != 0u && expected == size);
}

static TZrBool native_call_type_abi_alignment_valid(
        EZrFfiTypeKind kind,
        TZrUInt32 size,
        TZrUInt32 alignment,
        TZrUInt32 targetPointerSize) {
    TZrUInt32 expected = 0u;

    switch (kind) {
        case ZR_FFI_CONTRACT_TYPE_BOOL:
        case ZR_FFI_CONTRACT_TYPE_I8:
        case ZR_FFI_CONTRACT_TYPE_U8:
            expected = 1u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I16:
        case ZR_FFI_CONTRACT_TYPE_U16:
            expected = 2u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I32:
        case ZR_FFI_CONTRACT_TYPE_U32:
        case ZR_FFI_CONTRACT_TYPE_F32:
            expected = 4u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I64:
        case ZR_FFI_CONTRACT_TYPE_U64:
        case ZR_FFI_CONTRACT_TYPE_F64:
            /* The size is fixed at eight bytes, while the ABI may choose
             * four-byte alignment on a 32-bit target. */
            return (TZrBool)(alignment == 8u ||
                             (targetPointerSize == 4u && alignment == 4u));
        case ZR_FFI_CONTRACT_TYPE_USIZE:
        case ZR_FFI_CONTRACT_TYPE_ISIZE:
        case ZR_FFI_CONTRACT_TYPE_POINTER:
        case ZR_FFI_CONTRACT_TYPE_CALLBACK:
            expected = targetPointerSize;
            break;
        case ZR_FFI_CONTRACT_TYPE_ENUM:
            /* The compiler-side contract lowers enums to their declared
             * integer width.  Use that width's natural alignment as the
             * direct-lane witness; a packed enum can still use COPY, whose
             * temporary receives the natural alignment below. */
            if (size == 1u || size == 2u || size == 4u || size == 8u) {
                return (TZrBool)(alignment == size);
            }
            return ZR_FALSE;
        default:
            /* Aggregate alignment is described by its layout contract. */
            return ZR_TRUE;
    }
    return (TZrBool)(expected != 0u && expected == alignment);
}

/* Return an alignment suitable for a temporary/native frame slot.  The
 * serialized contract may intentionally describe packed or over-aligned
 * source storage; a COPY lane must nevertheless hand the native target an
 * address aligned for the target ABI. */
static TZrUInt32 native_call_safe_type_alignment(
        const SZrNativeImportContract *contract,
        const SZrFfiTypeContract *type) {
    TZrUInt32 safeAlignment;
    TZrUInt32 expected = 0u;

    if (contract == ZR_NULL || type == ZR_NULL) {
        return 1u;
    }
    safeAlignment = type->alignment == 0u ? 1u : type->alignment;
    switch (type->typeKind) {
        case ZR_FFI_CONTRACT_TYPE_BOOL:
        case ZR_FFI_CONTRACT_TYPE_I8:
        case ZR_FFI_CONTRACT_TYPE_U8:
            expected = 1u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I16:
        case ZR_FFI_CONTRACT_TYPE_U16:
            expected = 2u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I32:
        case ZR_FFI_CONTRACT_TYPE_U32:
        case ZR_FFI_CONTRACT_TYPE_F32:
            expected = 4u;
            break;
        case ZR_FFI_CONTRACT_TYPE_I64:
        case ZR_FFI_CONTRACT_TYPE_U64:
        case ZR_FFI_CONTRACT_TYPE_F64:
            expected = (contract->signature.targetPointerSize == 4u) ? 4u : 8u;
            break;
        case ZR_FFI_CONTRACT_TYPE_USIZE:
        case ZR_FFI_CONTRACT_TYPE_ISIZE:
        case ZR_FFI_CONTRACT_TYPE_POINTER:
        case ZR_FFI_CONTRACT_TYPE_CALLBACK:
            expected = contract->signature.targetPointerSize;
            break;
        case ZR_FFI_CONTRACT_TYPE_ENUM:
            if (type->size == 1u || type->size == 2u ||
                type->size == 4u || type->size == 8u) {
                expected = type->size;
            }
            break;
        case ZR_FFI_CONTRACT_TYPE_STRUCT:
        case ZR_FFI_CONTRACT_TYPE_UNION: {
            TZrUInt32 index;

            for (index = 0u; index < type->aggregateFieldCount; index++) {
                const SZrFfiAggregateFieldContract *field =
                        &contract->signature.aggregateFields[
                                type->aggregateFieldStart + index];
                TZrUInt32 fieldExpected = 1u;

                switch (field->typeKind) {
                    case ZR_FFI_CONTRACT_TYPE_I16:
                    case ZR_FFI_CONTRACT_TYPE_U16:
                        fieldExpected = 2u;
                        break;
                    case ZR_FFI_CONTRACT_TYPE_I32:
                    case ZR_FFI_CONTRACT_TYPE_U32:
                    case ZR_FFI_CONTRACT_TYPE_F32:
                        fieldExpected = 4u;
                        break;
                    case ZR_FFI_CONTRACT_TYPE_I64:
                    case ZR_FFI_CONTRACT_TYPE_U64:
                    case ZR_FFI_CONTRACT_TYPE_F64:
                        fieldExpected =
                                contract->signature.targetPointerSize == 4u
                                        ? 4u
                                        : 8u;
                        break;
                    case ZR_FFI_CONTRACT_TYPE_USIZE:
                    case ZR_FFI_CONTRACT_TYPE_ISIZE:
                    case ZR_FFI_CONTRACT_TYPE_POINTER:
                    case ZR_FFI_CONTRACT_TYPE_CALLBACK:
                        fieldExpected = contract->signature.targetPointerSize;
                        break;
                    case ZR_FFI_CONTRACT_TYPE_ENUM:
                        if (field->size == 1u || field->size == 2u ||
                            field->size == 4u || field->size == 8u) {
                            fieldExpected = field->size;
                        }
                        break;
                    default:
                        break;
                }
                if (field->alignment > safeAlignment) {
                    safeAlignment = field->alignment;
                }
                if (fieldExpected > safeAlignment) {
                    safeAlignment = fieldExpected;
                }
            }
            return safeAlignment;
        }
        default:
            /* Unknown aggregate storage keeps the producer's validated
             * alignment; size/layout validation has already ruled out an
             * impossible representation. */
            return safeAlignment;
    }
    if (expected > safeAlignment) {
        safeAlignment = expected;
    }
    return safeAlignment;
}

/* The common FFI contract validator checks that aggregate fields are inside
 * the aggregate, but deliberately leaves ABI ordering/alignment to the
 * backend.  A NativeCallPlan is a cached calling-convention witness, so it
 * must reject a layout that no C ABI can consume even when its producer has
 * recomputed the outer signature hash.  Keep this check structural (rather
 * than depending on libffi) so VM and AOT users share the same conservative
 * boundary. */
static TZrBool native_call_aggregate_layout_valid(
        const SZrNativeImportContract *contract,
        const SZrFfiTypeContract *type) {
    TZrUInt32 cursor = 0u;
    TZrUInt32 index;

    if (contract == ZR_NULL || type == ZR_NULL ||
        (type->typeKind != ZR_FFI_CONTRACT_TYPE_STRUCT &&
         type->typeKind != ZR_FFI_CONTRACT_TYPE_UNION)) {
        return ZR_TRUE;
    }
    if (type->aggregateFieldStart > contract->signature.aggregateFieldCount ||
        type->aggregateFieldCount >
                contract->signature.aggregateFieldCount -
                        type->aggregateFieldStart) {
        return ZR_FALSE;
    }
    if (type->alignment == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < type->aggregateFieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &contract->signature.aggregateFields[
                        type->aggregateFieldStart + index];
        TZrUInt32 fieldEnd;

        /* A packed aggregate may deliberately lower a field's alignment.
         * That is still representable by an explicit byte-copy bridge, so
         * structural validation only requires a well-formed power-of-two
         * alignment.  The direct-lane predicate below performs the stricter
         * natural-ABI alignment check. */
        if (field->alignment == 0u ||
            (field->alignment & (field->alignment - 1u)) != 0u ||
            !native_call_type_abi_size_valid(
                    field->typeKind,
                    field->size,
                    contract->signature.targetPointerSize,
                    ZR_NULL) ||
            field->offset > type->size ||
            field->size > type->size - field->offset) {
            return ZR_FALSE;
        }
        if (type->typeKind == ZR_FFI_CONTRACT_TYPE_UNION) {
            /* The canonical union lowering uses one storage element; every
             * source field therefore has offset zero. */
            if (field->offset != 0u) {
                return ZR_FALSE;
            }
            continue;
        }
        if (field->offset < cursor ||
            field->size > UINT32_MAX - field->offset) {
            return ZR_FALSE;
        }
        fieldEnd = field->offset + field->size;
        cursor = fieldEnd;
    }
    return ZR_TRUE;
}

/* A packed aggregate can be copied safely even though its source field
 * offsets are not naturally aligned.  It must not, however, be selected for
 * a direct C lane.  Keep this predicate separate from the structural check so
 * Prepare can downgrade such a value to COPY instead of accepting an unsafe
 * address. */
static TZrBool native_call_aggregate_direct_layout_compatible(
        const SZrNativeImportContract *contract,
        const SZrFfiTypeContract *type) {
    TZrUInt32 index;

    if (contract == ZR_NULL || type == ZR_NULL ||
        (type->typeKind != ZR_FFI_CONTRACT_TYPE_STRUCT &&
         type->typeKind != ZR_FFI_CONTRACT_TYPE_UNION)) {
        return ZR_TRUE;
    }
    if (type->aggregateFieldStart > contract->signature.aggregateFieldCount ||
        type->aggregateFieldCount >
                contract->signature.aggregateFieldCount -
                        type->aggregateFieldStart) {
        return ZR_FALSE;
    }
    /* A packed aggregate may have a non-rounded outer size.  It is safe to
     * carry through an explicit byte-copy bridge, but it cannot be presented
     * as a naturally aligned by-value C aggregate. */
    if (type->alignment == 0u ||
        (type->size % type->alignment) != 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < type->aggregateFieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &contract->signature.aggregateFields[
                        type->aggregateFieldStart + index];

        if (field->alignment == 0u ||
            !native_call_type_abi_alignment_valid(
                    field->typeKind,
                    field->size,
                    field->alignment,
                    contract->signature.targetPointerSize) ||
            type->alignment < field->alignment ||
            (field->offset % field->alignment) != 0u) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool native_call_class_matches(
        EZrNativeCallAbiClass abiClass,
        EZrFfiTypeKind typeKind,
        EZrFfiDirection direction,
        TZrBool isVariadic) {
    if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_AUTO ||
        abiClass == ZR_NATIVE_CALL_ABI_CLASS_UNSUPPORTED) {
        return ZR_FALSE;
    }
    if (direction != ZR_FFI_CONTRACT_DIRECTION_IN) {
        /* A callback identity is an input capability, not a mutable byte
         * slot.  Treating a ref/out callback as ordinary storage would lose
         * its registry lifetime and permit an address write-back. */
        return (TZrBool)(abiClass == ZR_NATIVE_CALL_ABI_CLASS_REF_OUT &&
                         typeKind != ZR_FFI_CONTRACT_TYPE_CALLBACK);
    }
    switch (abiClass) {
        case ZR_NATIVE_CALL_ABI_CLASS_SCALAR:
            return native_call_contract_type_is_scalar(typeKind);
        case ZR_NATIVE_CALL_ABI_CLASS_INLINE_STRUCT:
            return (TZrBool)(typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
                             typeKind == ZR_FFI_CONTRACT_TYPE_UNION);
        case ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW:
        case ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY:
            return (TZrBool)(typeKind == ZR_FFI_CONTRACT_TYPE_POINTER);
        case ZR_NATIVE_CALL_ABI_CLASS_CALLBACK:
            return (TZrBool)(typeKind == ZR_FFI_CONTRACT_TYPE_CALLBACK);
        case ZR_NATIVE_CALL_ABI_CLASS_VARARGS:
            /* Callback values carry a registry/root lifetime and must use
             * the callback lane even when the enclosing signature is
             * variadic; treating one as an untyped scalar would bypass that
             * protection. */
            return (TZrBool)(isVariadic &&
                             typeKind != ZR_FFI_CONTRACT_TYPE_CALLBACK &&
                             typeKind != ZR_FFI_CONTRACT_TYPE_VOID);
        default:
            return ZR_FALSE;
    }
}

static TZrUInt64 native_call_layout_hash(const SZrNativeImportContract *contract) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;

    /* Include every field that can affect the value representation.  The
     * canonical contract hash also covers ownership/marshalling policy, but
     * this hash is intentionally only the ABI layout witness: changing a
     * primitive width, alignment, or aggregate identity must invalidate a
     * cached native lane even when a producer accidentally reuses a layout
     * hash. */
    hash = native_call_hash_u32(hash, (TZrUInt32)contract->signature.abi);
    hash = native_call_hash_u32(
            hash, contract->signature.targetPointerSize);
    hash = native_call_hash_u32(
            hash, (TZrUInt32)contract->signature.targetEndianness);
    /* The target ABI hash includes compiler/platform details (for example
     * long-double alignment) that are not all represented by the primitive
     * fields below.  Include it in the layout witness so a cached plan cannot
     * be reused after an ABI-model change. */
    hash = native_call_hash_u64(hash, contract->signature.targetAbiHash);
    hash = native_call_hash_u32(hash, contract->signature.parameterCount);
    hash = native_call_hash_u32(
            hash, contract->signature.isVariadic ? 1u : 0u);
    hash = native_call_hash_u32(
            hash, (TZrUInt32)contract->signature.returnType.typeKind);
    hash = native_call_hash_u32(hash, contract->signature.returnType.size);
    hash = native_call_hash_u32(
            hash, contract->signature.returnType.alignment);
    hash = native_call_hash_u64(
            hash, contract->signature.returnType.canonicalTypeHash);
    hash = native_call_hash_u64(hash, contract->signature.returnType.layoutHash);
    hash = native_call_hash_u32(hash, contract->signature.returnType.flags);
    hash = native_call_hash_u32(
            hash, contract->signature.returnType.aggregateFieldStart);
    hash = native_call_hash_u32(
            hash, contract->signature.returnType.aggregateFieldCount);
    for (index = 0u; index < contract->signature.parameterCount; index++) {
        const SZrFfiParameterContract *parameter =
                &contract->signature.parameters[index];
        const SZrFfiTypeContract *type = &parameter->type;

        hash = native_call_hash_u32(hash, (TZrUInt32)type->typeKind);
        hash = native_call_hash_u32(hash, type->size);
        hash = native_call_hash_u32(hash, type->alignment);
        hash = native_call_hash_u64(hash, type->canonicalTypeHash);
        hash = native_call_hash_u64(hash, type->layoutHash);
        hash = native_call_hash_u32(hash, type->flags);
        hash = native_call_hash_u32(hash, type->aggregateFieldStart);
        hash = native_call_hash_u32(hash, type->aggregateFieldCount);
        hash = native_call_hash_u32(hash, (TZrUInt32)parameter->direction);
    }
    hash = native_call_hash_u32(hash, contract->signature.aggregateFieldCount);
    for (index = 0u; index < contract->signature.aggregateFieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &contract->signature.aggregateFields[index];

        hash = native_call_hash_u32(hash, (TZrUInt32)field->typeKind);
        hash = native_call_hash_u32(hash, field->size);
        hash = native_call_hash_u32(hash, field->alignment);
        hash = native_call_hash_u32(hash, field->offset);
    }
    return hash == 0u ? UINT64_C(1) : hash;
}

TZrUInt64 ZrCore_NativeCall_ComputeLayoutHash(
        const SZrNativeImportContract *contract) {
    if (contract == ZR_NULL ||
        !ZrCommon_NativeImportContract_Validate(contract)) {
        return 0u;
    }
    return native_call_layout_hash(contract);
}

TZrBool ZrCore_NativeCall_Prepare(
        const SZrNativeCallRequest *request,
        SZrNativeCallPlan *plan,
        SZrNativeCallDiagnostic *diagnostic) {
    const SZrNativeImportContract *contract;
    TZrUInt64 layoutHash;
    TZrUInt32 index;
    TZrBool allowDirect;
    TZrBool hasCallbackParameter = ZR_FALSE;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (plan != ZR_NULL) {
        memset(plan, 0, sizeof(*plan));
    }
    if (request == ZR_NULL || plan == ZR_NULL || request->contract == ZR_NULL) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    contract = request->contract;
    /* Seed the stable import identity before validating request-side flags as
     * well as the contract itself.  A malformed target-environment witness is
     * still attributable to the import that supplied it. */
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = contract->symbolId != 0u
                                       ? contract->symbolId
                                       : contract->declaringModuleId;
    }
    if (request->allowDirect > ZR_TRUE ||
        request->nativeAddressStable > ZR_TRUE ||
        request->hasTargetEnvironment > ZR_TRUE ||
        (TZrUInt32)request->targetEndianness >
                (TZrUInt32)ZR_FFI_CONTRACT_ENDIAN_BIG ||
        request->reserved0 != 0u) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 1u,
                                   ((TZrUInt64)request->allowDirect << 16u) |
                                           ((TZrUInt64)request->nativeAddressStable << 8u) |
                                           request->hasTargetEnvironment);
        return ZR_FALSE;
    }
    if (!ZrCommon_NativeImportContract_Validate(contract)) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    if ((contract->availability & ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS) == 0u) {
#else
    if ((contract->availability & ZR_FFI_CONTRACT_AVAILABILITY_UNIX) == 0u) {
#endif
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 1u,
                contract->availability);
        return ZR_FALSE;
    }
#if !defined(ZR_PLATFORM_WIN) && !defined(_WIN32)
    /* libffi exposes STDCALL only on the Windows ABI (and the native C
     * emitter has no portable way to spell it on Unix targets).  Reject it
     * at the shared boundary instead of publishing a plan whose call-frame
     * convention would be silently interpreted as C calling convention. */
    if (contract->signature.abi == ZR_FFI_CONTRACT_ABI_STDCALL) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                contract->signature.abi);
        return ZR_FALSE;
    }
    /* `GetLastError` is a Windows-only post-call channel.  Treating a Unix
     * errno (or an arbitrary thread-local value) as the requested policy
     * would make an otherwise ABI-compatible plan report the wrong native
     * failure.  Keep this rejection in the shared core boundary so AOT and
     * FFI consumers make the same conservative decision. */
    if (contract->signature.errorPolicy == ZR_FFI_CONTRACT_ERROR_LAST_ERROR) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                contract->signature.errorPolicy);
        return ZR_FALSE;
    }
#endif
    /* The common validator predates this runtime cache and treats C boolean
     * fields as ordinary bytes.  Reject non-canonical values here so a
     * malformed serialized contract cannot silently choose a different lane.
     */
    if (contract->signature.isVariadic > ZR_TRUE ||
        contract->callable.isVariadic > ZR_TRUE) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, ZR_TRUE,
                contract->signature.isVariadic);
        return ZR_FALSE;
    }
    for (index = 0u; index < contract->signature.parameterCount; index++) {
        if (contract->signature.parameters[index].isNullable > ZR_TRUE ||
            contract->callable.parameters[index].acceptsTemporary > ZR_TRUE) {
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index, ZR_TRUE,
                    contract->signature.parameters[index].isNullable);
            return ZR_FALSE;
        }
        if (contract->signature.parameters[index].type.typeKind ==
                ZR_FFI_CONTRACT_TYPE_CALLBACK) {
            hasCallbackParameter = ZR_TRUE;
        }
        if (contract->signature.parameters[index].type.canonicalTypeHash == 0u) {
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index, 1u, 0u);
            return ZR_FALSE;
        }
        /* The callable contract is the language-side view of the same
         * parameter.  A producer that hashes a different type there could
         * otherwise pass the common structural checks while selecting a lane
         * for bytes with a different meaning. */
        if (contract->callable.parameters[index].canonicalTypeHash !=
            contract->signature.parameters[index].type.canonicalTypeHash) {
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                    contract->signature.parameters[index].type.canonicalTypeHash,
                    contract->callable.parameters[index].canonicalTypeHash);
            return ZR_FALSE;
        }
    }
    /* The compact lease owns callback roots only for the active native call.
     * Scoped/static callback lifetimes need a provider-owned persistent root
     * registry (and an explicit close protocol) that this plan does not yet
     * carry.  Reject those contracts instead of publishing a callback that
     * could outlive its managed context. */
    if (hasCallbackParameter &&
        (contract->signature.callbackLifetime ==
                 ZR_FFI_CONTRACT_CALLBACK_LIFETIME_SCOPED ||
         contract->signature.callbackLifetime ==
                 ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC)) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL,
                contract->signature.callbackLifetime);
        return ZR_FALSE;
    }
    if (contract->signature.returnType.typeKind != ZR_FFI_CONTRACT_TYPE_VOID &&
        contract->signature.returnType.canonicalTypeHash == 0u) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 1u, 0u);
        return ZR_FALSE;
    }
    /* Callback identities are resolved through the per-call registry and the
     * compact plan has no return-value callback slot/root witness.  Publishing
     * a callback return as an ordinary pointer would therefore allow a stale
     * process-local code address to escape the resolver boundary. */
    if (contract->signature.returnType.typeKind ==
            ZR_FFI_CONTRACT_TYPE_CALLBACK) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                ZR_FFI_CONTRACT_TYPE_CALLBACK,
                contract->signature.returnType.typeKind);
        return ZR_FALSE;
    }
    if (contract->signature.returnType.canonicalTypeHash != 0u &&
        contract->callable.returnTypeHash !=
                contract->signature.returnType.canonicalTypeHash) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                contract->signature.returnType.canonicalTypeHash,
                contract->callable.returnTypeHash);
        return ZR_FALSE;
    }
    layoutHash = native_call_layout_hash(contract);
    if ((request->flags & ~((TZrUInt32)ZR_NATIVE_CALL_REQUEST_FLAG_KNOWN_MASK)) !=
                0u) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   ZR_NATIVE_CALL_REQUEST_FLAG_KNOWN_MASK,
                                   request->flags);
        return ZR_FALSE;
    }
    if ((request->flags & ZR_NATIVE_CALL_REQUEST_FLAG_BLOCKING_DETACHED) != 0u &&
        (request->flags & ZR_NATIVE_CALL_REQUEST_FLAG_NO_SAFEPOINT_CRITICAL) != 0u) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_THREAD_POLICY,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                                   request->flags);
        return ZR_FALSE;
    }
    if (request->expectedSignatureHash != 0u &&
        request->expectedSignatureHash != contract->signature.signatureHash) {
        native_call_set_diagnostic(diagnostic,
                                   ZR_NATIVE_CALL_STATUS_SIGNATURE_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   contract->signature.signatureHash,
                                   request->expectedSignatureHash);
        return ZR_FALSE;
    }
    if (request->expectedLayoutHash != 0u &&
        request->expectedLayoutHash != layoutHash) {
        native_call_set_diagnostic(diagnostic,
                                   ZR_NATIVE_CALL_STATUS_LAYOUT_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   layoutHash, request->expectedLayoutHash);
        return ZR_FALSE;
    }
    if (request->targetAbiHash != 0u &&
        request->targetAbiHash != contract->signature.targetAbiHash) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   contract->signature.targetAbiHash,
                                   request->targetAbiHash);
        return ZR_FALSE;
    }
    if (request->targetPointerSize != 0u &&
        request->targetPointerSize != contract->signature.targetPointerSize) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   contract->signature.targetPointerSize,
                                   request->targetPointerSize);
        return ZR_FALSE;
    }
    if (request->hasTargetEnvironment &&
        request->targetEndianness != contract->signature.targetEndianness) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   contract->signature.targetEndianness,
                                   request->targetEndianness);
        return ZR_FALSE;
    }
    if (request->hasTargetEnvironment &&
        request->targetPointerSize != contract->signature.targetPointerSize) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   contract->signature.targetPointerSize,
                                   request->targetPointerSize);
        return ZR_FALSE;
    }
    if (request->hasTargetEnvironment &&
        request->targetAbiHash != contract->signature.targetAbiHash) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   contract->signature.targetAbiHash,
                                   request->targetAbiHash);
        return ZR_FALSE;
    }
    /* A plan is consumed by this process's C ABI.  Do not publish a lane for
     * a foreign pointer width or byte order merely because a producer
     * recomputed the target hash; such a lane would reinterpret every raw
     * pointer at invocation time. */
    if (contract->signature.targetPointerSize != (TZrUInt32)sizeof(void *) ||
        contract->signature.targetEndianness !=
                (ZR_IO_IS_LITTLE_ENDIAN ? ZR_FFI_CONTRACT_ENDIAN_LITTLE
                                        : ZR_FFI_CONTRACT_ENDIAN_BIG)) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   sizeof(void *),
                                   contract->signature.targetPointerSize);
        return ZR_FALSE;
    }
    if (contract->signature.parameterCount > ZR_NATIVE_CALL_MAX_PARAMETERS) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_PARAMETER_LIMIT,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   ZR_NATIVE_CALL_MAX_PARAMETERS,
                                   contract->signature.parameterCount);
        return ZR_FALSE;
    }
    if (contract->signature.returnType.typeKind != ZR_FFI_CONTRACT_TYPE_VOID &&
        !native_call_type_abi_size_valid(
                contract->signature.returnType.typeKind,
                contract->signature.returnType.size,
                contract->signature.targetPointerSize,
                ZR_NULL)) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                contract->signature.returnType.size);
        return ZR_FALSE;
    }
    if (contract->signature.returnType.typeKind != ZR_FFI_CONTRACT_TYPE_VOID &&
        !native_call_type_abi_alignment_valid(
                contract->signature.returnType.typeKind,
                contract->signature.returnType.size,
                contract->signature.returnType.alignment,
                contract->signature.targetPointerSize)) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                contract->signature.returnType.alignment);
        return ZR_FALSE;
    }
    for (index = 0u; index < contract->signature.parameterCount; index++) {
        const SZrFfiParameterContract *parameter =
                &contract->signature.parameters[index];

        if (!native_call_type_abi_size_valid(
                    parameter->type.typeKind,
                    parameter->type.size,
                    contract->signature.targetPointerSize,
                    ZR_NULL)) {
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index, 0u,
                    parameter->type.size);
            return ZR_FALSE;
        }
        if (!native_call_aggregate_layout_valid(contract, &parameter->type)) {
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index, 0u,
                    parameter->type.layoutHash);
            return ZR_FALSE;
        }
    }
    if (!native_call_aggregate_layout_valid(
                contract, &contract->signature.returnType)) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                contract->signature.returnType.layoutHash);
        return ZR_FALSE;
    }
    if ((contract->signature.returnType.typeKind ==
                 ZR_FFI_CONTRACT_TYPE_STRUCT ||
         contract->signature.returnType.typeKind ==
                 ZR_FFI_CONTRACT_TYPE_UNION) &&
        (contract->signature.returnType.flags &
         ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE) == 0u) {
        /* The compact plan has no return-temporary/write-back lane yet.  A
         * non-blittable aggregate therefore cannot be published as a direct
         * C return; reject it until a backend-specific return bridge exists. */
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE,
                contract->signature.returnType.flags);
        return ZR_FALSE;
    }
    /* There is no per-return temporary in the compact plan yet.  Do not
     * publish a direct return lane for a packed/non-native aggregate until a
     * backend supplies an explicit return bridge. */
    if (!native_call_aggregate_direct_layout_compatible(
                contract, &contract->signature.returnType)) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ABI_MISMATCH,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                contract->signature.returnType.layoutHash);
        return ZR_FALSE;
    }

    plan->magic = ZR_NATIVE_CALL_PLAN_MAGIC;
    plan->schemaVersion = ZR_NATIVE_CALL_CONTRACT_SCHEMA_VERSION;
    plan->parameterCount = contract->signature.parameterCount;
    plan->marshalOpCount = plan->parameterCount;
    plan->isVariadic = contract->signature.isVariadic;
    plan->signatureHash = contract->signature.signatureHash;
    plan->layoutHash = native_call_layout_hash(contract);
    plan->sourceId = contract->symbolId != 0u
                             ? contract->symbolId
                             : contract->declaringModuleId;
    plan->requiredCapabilities = contract->requiredCapabilities;
    plan->callbackId = request->callbackId;
    plan->targetAbiHash = contract->signature.targetAbiHash;
    plan->abi = contract->signature.abi;
    plan->charset = contract->signature.charset;
    plan->targetPointerSize = contract->signature.targetPointerSize;
    plan->targetEndianness = contract->signature.targetEndianness;
    plan->exceptionPolicy = contract->signature.errorPolicy;
    plan->cleanupPolicy = contract->signature.cleanupPolicy;
    plan->callbackLifetime = contract->signature.callbackLifetime;
    plan->callbackThreadPolicy = contract->signature.callbackThreadPolicy;
    plan->callbackExceptionPolicy = contract->signature.callbackExceptionPolicy;
    plan->nativeSafepointMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    if ((request->flags & ZR_NATIVE_CALL_REQUEST_FLAG_BLOCKING_DETACHED) != 0u) {
        plan->nativeSafepointMode = ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED;
    } else if ((request->flags & ZR_NATIVE_CALL_REQUEST_FLAG_NO_SAFEPOINT_CRITICAL) != 0u) {
        plan->nativeSafepointMode = ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL;
    }
    allowDirect = (TZrBool)(request->allowDirect && request->nativeAddressStable &&
                            (request->flags & ZR_NATIVE_CALL_REQUEST_FLAG_DISALLOW_DIRECT) == 0u);
    if (contract->signature.isVariadic) {
        plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_HAS_VARARGS |
                       ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE;
    }
    for (index = 0u; index < plan->parameterCount; index++) {
        const SZrFfiParameterContract *parameter =
                &contract->signature.parameters[index];
        SZrNativeCallMarshalOp *op = &plan->marshalOps[index];
        EZrNativeCallAbiClass requestedClass = request->parameterClasses[index];
        EZrNativeCallAbiClass abiClass = requestedClass;
        TZrBool canDirect = allowDirect;
        EZrFfiMarshallingKind marshalling = parameter->marshalling;

        memset(op, 0, sizeof(*op));
        if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_AUTO) {
            if (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_CALLBACK) {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_CALLBACK;
            } else if (parameter->direction != ZR_FFI_CONTRACT_DIRECTION_IN) {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_REF_OUT;
            } else if (contract->signature.isVariadic) {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_VARARGS;
            } else if (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
                       parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_UNION) {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_INLINE_STRUCT;
            } else if (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_POINTER) {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW;
            } else if (native_call_contract_type_is_scalar(parameter->type.typeKind) ||
                       parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_ENUM) {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_SCALAR;
            } else {
                abiClass = ZR_NATIVE_CALL_ABI_CLASS_UNSUPPORTED;
            }
        }
        if (!native_call_class_matches(abiClass, parameter->type.typeKind,
                                       parameter->direction,
                                       contract->signature.isVariadic)) {
            native_call_set_diagnostic(diagnostic,
                                       ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE,
                                       ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                                       parameter->type.typeKind, abiClass);
            goto prepare_fail;
        }
        if (parameter->isNullable &&
            parameter->type.typeKind != ZR_FFI_CONTRACT_TYPE_POINTER &&
            parameter->type.typeKind != ZR_FFI_CONTRACT_TYPE_CALLBACK) {
            native_call_set_diagnostic(diagnostic,
                                       ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE,
                                       ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                                       ZR_FFI_CONTRACT_TYPE_POINTER,
                                       parameter->type.typeKind);
            goto prepare_fail;
        }
        if (parameter->isNullable &&
            parameter->direction != ZR_FFI_CONTRACT_DIRECTION_IN) {
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index, 0u,
                    parameter->direction);
            goto prepare_fail;
        }
        if ((parameter->ownership == ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER ||
             parameter->ownership == ZR_FFI_CONTRACT_OWNERSHIP_SHARED) &&
            parameter->type.typeKind != ZR_FFI_CONTRACT_TYPE_POINTER &&
            parameter->type.typeKind != ZR_FFI_CONTRACT_TYPE_CALLBACK) {
            /* Transfer/shared is an escape contract.  A scalar or inline
             * aggregate has no object identity for the lease to retain, so
             * silently copying it would make the ownership promise
             * unverifiable.  Reject rather than publish a false lifetime
             * witness. */
            native_call_set_diagnostic(
                    diagnostic, ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
                    ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                    ZR_FFI_CONTRACT_TYPE_POINTER,
                    parameter->type.typeKind);
            goto prepare_fail;
        }
        op->abiClass = abiClass;
        op->direction = parameter->direction;
        op->ownership = parameter->ownership;
        op->parameterIndex = index;
        op->byteSize = parameter->type.size;
        op->byteAlignment = native_call_safe_type_alignment(
                contract, &parameter->type);
        op->canonicalTypeHash = parameter->type.canonicalTypeHash;
        op->layoutHash = parameter->type.layoutHash != 0u
                                 ? parameter->type.layoutHash
                                 : parameter->type.canonicalTypeHash;
        /* A parameter with a non-natural storage alignment can still be
         * represented safely by an aligned temporary.  Keep the strict
         * alignment witness for the direct predicate below, but do not
         * reject the whole contract when COPY/BRIDGE can repair it. */
        if (!native_call_type_abi_alignment_valid(
                    parameter->type.typeKind,
                    parameter->type.size,
                    parameter->type.alignment,
                    contract->signature.targetPointerSize)) {
            canDirect = ZR_FALSE;
        }
        if (parameter->isNullable) {
            op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_NULLABLE;
        }
        if (marshalling == ZR_FFI_CONTRACT_MARSHALLING_REGISTERED &&
            abiClass != ZR_NATIVE_CALL_ABI_CLASS_CALLBACK &&
            abiClass != ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW &&
            abiClass != ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY &&
            !(abiClass == ZR_NATIVE_CALL_ABI_CLASS_VARARGS &&
              parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_POINTER)) {
            native_call_set_diagnostic(diagnostic,
                                       ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
                                       ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                                       ZR_FFI_CONTRACT_MARSHALLING_REGISTERED,
                                       abiClass);
            goto prepare_fail;
        }
        if (marshalling == ZR_FFI_CONTRACT_MARSHALLING_PIN &&
            abiClass != ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW &&
            abiClass != ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY &&
            !(abiClass == ZR_NATIVE_CALL_ABI_CLASS_VARARGS &&
              parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_POINTER)) {
            native_call_set_diagnostic(diagnostic,
                                       ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
                                       ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                                       ZR_FFI_CONTRACT_MARSHALLING_PIN,
                                       abiClass);
            goto prepare_fail;
        }
        if (marshalling == ZR_FFI_CONTRACT_MARSHALLING_COPY &&
            abiClass == ZR_NATIVE_CALL_ABI_CLASS_CALLBACK) {
            native_call_set_diagnostic(diagnostic,
                                       ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING,
                                       ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                                       ZR_FFI_CONTRACT_MARSHALLING_COPY,
                                       abiClass);
            goto prepare_fail;
        }
        if (parameter->ownership != ZR_FFI_CONTRACT_OWNERSHIP_BORROWED &&
            parameter->ownership != ZR_FFI_CONTRACT_OWNERSHIP_PINNED) {
            canDirect = ZR_FALSE;
        }
        if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_CALLBACK) {
            if (request->callbackId == 0u) {
                native_call_set_diagnostic(diagnostic,
                                           ZR_NATIVE_CALL_STATUS_CALLBACK_UNRESOLVED,
                                           ZR_NATIVE_CALL_PHASE_VALIDATE, index,
                                           1u, request->callbackId);
                goto prepare_fail;
            }
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK |
                           ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE |
                           ZR_NATIVE_CALL_PLAN_FLAG_CALLBACK_RELOAD |
                           ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
            if (contract->signature.callbackThreadPolicy ==
                    ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH) {
                plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH;
            }
            /* The registry protects the process-local code address, while
             * this root protects the managed callback context across GC and
             * nested VM calls. */
            op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT;
            op->marshalKind = marshalling == ZR_FFI_CONTRACT_MARSHALLING_REGISTERED
                                      ? ZR_NATIVE_CALL_MARSHAL_REGISTERED
                                      : ZR_NATIVE_CALL_MARSHAL_BRIDGE;
        } else if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_REF_OUT) {
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_HAS_REF_OUT |
                           ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_WRITEBACK |
                           ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
            op->marshalKind = ZR_NATIVE_CALL_MARSHAL_COPY;
            op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT |
                         ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_WRITEBACK;
        } else if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_VARARGS) {
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE;
            op->marshalKind = ZR_NATIVE_CALL_MARSHAL_BRIDGE;
            /* A pointer in a variadic bridge carries no typed tail metadata;
             * retain its managed owner for the duration of argument
             * lowering.  The bridge still owns the temporary bytes. */
            if (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_POINTER) {
                plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
                op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT;
            }
        } else if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW ||
                   abiClass == ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY) {
            if (marshalling == ZR_FFI_CONTRACT_MARSHALLING_REGISTERED) {
                op->marshalKind = ZR_NATIVE_CALL_MARSHAL_REGISTERED;
                plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
                op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT;
            } else if (marshalling == ZR_FFI_CONTRACT_MARSHALLING_COPY ||
                       !canDirect) {
                op->marshalKind = ZR_NATIVE_CALL_MARSHAL_COPY;
            } else {
                op->marshalKind = ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT;
            }
            if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) {
                plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN;
                op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN;
            } else {
                plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
                op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT;
            }
        } else if (abiClass == ZR_NATIVE_CALL_ABI_CLASS_INLINE_STRUCT) {
            if ((parameter->type.flags & ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE) == 0u ||
                marshalling == ZR_FFI_CONTRACT_MARSHALLING_COPY ||
                !native_call_aggregate_direct_layout_compatible(
                        contract, &parameter->type)) {
                canDirect = ZR_FALSE;
            }
            op->marshalKind = canDirect ? ZR_NATIVE_CALL_MARSHAL_DIRECT
                                         : ZR_NATIVE_CALL_MARSHAL_COPY;
        } else {
            if (marshalling == ZR_FFI_CONTRACT_MARSHALLING_COPY ||
                (parameter->type.flags &
                 ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE) == 0u) {
                canDirect = ZR_FALSE;
            }
            op->marshalKind = canDirect ? ZR_NATIVE_CALL_MARSHAL_DIRECT
                                         : ZR_NATIVE_CALL_MARSHAL_COPY;
        }
        /* Variadic calls always use the bridge, including fixed arguments;
         * a C ABI does not expose a safe direct lane for an unknown tail. */
        if (contract->signature.isVariadic &&
            abiClass != ZR_NATIVE_CALL_ABI_CLASS_CALLBACK) {
            if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_DIRECT) {
                op->marshalKind = ZR_NATIVE_CALL_MARSHAL_BRIDGE;
            } else if (op->marshalKind ==
                               ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) {
                /* A variadic tail has no stable typed-pointer ABI.  Drop a
                 * speculative pin/direct lane and copy into bridge storage;
                 * the root obligation keeps the source object alive until
                 * the bridge has consumed it. */
                op->marshalKind = ZR_NATIVE_CALL_MARSHAL_COPY;
                op->flags &= (TZrUInt32)~ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN;
                op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT;
                plan->flags &= (TZrUInt32)~ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN;
                plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
            }
        }
        /* Transfer/shared ownership is an escape from the call.  Keep the
         * managed value rooted and use a temporary even when the byte layout
         * itself happens to be compatible.  A REGISTERED lane is already
         * provider-owned stable storage, so it is the one explicit escape
         * witness that does not need another temporary. */
        if ((parameter->ownership == ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER ||
             parameter->ownership == ZR_FFI_CONTRACT_OWNERSHIP_SHARED) &&
            (abiClass == ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW ||
             abiClass == ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY) &&
            op->marshalKind != ZR_NATIVE_CALL_MARSHAL_REGISTERED) {
            op->marshalKind = ZR_NATIVE_CALL_MARSHAL_COPY;
            op->flags &= (TZrUInt32)~ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN;
            op->flags |= ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT;
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
        }
        if (op->marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY ||
            op->marshalKind == ZR_NATIVE_CALL_MARSHAL_BRIDGE ||
            op->marshalKind == ZR_NATIVE_CALL_MARSHAL_REGISTERED) {
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE;
        }
        if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN) != 0u) {
            plan->pinCount++;
        }
        if ((op->flags & ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) != 0u) {
            plan->rootCount++;
        }
    }
    /* A lane may be downgraded after its initial classification (for
     * example, transfer/shared ownership turns a pinned direct view into a
     * copied bridge).  Recompute the aggregate lifetime flags from the final
     * operations so no stale pin/root obligation survives that downgrade. */
    plan->pinCount = 0u;
    plan->rootCount = 0u;
    plan->flags &= (TZrUInt32)~(ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN |
                                ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT);
    for (index = 0u; index < plan->marshalOpCount; index++) {
        if ((plan->marshalOps[index].flags &
             ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_PIN) != 0u) {
            plan->pinCount++;
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_PIN;
        }
        if ((plan->marshalOps[index].flags &
             ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) != 0u) {
            plan->rootCount++;
            plan->flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ROOT;
        }
    }
    if ((plan->nativeSafepointMode == ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED &&
         (plan->pinCount != 0u || plan->rootCount != 0u ||
          (plan->flags & (ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK |
                          ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH)) != 0u)) ||
        (plan->nativeSafepointMode == ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL &&
         (plan->pinCount != 0u || plan->rootCount != 0u ||
          (plan->flags & (ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK |
                          ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_ATTACH)) != 0u))) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_THREAD_POLICY,
                                   ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                                   plan->nativeSafepointMode, plan->flags);
        goto prepare_fail;
    }
    /* A variadic signature has an untyped tail even when there are no fixed
     * parameters.  Keep the cache on the bridge lane rather than publishing
     * an (otherwise vacuously) direct-compatible zero-argument plan. */
    plan->directCompatible = (TZrBool)!plan->isVariadic;
    for (index = 0u; index < plan->marshalOpCount; index++) {
        if (plan->marshalOps[index].marshalKind != ZR_NATIVE_CALL_MARSHAL_DIRECT &&
            plan->marshalOps[index].marshalKind != ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT) {
            plan->directCompatible = ZR_FALSE;
            break;
        }
    }
    plan->requiresBridge = (TZrBool)((plan->flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE) != 0u);
    plan->valid = ZR_TRUE;
    plan->planHash = native_call_plan_hash(plan);
    if (!ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        goto prepare_fail;
    }
    return ZR_TRUE;

prepare_fail:
    memset(plan, 0, sizeof(*plan));
    return ZR_FALSE;
}

TZrBool ZrCore_NativeCall_InvokeResolved(
        SZrState *state,
        const SZrNativeCallPlan *plan,
        FZrNativeCallResolvedInvoker invoker,
        TZrPtr userData,
        SZrNativeCallDiagnostic *diagnostic) {
    SZrNativeCallLease lease;
    EZrNativeCallStatus status;

    /* LeaseBegin preserves an already-active lease so callers can report a
     * duplicate begin without leaking pins/roots.  The resolver-owned lease
     * is always fresh here; initialize it before handing it to the lifetime
     * API rather than reading an indeterminate active bit. */
    memset(&lease, 0, sizeof(lease));
    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (plan == ZR_NULL) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        return ZR_FALSE;
    }
    if (state == ZR_NULL) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (invoker == ZR_NULL) {
        native_call_set_diagnostic(diagnostic, ZR_NATIVE_CALL_STATUS_INVOKE_UNRESOLVED,
                                   ZR_NATIVE_CALL_PHASE_INVOKE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_NativeCall_LeaseBegin(state, plan, &lease, diagnostic)) {
        return ZR_FALSE;
    }
    status = invoker(state, plan, &lease, userData, diagnostic);
    if (diagnostic != ZR_NULL && diagnostic->sourceId == 0u) {
        /* A resolver is allowed to clear/fill only the fields it owns.  Keep
         * the plan's import identity as the fallback attribution when it
         * returns a bare status. */
        diagnostic->sourceId = plan->sourceId;
    }
    if (status == ZR_NATIVE_CALL_STATUS_OK &&
        lease.pinObligationCount < lease.requiredPinCount) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, UINT32_MAX,
                lease.requiredPinCount, lease.pinObligationCount);
        status = ZR_NATIVE_CALL_STATUS_PIN_FAILED;
    }
    if (status == ZR_NATIVE_CALL_STATUS_OK &&
        lease.rootObligationCount < lease.requiredRootCount) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ROOT_FAILED,
                ZR_NATIVE_CALL_PHASE_ROOT, UINT32_MAX,
                lease.requiredRootCount, lease.rootObligationCount);
        status = ZR_NATIVE_CALL_STATUS_ROOT_FAILED;
    }
    /* Keep the invoker's diagnostic details while still releasing every
     * lease resource on success and failure. */
    ZrCore_NativeCall_LeaseEnd(&lease, ZR_NULL);
    if ((TZrUInt32)status >
            (TZrUInt32)ZR_NATIVE_CALL_STATUS_INTERNAL) {
        native_call_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVOKE_FAILED,
                ZR_NATIVE_CALL_PHASE_INVOKE,
                UINT32_MAX,
                ZR_NATIVE_CALL_STATUS_INTERNAL,
                (TZrUInt32)status);
        return ZR_FALSE;
    }
    if (status != ZR_NATIVE_CALL_STATUS_OK) {
        native_call_set_terminal_status(
                diagnostic,
                status,
                status == ZR_NATIVE_CALL_STATUS_EXCEPTION_TRANSLATION
                        ? ZR_NATIVE_CALL_PHASE_TRANSLATE_EXCEPTION
                        : ZR_NATIVE_CALL_PHASE_INVOKE);
        return ZR_FALSE;
    }
    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    return ZR_TRUE;
}
