#include "backend_aot_ir_adapter.h"

#include <string.h>

static void backend_aot_ir_adapter_clear_diagnostic(
        SZrBackendAotIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_BACKEND_AOT_IR_OK;
    }
}

static void backend_aot_ir_adapter_clear_facts(
        SZrBackendAotIrFacts *facts) {
    if (facts != ZR_NULL) {
        (void)memset(facts, 0, sizeof(*facts));
        facts->descriptorOnly = ZR_TRUE;
    }
}

static EZrBackendAotIrStatus backend_aot_ir_adapter_map_status(
        EZrAotIrStatus status) {
    switch (status) {
        case ZR_AOT_IR_OK:
            return ZR_BACKEND_AOT_IR_OK;
        case ZR_AOT_IR_RELOCATION:
            return ZR_BACKEND_AOT_IR_RELOCATION_PRESENT;
        case ZR_AOT_IR_UNSUPPORTED:
            return ZR_BACKEND_AOT_IR_UNSUPPORTED;
        case ZR_AOT_IR_INVALID_TARGET:
            return ZR_BACKEND_AOT_IR_TARGET_MISMATCH;
        case ZR_AOT_IR_INVALID_ARGUMENT:
            return ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        case ZR_AOT_IR_INVALID_CONTRACT:
        case ZR_AOT_IR_VERSION_MISMATCH:
        case ZR_AOT_IR_INVALID_ID:
        case ZR_AOT_IR_DUPLICATE_ID:
        case ZR_AOT_IR_INVALID_RANGE:
        case ZR_AOT_IR_INVALID_CFG:
        case ZR_AOT_IR_INVALID_OPCODE:
        case ZR_AOT_IR_INVALID_EFFECT:
        case ZR_AOT_IR_INVALID_LAYOUT:
        case ZR_AOT_IR_INVALID_SIGNATURE:
        default:
            return ZR_BACKEND_AOT_IR_INVALID_AOTIR;
    }
}

static void backend_aot_ir_adapter_copy_diagnostic(
        SZrBackendAotIrDiagnostic *diagnostic,
        EZrAotIrStatus sourceStatus,
        const SZrAotIrDiagnostic *source) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->status = backend_aot_ir_adapter_map_status(sourceStatus);
    diagnostic->aotIrStatus = sourceStatus;
    if (source != ZR_NULL) {
        diagnostic->functionId = source->functionId;
        diagnostic->blockId = source->blockId;
        diagnostic->instructionId = source->instructionId;
        diagnostic->index = source->index;
        diagnostic->expected = source->expected;
        diagnostic->actual = source->actual;
    }
}

static TZrBool backend_aot_ir_adapter_bool_valid(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static TZrBool backend_aot_ir_adapter_target_valid(
        EZrAotIrEmitterTarget target) {
    return (TZrBool)(target == ZR_AOT_IR_EMITTER_C ||
                     target == ZR_AOT_IR_EMITTER_LLVM);
}

static TZrUInt64 backend_aot_ir_adapter_hash_u32(TZrUInt64 hash,
                                                  TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; ++index) {
        hash = (hash ^ (TZrUInt8)(value >> (index * 8u))) *
               UINT64_C(1099511628211);
    }
    return hash;
}

static TZrUInt64 backend_aot_ir_adapter_hash_u64(TZrUInt64 hash,
                                                  TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; ++index) {
        hash = (hash ^ (TZrUInt8)(value >> (index * 8u))) *
               UINT64_C(1099511628211);
    }
    return hash;
}

const TZrChar *backend_aot_ir_adapter_status_name(
        EZrBackendAotIrStatus status) {
    switch (status) {
        case ZR_BACKEND_AOT_IR_OK: return "ok";
        case ZR_BACKEND_AOT_IR_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_BACKEND_AOT_IR_INVALID_AOTIR: return "invalid-aotir";
        case ZR_BACKEND_AOT_IR_RELOCATION_PRESENT: return "relocation-present";
        case ZR_BACKEND_AOT_IR_TARGET_MISMATCH: return "target-mismatch";
        case ZR_BACKEND_AOT_IR_UNSUPPORTED: return "unsupported";
        case ZR_BACKEND_AOT_IR_ARTIFACT_UNAVAILABLE: return "artifact-unavailable";
        case ZR_BACKEND_AOT_IR_CAPACITY: return "capacity";
        case ZR_BACKEND_AOT_IR_COVERAGE_UNAVAILABLE: return "coverage-unavailable";
        case ZR_BACKEND_AOT_IR_COVERAGE_INVALID: return "coverage-invalid";
        case ZR_BACKEND_AOT_IR_PROFILE_MISMATCH: return "profile-mismatch";
        case ZR_BACKEND_AOT_IR_TOOLCHAIN_UNSUPPORTED: return "toolchain-unsupported";
        case ZR_BACKEND_AOT_IR_FALLBACK: return "fallback";
        case ZR_BACKEND_AOT_IR_STATUS_COUNT:
        default: return "unknown";
    }
}

