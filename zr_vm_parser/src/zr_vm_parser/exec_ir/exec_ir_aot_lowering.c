#include "zr_vm_parser/aot_ir_lowering.h"

#include <stdlib.h>
#include <string.h>

static TZrUInt64 lowering_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    for (TZrUInt32 i = 0u; i < 4u; ++i) {
        hash = (hash ^ (TZrUInt8)(value >> (i * 8u))) * UINT64_C(1099511628211);
    }
    return hash;
}

static TZrBool lowering_bool_valid(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static EZrAotIrLoweringKind lowering_kind(TZrUInt32 opcode) {
    switch ((EZrExecIrOpcode)opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
        case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV:
        case ZR_EXEC_IR_OPCODE_NEG:
        case ZR_EXEC_IR_OPCODE_COMPARE:
            return ZR_AOT_IR_LOWERING_TYPED_SCALAR;
        case ZR_EXEC_IR_OPCODE_BRANCH:
        case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
        case ZR_EXEC_IR_OPCODE_SWITCH:
        case ZR_EXEC_IR_OPCODE_RETURN:
        case ZR_EXEC_IR_OPCODE_THROW:
            return ZR_AOT_IR_LOWERING_CONTROL;
        case ZR_EXEC_IR_OPCODE_CALL:
        case ZR_EXEC_IR_OPCODE_INVOKE:
            return ZR_AOT_IR_LOWERING_CALL;
        case ZR_EXEC_IR_OPCODE_PLACE_BASE:
        case ZR_EXEC_IR_OPCODE_PLACE_PROJECT:
            return ZR_AOT_IR_LOWERING_MEMBER_BRIDGE;
        case ZR_EXEC_IR_OPCODE_LOAD:
        case ZR_EXEC_IR_OPCODE_STORE:
        case ZR_EXEC_IR_OPCODE_ALLOC:
        case ZR_EXEC_IR_OPCODE_BARRIER:
        case ZR_EXEC_IR_OPCODE_DROP:
            return ZR_AOT_IR_LOWERING_CONTAINER_BRIDGE;
        case ZR_EXEC_IR_OPCODE_SUSPEND:
            return ZR_AOT_IR_LOWERING_ASYNC_BOUNDARY;
        case ZR_EXEC_IR_OPCODE_PHI:
            return ZR_AOT_IR_LOWERING_INLINE_LAYOUT;
        case ZR_EXEC_IR_OPCODE_NOP:
        default:
            return ZR_AOT_IR_LOWERING_RUNTIME_BRIDGE;
    }
}

TZrBool ZrParser_AotIr_LowerShared(const SZrAotIrModule *module,
                                   SZrAotIrLoweringResult *result,
                                   SZrAotIrDiagnostic *diagnostic) {
    TZrUInt64 sourceHash;
    TZrUInt64 loweringHash = UINT64_C(1469598103934665603);
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    result->count = 0u;
    result->unsupportedCount = 0u;
    result->runtimeBridgeCount = 0u;
    result->sourceHash = 0u;
    result->loweringHash = 0u;
    if (module == ZR_NULL || result->records == ZR_NULL || result->capacity == 0u) {
        return ZR_FALSE;
    }
    if (ZrCore_AotIr_ValidateModule(module, diagnostic) != ZR_AOT_IR_OK ||
        !ZrCore_AotIr_IsRelocationFree(module, diagnostic)) {
        return ZR_FALSE;
    }
    sourceHash = ZrCore_AotIr_HashModule(module);
    if (sourceHash == 0u) {
        return ZR_FALSE;
    }
    for (TZrUInt32 f = 0u; f < module->functionCount; ++f) {
        const SZrAotIrFunction *function = &module->functions[f];
        for (TZrUInt32 i = 0u; i < function->instructionCount; ++i) {
            const SZrAotIrInstruction *instruction = &function->instructions[i];
            EZrAotIrLoweringKind kind = lowering_kind(instruction->opcode);
            if (result->count == result->capacity) {
                if (diagnostic != ZR_NULL) {
                    diagnostic->status = ZR_AOT_IR_INVALID_ARGUMENT;
                    diagnostic->functionId = function->id;
                    diagnostic->instructionId = instruction->id;
                    diagnostic->expected = result->capacity;
                    diagnostic->actual = result->count + 1u;
                }
                result->count = 0u;
                return ZR_FALSE;
            }
            result->records[result->count++] = (SZrAotIrLoweringRecord){
                function->id, instruction->id, instruction->opcode,
                instruction->sourceId, instruction->bindingRow,
                instruction->layoutId, instruction->flags, kind};
            loweringHash = lowering_hash_u32(loweringHash, function->id);
            loweringHash = lowering_hash_u32(loweringHash, instruction->id);
            loweringHash = lowering_hash_u32(loweringHash, instruction->opcode);
            loweringHash = lowering_hash_u32(loweringHash, (TZrUInt32)kind);
            if (kind == ZR_AOT_IR_LOWERING_RUNTIME_BRIDGE) {
                result->runtimeBridgeCount++;
            }
            if (kind == ZR_AOT_IR_LOWERING_UNSUPPORTED) {
                result->unsupportedCount++;
            }
        }
    }
    result->sourceHash = sourceHash;
    result->loweringHash = loweringHash;
    return ZR_TRUE;
}

TZrBool ZrParser_AotIr_LoweringIsPointerFree(const SZrAotIrLoweringResult *result) {
    return (TZrBool)(result != ZR_NULL &&
                     (result->count == 0u || result->records != ZR_NULL));
}

static TZrUInt64 emit_hash_mix(TZrUInt64 hash, TZrUInt64 value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

static TZrBool aot_ir_emit_target(const SZrAotIrModule *module,
                                  const SZrAotIrEmitOptions *options,
                                  EZrAotIrEmitterTarget target,
                                  SZrAotIrEmitResult *result,
                                  SZrAotIrDiagnostic *diagnostic) {
    SZrAotIrLoweringRecord *records;
    SZrAotIrLoweringResult lowering;
    TZrUInt32 capacity = 0u;
    TZrUInt64 contractHash;
    if (result == ZR_NULL || module == ZR_NULL || module->functions == ZR_NULL ||
        module->functionCount == 0u || options == ZR_NULL ||
        !lowering_bool_valid(options->strictFloatingPoint) ||
        !lowering_bool_valid(options->allowRuntimeBridge) ||
        !lowering_bool_valid(options->allowInterpreterFallback) ||
        options->target != target ||
        (target != ZR_AOT_IR_EMITTER_C && target != ZR_AOT_IR_EMITTER_LLVM)) {
        if (diagnostic != ZR_NULL) {
            memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->status = ZR_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    memset(result, 0, sizeof(*result));
    for (TZrUInt32 f = 0u; f < module->functionCount; ++f) {
        if (module->functions[f].instructionCount > UINT32_MAX - capacity) {
            if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_RANGE;
            return ZR_FALSE;
        }
        capacity += module->functions[f].instructionCount;
    }
    records = capacity == 0u ? ZR_NULL :
              (SZrAotIrLoweringRecord *)calloc(capacity, sizeof(*records));
    if (capacity != 0u && records == ZR_NULL) {
        if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    memset(&lowering, 0, sizeof(lowering));
    lowering.records = records; lowering.capacity = capacity;
    if (!ZrParser_AotIr_LowerShared(module, &lowering, diagnostic)) {
        free(records); return ZR_FALSE;
    }
    result->target = target;
    result->sourceHash = lowering.sourceHash;
    /* Count every bridge class exactly once below.  The shared lowering
     * result only tracks the generic runtime-bridge kind; copying that value
     * here would double-count those records when the target walk accounts for
     * async/container bridges as well. */
    result->runtimeBridgeCount = 0u;
    result->unsupportedCount = lowering.unsupportedCount;
    for (TZrUInt32 i = 0u; i < lowering.count; ++i) {
        const SZrAotIrLoweringRecord *record = &records[i];
        switch (record->kind) {
            case ZR_AOT_IR_LOWERING_TYPED_SCALAR:
            case ZR_AOT_IR_LOWERING_CONTROL:
            case ZR_AOT_IR_LOWERING_CALL:
            case ZR_AOT_IR_LOWERING_MEMBER_BRIDGE:
            case ZR_AOT_IR_LOWERING_INLINE_LAYOUT:
                result->nativeCount++;
                break;
            case ZR_AOT_IR_LOWERING_RUNTIME_BRIDGE:
            case ZR_AOT_IR_LOWERING_CONTAINER_BRIDGE:
            case ZR_AOT_IR_LOWERING_ASYNC_BOUNDARY:
                if (!options->allowRuntimeBridge) {
                    if (diagnostic != ZR_NULL) {
                        diagnostic->status = ZR_AOT_IR_UNSUPPORTED;
                        diagnostic->functionId = record->functionId;
                        diagnostic->instructionId = record->instructionId;
                    }
                    free(records); return ZR_FALSE;
                }
                result->runtimeBridgeCount++;
                break;
            case ZR_AOT_IR_LOWERING_UNSUPPORTED:
            default:
                if (!options->allowInterpreterFallback) {
                    if (diagnostic != ZR_NULL) {
                        diagnostic->status = ZR_AOT_IR_UNSUPPORTED;
                        diagnostic->functionId = record->functionId;
                        diagnostic->instructionId = record->instructionId;
                    }
                    free(records); return ZR_FALSE;
                }
                result->interpreterFallbackCount++;
                break;
        }
    }
    contractHash = emit_hash_mix(UINT64_C(1469598103934665603), module->moduleHash);
    contractHash = emit_hash_mix(contractHash, target);
    contractHash = emit_hash_mix(contractHash, options->strictFloatingPoint ? 1u : 0u);
    contractHash = emit_hash_mix(contractHash, result->sourceHash);
    contractHash = emit_hash_mix(contractHash, result->nativeCount);
    contractHash = emit_hash_mix(contractHash, result->runtimeBridgeCount);
    contractHash = emit_hash_mix(contractHash, result->interpreterFallbackCount);
    result->contractHash = contractHash;
    free(records);
    return ZR_TRUE;
}

TZrBool ZrParser_AotIr_EmitC(const SZrAotIrModule *module,
                             const SZrAotIrEmitOptions *options,
                             SZrAotIrEmitResult *result,
                             SZrAotIrDiagnostic *diagnostic) {
    return aot_ir_emit_target(module, options, ZR_AOT_IR_EMITTER_C,
                              result, diagnostic);
}

TZrBool ZrParser_AotIr_EmitLlvm(const SZrAotIrModule *module,
                                const SZrAotIrEmitOptions *options,
                                SZrAotIrEmitResult *result,
                                SZrAotIrDiagnostic *diagnostic) {
    return aot_ir_emit_target(module, options, ZR_AOT_IR_EMITTER_LLVM,
                              result, diagnostic);
}
