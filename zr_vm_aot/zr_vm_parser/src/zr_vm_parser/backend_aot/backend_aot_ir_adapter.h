#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_ADAPTER_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_ADAPTER_H

/*
 * Contract-only bridge between the shared ExecIR/AOTIR model and the dormant
 * AOT archive.  This header intentionally does not expose a function pointer,
 * executable address, or a legacy SZrInstruction view.  The records returned
 * by this layer are scalar facts and borrowed views whose lifetime is bounded
 * by the caller's AOTIR module.
 */

#include "zr_vm_parser/aot_ir_lowering.h"

typedef enum EZrBackendAotIrStatus {
    ZR_BACKEND_AOT_IR_OK = 0,
    ZR_BACKEND_AOT_IR_INVALID_ARGUMENT,
    ZR_BACKEND_AOT_IR_INVALID_AOTIR,
    ZR_BACKEND_AOT_IR_RELOCATION_PRESENT,
    ZR_BACKEND_AOT_IR_TARGET_MISMATCH,
    ZR_BACKEND_AOT_IR_UNSUPPORTED,
    ZR_BACKEND_AOT_IR_ARTIFACT_UNAVAILABLE,
    ZR_BACKEND_AOT_IR_CAPACITY,
    ZR_BACKEND_AOT_IR_COVERAGE_UNAVAILABLE,
    ZR_BACKEND_AOT_IR_COVERAGE_INVALID,
    ZR_BACKEND_AOT_IR_PROFILE_MISMATCH,
    ZR_BACKEND_AOT_IR_TOOLCHAIN_UNSUPPORTED,
    ZR_BACKEND_AOT_IR_FALLBACK,
    ZR_BACKEND_AOT_IR_STATUS_COUNT
} EZrBackendAotIrStatus;

typedef struct SZrBackendAotIrDiagnostic {
    EZrBackendAotIrStatus status;
    EZrAotIrStatus aotIrStatus;
    TZrUInt32 functionId;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrBackendAotIrDiagnostic;

/* Scalar emission facts.  `descriptorOnly` is always true for this archive
 * adapter; no source text, LLVM module, or machine code is produced here. */
typedef struct SZrBackendAotIrFacts {
    EZrAotIrEmitterTarget target;
    TZrBool descriptorOnly;
    TZrBool artifactAvailable;
    TZrBool fallbackUsed;
    TZrUInt32 instructionCount;
    TZrUInt32 semanticSiteCount;
    TZrUInt32 nativeLoweredCount;
    TZrUInt32 runtimeBridgeCount;
    TZrUInt32 interpreterFallbackCount;
    TZrUInt32 unsupportedCount;
    TZrUInt64 moduleHash;
    TZrUInt64 sourceHash;
    TZrUInt64 contractHash;
} SZrBackendAotIrFacts;

ZR_PARSER_API const TZrChar *backend_aot_ir_adapter_status_name(
        EZrBackendAotIrStatus status);

ZR_PARSER_API EZrBackendAotIrStatus backend_aot_ir_adapter_validate(
        const SZrAotIrModule *module,
        SZrBackendAotIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_adapter_count_instructions(
        const SZrAotIrModule *module,
        TZrUInt32 *outCount,
        SZrBackendAotIrDiagnostic *diagnostic);

/* Collects the shared lowering records into caller-owned storage.  No storage
 * is allocated and no pointer is copied into a persisted fact. */
ZR_PARSER_API TZrBool backend_aot_ir_adapter_collect(
        const SZrAotIrModule *module,
        SZrAotIrLoweringRecord *records,
        TZrUInt32 capacity,
        SZrAotIrLoweringResult *outLowering,
        SZrBackendAotIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_adapter_facts_from_lowering(
        const SZrAotIrModule *module,
        EZrAotIrEmitterTarget target,
        const SZrAotIrEmitOptions *options,
        const SZrAotIrLoweringResult *lowering,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/* Shared target driver used by the C and LLVM wrappers.  The underlying
 * parser facade only computes a descriptor/coverage result; it does not emit
 * source text, LLVM IR, or executable bytes. */
ZR_PARSER_API TZrBool backend_aot_ir_adapter_emit_target(
        const SZrAotIrModule *module,
        EZrAotIrEmitterTarget target,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_c_emit(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_c_emit_ex(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        TZrBool requireArtifact,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_llvm_emit(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_llvm_emit_ex(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        TZrBool requireArtifact,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/* Naming aliases used by older archive notes. */
static inline TZrBool backend_aot_ir_emit_c(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    return backend_aot_ir_c_emit(module, options, outFacts, diagnostic);
}

static inline TZrBool backend_aot_ir_emit_llvm(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    return backend_aot_ir_llvm_emit(module, options, outFacts, diagnostic);
}

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_ADAPTER_H */