EZrBackendAotIrStatus backend_aot_ir_adapter_validate(
        const SZrAotIrModule *module,
        SZrBackendAotIrDiagnostic *diagnostic) {
    SZrAotIrDiagnostic sourceDiagnostic;
    EZrAotIrStatus sourceStatus;

    backend_aot_ir_adapter_clear_diagnostic(diagnostic);
    if (module == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
    }

    (void)memset(&sourceDiagnostic, 0, sizeof(sourceDiagnostic));
    sourceStatus = ZrCore_AotIr_ValidateModule(module, &sourceDiagnostic);
    if (sourceStatus != ZR_AOT_IR_OK) {
        backend_aot_ir_adapter_copy_diagnostic(diagnostic, sourceStatus,
                                                &sourceDiagnostic);
        return backend_aot_ir_adapter_map_status(sourceStatus);
    }
    (void)memset(&sourceDiagnostic, 0, sizeof(sourceDiagnostic));
    if (!ZrCore_AotIr_IsRelocationFree(module, &sourceDiagnostic)) {
        backend_aot_ir_adapter_copy_diagnostic(diagnostic,
                                               ZR_AOT_IR_RELOCATION,
                                               &sourceDiagnostic);
        return ZR_BACKEND_AOT_IR_RELOCATION_PRESENT;
    }
    return ZR_BACKEND_AOT_IR_OK;
}

TZrBool backend_aot_ir_adapter_count_instructions(
        const SZrAotIrModule *module,
        TZrUInt32 *outCount,
        SZrBackendAotIrDiagnostic *diagnostic) {
    TZrUInt32 count = 0u;
    EZrBackendAotIrStatus status;

    backend_aot_ir_adapter_clear_diagnostic(diagnostic);
    if (outCount != ZR_NULL) {
        *outCount = 0u;
    }
    if (outCount == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    status = backend_aot_ir_adapter_validate(module, diagnostic);
    if (status != ZR_BACKEND_AOT_IR_OK) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < module->functionCount; ++index) {
        TZrUInt32 instructionCount = module->functions[index].instructionCount;
        if (instructionCount > UINT32_MAX - count) {
            if (diagnostic != ZR_NULL) {
                diagnostic->status = ZR_BACKEND_AOT_IR_CAPACITY;
                diagnostic->functionId = module->functions[index].id;
                diagnostic->expected = UINT32_MAX;
                diagnostic->actual = (TZrUInt64)count + instructionCount;
            }
            return ZR_FALSE;
        }
        count += instructionCount;
    }
    *outCount = count;
    return ZR_TRUE;
}

TZrBool backend_aot_ir_adapter_collect(
        const SZrAotIrModule *module,
        SZrAotIrLoweringRecord *records,
        TZrUInt32 capacity,
        SZrAotIrLoweringResult *outLowering,
        SZrBackendAotIrDiagnostic *diagnostic) {
    TZrUInt32 required = 0u;
    SZrAotIrDiagnostic sourceDiagnostic;

    backend_aot_ir_adapter_clear_diagnostic(diagnostic);
    if (outLowering != ZR_NULL) {
        (void)memset(outLowering, 0, sizeof(*outLowering));
    }
    if (outLowering == ZR_NULL ||
        (capacity != 0u && records == ZR_NULL) ||
        !backend_aot_ir_adapter_count_instructions(module, &required,
                                                    diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->status == ZR_BACKEND_AOT_IR_OK) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (required > capacity) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_CAPACITY;
            diagnostic->expected = required;
            diagnostic->actual = capacity;
        }
        return ZR_FALSE;
    }
    outLowering->records = records;
    outLowering->capacity = capacity;
    (void)memset(&sourceDiagnostic, 0, sizeof(sourceDiagnostic));
    if (!ZrParser_AotIr_LowerShared(module, outLowering,
                                    &sourceDiagnostic)) {
        backend_aot_ir_adapter_copy_diagnostic(diagnostic,
                                               sourceDiagnostic.status,
                                               &sourceDiagnostic);
        return ZR_FALSE;
    }
    if (!ZrParser_AotIr_LoweringIsPointerFree(outLowering)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_AOTIR;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool backend_aot_ir_adapter_facts_from_lowering(
        const SZrAotIrModule *module,
        EZrAotIrEmitterTarget target,
        const SZrAotIrEmitOptions *options,
        const SZrAotIrLoweringResult *lowering,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    TZrUInt32 instructionCount = 0u;
    TZrUInt64 contractHash;

    backend_aot_ir_adapter_clear_diagnostic(diagnostic);
    backend_aot_ir_adapter_clear_facts(outFacts);
    if (outFacts == ZR_NULL || options == ZR_NULL ||
        !backend_aot_ir_adapter_target_valid(target) ||
        options->target != target ||
        !backend_aot_ir_adapter_bool_valid(options->strictFloatingPoint) ||
        !backend_aot_ir_adapter_bool_valid(options->allowRuntimeBridge) ||
        !backend_aot_ir_adapter_bool_valid(options->allowInterpreterFallback) ||
        lowering == ZR_NULL ||
        lowering->count > lowering->capacity ||
        !backend_aot_ir_adapter_count_instructions(module, &instructionCount,
                                                   diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->status == ZR_BACKEND_AOT_IR_OK) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (lowering->count != instructionCount || lowering->sourceHash == 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_AOTIR;
            diagnostic->expected = instructionCount;
            diagnostic->actual = lowering->count;
        }
        return ZR_FALSE;
    }
    /* A caller may provide a hand-built lowering result instead of using
     * backend_aot_ir_adapter_collect().  Re-check the pointer-free view at
     * this boundary so no dangling record array can be treated as facts. */
    if (!ZrParser_AotIr_LoweringIsPointerFree(lowering)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_AOTIR;
            diagnostic->expected = lowering->count;
            diagnostic->actual = 0u;
        }
        return ZR_FALSE;
    }

    outFacts->target = target;
    outFacts->descriptorOnly = ZR_TRUE;
    outFacts->artifactAvailable = ZR_FALSE;
    outFacts->instructionCount = instructionCount;
    outFacts->semanticSiteCount = instructionCount;
    outFacts->moduleHash = module->moduleHash;
    outFacts->sourceHash = lowering->sourceHash;

    for (TZrUInt32 index = 0u; index < lowering->count; ++index) {
        const SZrAotIrLoweringRecord *record = &lowering->records[index];
        switch (record->kind) {
            case ZR_AOT_IR_LOWERING_TYPED_SCALAR:
            case ZR_AOT_IR_LOWERING_CONTROL:
            case ZR_AOT_IR_LOWERING_CALL:
            case ZR_AOT_IR_LOWERING_MEMBER_BRIDGE:
            case ZR_AOT_IR_LOWERING_INLINE_LAYOUT:
                outFacts->nativeLoweredCount++;
                break;
            case ZR_AOT_IR_LOWERING_CONTAINER_BRIDGE:
            case ZR_AOT_IR_LOWERING_ASYNC_BOUNDARY:
            case ZR_AOT_IR_LOWERING_RUNTIME_BRIDGE:
                if (!options->allowRuntimeBridge) {
                    if (diagnostic != ZR_NULL) {
                        diagnostic->status = ZR_BACKEND_AOT_IR_UNSUPPORTED;
                        diagnostic->functionId = record->functionId;
                        diagnostic->instructionId = record->instructionId;
                        diagnostic->sourceId = record->sourceId;
                    }
                    (void)memset(outFacts, 0, sizeof(*outFacts));
                    return ZR_FALSE;
                }
                outFacts->runtimeBridgeCount++;
                break;
            case ZR_AOT_IR_LOWERING_UNSUPPORTED:
            default:
                outFacts->unsupportedCount++;
                if (!options->allowInterpreterFallback) {
                    if (diagnostic != ZR_NULL) {
                        diagnostic->status = ZR_BACKEND_AOT_IR_UNSUPPORTED;
                        diagnostic->functionId = record->functionId;
                        diagnostic->instructionId = record->instructionId;
                        diagnostic->sourceId = record->sourceId;
                    }
                    (void)memset(outFacts, 0, sizeof(*outFacts));
                    return ZR_FALSE;
                }
                outFacts->interpreterFallbackCount++;
                break;
        }
    }
    outFacts->fallbackUsed = (TZrBool)(outFacts->interpreterFallbackCount != 0u);
    contractHash = UINT64_C(1469598103934665603);
    contractHash = backend_aot_ir_adapter_hash_u64(contractHash,
                                                   module->moduleHash);
    contractHash = backend_aot_ir_adapter_hash_u64(contractHash,
                                                   lowering->sourceHash);
    contractHash = backend_aot_ir_adapter_hash_u64(contractHash,
                                                   lowering->loweringHash);
    contractHash = backend_aot_ir_adapter_hash_u32(contractHash,
                                                   (TZrUInt32)target);
    contractHash = backend_aot_ir_adapter_hash_u32(
            contractHash, (TZrUInt32)options->strictFloatingPoint);
    contractHash = backend_aot_ir_adapter_hash_u32(
            contractHash, outFacts->nativeLoweredCount);
    contractHash = backend_aot_ir_adapter_hash_u32(
            contractHash, outFacts->runtimeBridgeCount);
    contractHash = backend_aot_ir_adapter_hash_u32(
            contractHash, outFacts->interpreterFallbackCount);
    contractHash = backend_aot_ir_adapter_hash_u32(
            contractHash, outFacts->unsupportedCount);
    outFacts->contractHash = contractHash;
    return ZR_TRUE;
}

TZrBool backend_aot_ir_adapter_emit_target(
        const SZrAotIrModule *module,
        EZrAotIrEmitterTarget target,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    SZrAotIrEmitResult emitted;
    SZrAotIrDiagnostic sourceDiagnostic;
    TZrUInt32 instructionCount = 0u;
    TZrUInt32 accounted;

    backend_aot_ir_adapter_clear_diagnostic(diagnostic);
    backend_aot_ir_adapter_clear_facts(outFacts);
    if (outFacts == ZR_NULL || options == ZR_NULL ||
        !backend_aot_ir_adapter_target_valid(target) ||
        options->target != target ||
        !backend_aot_ir_adapter_bool_valid(options->strictFloatingPoint) ||
        !backend_aot_ir_adapter_bool_valid(options->allowRuntimeBridge) ||
        !backend_aot_ir_adapter_bool_valid(options->allowInterpreterFallback) ||
        !backend_aot_ir_adapter_count_instructions(module, &instructionCount,
                                                   diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->status == ZR_BACKEND_AOT_IR_OK) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }

    (void)memset(&emitted, 0, sizeof(emitted));
    (void)memset(&sourceDiagnostic, 0, sizeof(sourceDiagnostic));
    if (target == ZR_AOT_IR_EMITTER_C) {
        if (!ZrParser_AotIr_EmitC(module, options, &emitted,
                                  &sourceDiagnostic)) {
            backend_aot_ir_adapter_copy_diagnostic(
                    diagnostic, sourceDiagnostic.status, &sourceDiagnostic);
            if (diagnostic != ZR_NULL &&
                diagnostic->status == ZR_BACKEND_AOT_IR_OK) {
                diagnostic->status = ZR_BACKEND_AOT_IR_UNSUPPORTED;
            }
            return ZR_FALSE;
        }
    } else {
        if (!ZrParser_AotIr_EmitLlvm(module, options, &emitted,
                                     &sourceDiagnostic)) {
            backend_aot_ir_adapter_copy_diagnostic(
                    diagnostic, sourceDiagnostic.status, &sourceDiagnostic);
            if (diagnostic != ZR_NULL &&
                diagnostic->status == ZR_BACKEND_AOT_IR_OK) {
                diagnostic->status = ZR_BACKEND_AOT_IR_UNSUPPORTED;
            }
            return ZR_FALSE;
        }
    }
    if (emitted.nativeCount > instructionCount ||
        emitted.interpreterFallbackCount > instructionCount - emitted.nativeCount) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_AOTIR;
            diagnostic->expected = instructionCount;
            diagnostic->actual = emitted.nativeCount;
        }
        return ZR_FALSE;
    }
    /* `unsupportedCount` is a static annotation.  When fallback is allowed,
     * the same operation is also counted in interpreterFallbackCount, so it
     * must not be subtracted twice from the semantic denominator. */
    accounted = emitted.nativeCount + emitted.interpreterFallbackCount;
    backend_aot_ir_adapter_clear_facts(outFacts);
    outFacts->target = target;
    outFacts->descriptorOnly = ZR_TRUE;
    outFacts->artifactAvailable = ZR_FALSE;
    outFacts->fallbackUsed = (TZrBool)(emitted.interpreterFallbackCount != 0u);
    outFacts->instructionCount = instructionCount;
    outFacts->semanticSiteCount = instructionCount;
    outFacts->nativeLoweredCount = emitted.nativeCount;
    /* The parser facade's historical runtimeBridgeCount includes a temporary
     * compatibility count.  Derive the canonical value from the complete
     * semantic denominator so a bridge is never counted twice. */
    outFacts->runtimeBridgeCount = instructionCount - accounted;
    outFacts->interpreterFallbackCount = emitted.interpreterFallbackCount;
    outFacts->unsupportedCount = emitted.unsupportedCount;
    outFacts->moduleHash = module->moduleHash;
    outFacts->sourceHash = emitted.sourceHash;
    outFacts->contractHash = emitted.contractHash;
    return ZR_TRUE;
}
