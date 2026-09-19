#include "zr_vm_parser/exec_ir_vectorize.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * This pass is deliberately a planner.  The fixed-width ExecIR schema has no
 * vector opcode or lane payload, so changing an instruction in place would
 * either lose scalar semantics or create an artifact that the verifier cannot
 * understand.  We inspect the loop and publish a side-table row that a
 * lowering backend can consume.  A scalar row is a successful, explicit
 * fallback and not an error.
 */

#define ZR_VECTORIZE_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_VECTORIZE_HASH_PRIME UINT64_C(1099511628211)

static void vectorize_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 byte;
    for (byte = 0u; byte < 4u; ++byte) {
        *hash ^= (TZrUInt64)((value >> (byte * 8u)) & 0xffu);
        *hash *= ZR_VECTORIZE_HASH_PRIME;
    }
}

static void vectorize_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    vectorize_hash_u32(hash, (TZrUInt32)value);
    vectorize_hash_u32(hash, (TZrUInt32)(value >> 32u));
}

/* A local address-free hash keeps the pass usable in a small parser-only
 * target.  Full module hashing can still combine this value with the shared
 * ExecIR hash at artifact publication time. */
static TZrUInt64 vectorize_function_hash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZR_VECTORIZE_HASH_OFFSET;
    TZrUInt32 index;
    if (function == ZR_NULL) return 0u;
    vectorize_hash_u32(&hash, function->id);
    vectorize_hash_u32(&hash, function->functionToken);
    vectorize_hash_u64(&hash, function->signatureHash);
    vectorize_hash_u32(&hash, function->instructionCount);
    vectorize_hash_u32(&hash, function->blockCount);
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        vectorize_hash_u32(&hash, instruction->opcode);
        vectorize_hash_u32(&hash, instruction->flags);
        vectorize_hash_u32(&hash, instruction->typeToken);
        vectorize_hash_u32(&hash, instruction->matchTypeToken);
        vectorize_hash_u32(&hash, instruction->layoutId);
        vectorize_hash_u32(&hash, instruction->sourceId);
        vectorize_hash_u32(&hash, instruction->operandRange.start);
        vectorize_hash_u32(&hash, instruction->operandRange.count);
        vectorize_hash_u32(&hash, instruction->resultRange.start);
        vectorize_hash_u32(&hash, instruction->resultRange.count);
        vectorize_hash_u32(&hash, instruction->memoryIn.start);
        vectorize_hash_u32(&hash, instruction->memoryIn.count);
        vectorize_hash_u32(&hash, instruction->memoryOut.start);
        vectorize_hash_u32(&hash, instruction->memoryOut.count);
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        vectorize_hash_u32(&hash, block->id);
        vectorize_hash_u32(&hash, block->instructionRange.start);
        vectorize_hash_u32(&hash, block->instructionRange.count);
        vectorize_hash_u32(&hash, block->flags);
    }
    return hash == 0u ? ZR_VECTORIZE_HASH_OFFSET : hash;
}

static void vectorize_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                 EZrExecutionDiagnosticCode code,
                                 const SZrExecIrFunction *function,
                                 TZrExecIrBlockId blockId,
                                 TZrExecIrInstructionId instructionId,
                                 EZrExecIrVectorizeReason reason) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    if (function != ZR_NULL) diagnostic->functionToken = function->functionToken;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->actualVersion = (TZrUInt32)reason;
}

static TZrBool vectorize_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool vectorize_function_valid(const SZrExecIrFunction *function,
                                        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (function == ZR_NULL ||
        function->sealed ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operandPool == ZR_NULL) ||
        (function->resultCount != 0u && function->resultPool == ZR_NULL) ||
        (function->memoryTokenCount != 0u && function->memoryTokenPool == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL)) {
        vectorize_diagnostic(diagnostic,
                             function != ZR_NULL && function->sealed
                                 ? ZR_EXEC_IR_DIAGNOSTIC_SEALED
                                 : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                             function, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (block->id == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            !vectorize_range_valid(block->instructionRange,
                                   function->instructionCount) ||
            !vectorize_range_valid(block->predecessorRange,
                                   function->predecessorCount) ||
            !vectorize_range_valid(block->successorRange,
                                   function->successorCount) ||
            !vectorize_range_valid(block->phis, function->phiCount)) {
            vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                 function, block->id, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool vectorize_loop_info_valid(const SZrExecIrLoopInfo *info,
                                         const SZrExecIrFunction *function,
                                         SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (info == ZR_NULL || info->magic != ZR_EXEC_IR_LOOP_INFO_MAGIC ||
        info->schemaVersion != ZR_EXEC_IR_LOOP_SCHEMA_VERSION ||
        info->loopCount > info->loopCapacity ||
        info->memberCount > info->memberCapacity ||
        info->inductionCount > info->inductionCapacity ||
        (info->loopCount != 0u && info->loops == ZR_NULL) ||
        (info->memberCount != 0u && info->memberBlocks == ZR_NULL) ||
        (info->inductionCount != 0u && info->inductions == ZR_NULL)) {
        vectorize_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                             function, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
        return ZR_FALSE;
    }
    for (index = 0u; index < info->loopCount; ++index) {
        const SZrExecIrLoop *loop = &info->loops[index];
        if (loop->id == 0u || loop->id != index + 1u ||
            loop->memberOffset > info->memberCount ||
            loop->memberCount > info->memberCount - loop->memberOffset ||
            loop->inductionOffset > info->inductionCount ||
            loop->inductionCount > info->inductionCount - loop->inductionOffset) {
            vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                 function, loop->headerBlockId, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool vectorize_bool_valid(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static TZrBool vectorize_options_valid(const SZrExecIrVectorizeOptions *options,
                                       SZrExecIrDiagnostic *diagnostic) {
    if (options == ZR_NULL) return ZR_TRUE;
    if (options->schemaVersion != 0u &&
        options->schemaVersion != ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION) {
        vectorize_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                             ZR_NULL, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_VERSION);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION;
            diagnostic->actualVersion = options->schemaVersion;
        }
        return ZR_FALSE;
    }
    if ((options->targetVectorLanes > ZR_EXEC_IR_VECTORIZER_MAX_LANES) ||
        (options->requiredAlignment != 0u &&
         (options->requiredAlignment & (options->requiredAlignment - 1u)) != 0u) ||
        (options->sourceAlignment != 0u &&
         (options->sourceAlignment & (options->sourceAlignment - 1u)) != 0u) ||
        options->aliasState > ZR_EXEC_IR_VECTORIZER_ALIAS_MUST_ALIAS ||
        (options->elementBytes != 0u && options->strideBytes == 0) ||
        (options->elementBytes != 0u && options->strideBytes != 0 &&
         options->strideBytes % (TZrMemoryOffset)options->elementBytes != 0) ||
        !vectorize_bool_valid(options->boundsProven) ||
        !vectorize_bool_valid(options->strideProven) ||
        !vectorize_bool_valid(options->dependenceProven) ||
        !vectorize_bool_valid(options->allowOrderedStores) ||
        !vectorize_bool_valid(options->aliasCheckable) ||
        !vectorize_bool_valid(options->allowRuntimeAliasGuard) ||
        !vectorize_bool_valid(options->allowMaskedTail) ||
        !vectorize_bool_valid(options->allowScalarFallback) ||
        !vectorize_bool_valid(options->preserveSourceOrder) ||
        !vectorize_bool_valid(options->disableVectorization) ||
        !vectorize_bool_valid(options->reserved0)) {
        vectorize_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                             ZR_NULL, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void vectorize_options_effective(
        const SZrExecIrVectorizeOptions *input,
        SZrExecIrVectorizeOptions *output) {
    TZrBool initialized;
    ZrParser_ExecIr_VectorizeOptionsInit(output);
    if (input == ZR_NULL) return;
    /* Preserve explicit zero values for policy/alias fields, but retain the
     * initialized defaults for dimensions and opt-in safety switches when the
     * caller supplied a zero-initialized options object. */
    initialized = (TZrBool)(input->schemaVersion != 0u);
    output->schemaVersion = input->schemaVersion == 0u
                                    ? ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION
                                    : input->schemaVersion;
    output->loopInfo = input->loopInfo;
    output->plan = input->plan;
    output->numericPolicy = input->numericPolicy;
    output->elementKind = input->elementKind == ZR_EXEC_IR_NUMERIC_ELEMENT_INVALID
                                  ? ZR_EXEC_IR_NUMERIC_ELEMENT_F32
                                  : input->elementKind;
    if (input->targetVectorLanes != 0u)
        output->targetVectorLanes = input->targetVectorLanes;
    if (input->targetCapabilities != 0u)
        output->targetCapabilities = input->targetCapabilities;
    if (input->sourceAlignment != 0u)
        output->sourceAlignment = input->sourceAlignment;
    if (input->requiredAlignment != 0u)
        output->requiredAlignment = input->requiredAlignment;
    if (input->minTripCount != 0u)
        output->minTripCount = input->minTripCount;
    if (input->maxCodeBytes != 0u)
        output->maxCodeBytes = input->maxCodeBytes;
    if (input->maxBridgeCost != 0u)
        output->maxBridgeCost = input->maxBridgeCost;
    if (input->maxLoops != 0u)
        output->maxLoops = input->maxLoops;
    if (input->strideBytes != 0)
        output->strideBytes = input->strideBytes;
    if (input->elementBytes != 0u)
        output->elementBytes = input->elementBytes;
    if (initialized) {
        output->aliasState = input->aliasState;
        output->boundsProven = input->boundsProven;
        output->strideProven = input->strideProven;
        output->dependenceProven = input->dependenceProven;
        output->allowOrderedStores = input->allowOrderedStores;
        output->aliasCheckable = input->aliasCheckable;
        output->allowRuntimeAliasGuard = input->allowRuntimeAliasGuard;
        output->allowMaskedTail = input->allowMaskedTail;
        output->allowScalarFallback = input->allowScalarFallback;
        output->preserveSourceOrder = input->preserveSourceOrder;
        output->reserved0 = input->reserved0;
    }
    /* A true disable bit is meaningful even for a zero-initialized options
     * object; zero continues to mean the default enabled state. */
    if (input->disableVectorization != ZR_FALSE)
        output->disableVectorization = input->disableVectorization;
    output->remarks = input->remarks;
}

void ZrParser_ExecIr_VectorizeOptionsInit(
        SZrExecIrVectorizeOptions *options) {
    if (options == ZR_NULL) return;
    memset(options, 0, sizeof(*options));
    options->schemaVersion = ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION;
    options->elementKind = ZR_EXEC_IR_NUMERIC_ELEMENT_F32;
    options->targetVectorLanes = ZR_EXEC_IR_VECTORIZER_DEFAULT_LANES;
    options->targetCapabilities = ZR_EXEC_IR_NUMERIC_TARGET_CAP_VECTOR |
                                  ZR_EXEC_IR_NUMERIC_TARGET_CAP_F32 |
                                  ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK |
                                  ZR_EXEC_IR_NUMERIC_TARGET_CAP_UNALIGNED;
    options->sourceAlignment = 16u;
    options->requiredAlignment = 1u;
    options->minTripCount = ZR_EXEC_IR_VECTORIZER_DEFAULT_MIN_TRIP;
    options->maxCodeBytes = ZR_EXEC_IR_VECTORIZER_DEFAULT_MAX_CODE_BYTES;
    options->maxBridgeCost = ZR_EXEC_IR_VECTORIZER_DEFAULT_MAX_BRIDGE_COST;
    options->maxLoops = UINT32_MAX;
    options->strideBytes = 1;
    options->elementBytes = 1u;
    options->aliasState = ZR_EXEC_IR_VECTORIZER_ALIAS_UNKNOWN;
    options->boundsProven = ZR_TRUE;
    options->strideProven = ZR_TRUE;
    options->dependenceProven = ZR_TRUE;
    options->allowOrderedStores = ZR_FALSE;
    options->aliasCheckable = ZR_TRUE;
    options->allowRuntimeAliasGuard = ZR_TRUE;
    options->allowMaskedTail = ZR_TRUE;
    options->allowScalarFallback = ZR_TRUE;
    options->preserveSourceOrder = ZR_TRUE;
}

void ZrParser_ExecIr_VectorizePlanInit(SZrExecIrVectorizePlan *plan) {
    if (plan == ZR_NULL) return;
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_EXEC_IR_VECTORIZER_MAGIC;
    plan->schemaVersion = ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION;
}

void ZrParser_ExecIr_VectorizePlanFree(SZrExecIrVectorizePlan *plan) {
    if (plan == ZR_NULL) return;
    if (plan->magic == ZR_EXEC_IR_VECTORIZER_MAGIC &&
        plan->schemaVersion == ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION) {
        free(plan->loops);
    }
    memset(plan, 0, sizeof(*plan));
}

const SZrExecIrVectorizeLoopPlan *
ZrParser_ExecIr_VectorizePlanAt(const SZrExecIrVectorizePlan *plan,
                                TZrUInt32 index) {
    if (plan == ZR_NULL || plan->magic != ZR_EXEC_IR_VECTORIZER_MAGIC ||
        plan->schemaVersion != ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION ||
        plan->loopCount > plan->loopCapacity || index >= plan->loopCount ||
        plan->loops == ZR_NULL) return ZR_NULL;
    return &plan->loops[index];
}

static TZrBool vectorize_plan_append(SZrExecIrVectorizePlan *plan,
                                     const SZrExecIrVectorizeLoopPlan *row,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 capacity;
    SZrExecIrVectorizeLoopPlan *replacement;
    if (plan == ZR_NULL || row == ZR_NULL ||
        plan->magic != ZR_EXEC_IR_VECTORIZER_MAGIC ||
        plan->schemaVersion != ZR_EXEC_IR_VECTORIZER_SCHEMA_VERSION ||
        plan->loopCount > plan->loopCapacity) {
        vectorize_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                             ZR_NULL, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
        return ZR_FALSE;
    }
    if (plan->loopCount == UINT32_MAX) {
        vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                             ZR_NULL, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_COST);
        return ZR_FALSE;
    }
    if (plan->loopCount == plan->loopCapacity) {
        capacity = plan->loopCapacity == 0u ? 4u : plan->loopCapacity;
        if (capacity > UINT32_MAX / 2u) capacity = UINT32_MAX;
        else capacity *= 2u;
        if (capacity < plan->loopCount + 1u
#if SIZE_MAX < UINT32_MAX
            || (size_t)capacity > SIZE_MAX / sizeof(*replacement)
#endif
        ) {
            vectorize_diagnostic(diagnostic,
                                 ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                 ZR_NULL, 0u, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_COST);
            return ZR_FALSE;
        }
        replacement = (SZrExecIrVectorizeLoopPlan *)realloc(
                plan->loops, (size_t)capacity * sizeof(*replacement));
        if (replacement == ZR_NULL) {
            vectorize_diagnostic(diagnostic,
                                 ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                 ZR_NULL, 0u, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_COST);
            return ZR_FALSE;
        }
        plan->loops = replacement;
        plan->loopCapacity = capacity;
    }
    plan->loops[plan->loopCount++] = *row;
    if (row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR) {
        if (plan->vectorizedCount == UINT32_MAX) return ZR_FALSE;
        ++plan->vectorizedCount;
        plan->changed = ZR_TRUE;
    } else if (row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK) {
        if (plan->scalarFallbackCount == UINT32_MAX) return ZR_FALSE;
        ++plan->scalarFallbackCount;
    } else {
        if (plan->rejectedCount == UINT32_MAX) return ZR_FALSE;
        ++plan->rejectedCount;
    }
    if (row->estimatedCodeBytes > UINT32_MAX - plan->totalCodeBytes ||
        row->bridgeCost > UINT32_MAX - plan->totalBridgeCost) {
        vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                             ZR_NULL, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_COST);
        return ZR_FALSE;
    }
    plan->totalCodeBytes += row->estimatedCodeBytes;
    plan->totalBridgeCost += row->bridgeCost;
    return ZR_TRUE;
}

static TZrBool vectorize_u64_mul(TZrUInt64 left, TZrUInt64 right,
                                 TZrUInt64 *result) {
    if (result == ZR_NULL || (right != 0u && left > UINT64_MAX / right))
        return ZR_FALSE;
    *result = left * right;
    return ZR_TRUE;
}

static TZrBool vectorize_u64_add(TZrUInt64 left, TZrUInt64 right,
                                 TZrUInt64 *result) {
    if (result == ZR_NULL || left > UINT64_MAX - right) return ZR_FALSE;
    *result = left + right;
    return ZR_TRUE;
}

static TZrBool vectorize_is_terminator(EZrExecIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_EXEC_IR_OPCODE_BRANCH ||
                     opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
                     opcode == ZR_EXEC_IR_OPCODE_SWITCH ||
                     opcode == ZR_EXEC_IR_OPCODE_INVOKE ||
                     opcode == ZR_EXEC_IR_OPCODE_THROW ||
                     opcode == ZR_EXEC_IR_OPCODE_SUSPEND ||
                     opcode == ZR_EXEC_IR_OPCODE_RETURN);
}

static TZrBool vectorize_is_arithmetic(EZrExecIrOpcode opcode,
                                       EZrNumericVectorOperation *operation) {
    if (operation == ZR_NULL) return ZR_FALSE;
    switch (opcode) {
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
            *operation = ZR_EXEC_IR_NUMERIC_VECTOR_ADD;
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_SUB:
            *operation = ZR_EXEC_IR_NUMERIC_VECTOR_SUB;
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_MUL:
            *operation = ZR_EXEC_IR_NUMERIC_VECTOR_MUL;
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_NEG:
            *operation = ZR_EXEC_IR_NUMERIC_VECTOR_NEG;
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_COMPARE:
            *operation = ZR_EXEC_IR_NUMERIC_VECTOR_COMPARE;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool vectorize_induction_value(const SZrExecIrLoopInfo *info,
                                         const SZrExecIrLoop *loop,
                                         TZrExecIrValueId valueId) {
    TZrUInt32 index;
    if (info == ZR_NULL || loop == ZR_NULL || valueId == 0u ||
        loop->inductionOffset > info->inductionCount ||
        loop->inductionCount > info->inductionCount - loop->inductionOffset ||
        info->inductions == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < loop->inductionCount; ++index) {
        if (info->inductions[loop->inductionOffset + index].valueId == valueId)
            return ZR_TRUE;
    }
    return ZR_FALSE;
}

static EZrExecIrVectorizeReason vectorize_scan_loop(
        const SZrExecIrFunction *function, const SZrExecIrLoopInfo *info,
        const SZrExecIrLoop *loop, const SZrExecIrVectorizeOptions *options,
        TZrUInt32 *bodyCount, EZrNumericVectorOperation *operation,
        TZrExecIrSourceId *sourceId, TZrBool *hasMemory, TZrBool *hasStore,
        TZrBool *hasForbiddenEffect, TZrBool *hasDependence,
        SZrExecIrDiagnostic *diagnostic) {
    const TZrExecIrBlockId *members;
    TZrUInt32 memberIndex;
    TZrBool foundOperation = ZR_FALSE;
    if (bodyCount == ZR_NULL || operation == ZR_NULL || sourceId == ZR_NULL ||
        hasMemory == ZR_NULL || hasStore == ZR_NULL ||
        hasForbiddenEffect == ZR_NULL ||
        hasDependence == ZR_NULL) return ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP;
    *bodyCount = 0u;
    *operation = ZR_EXEC_IR_NUMERIC_VECTOR_INVALID;
    *sourceId = 0u;
    *hasMemory = ZR_FALSE;
    *hasStore = ZR_FALSE;
    *hasForbiddenEffect = ZR_FALSE;
    *hasDependence = ZR_FALSE;
    members = ZrParser_ExecIr_LoopMembers(info, loop);
    if (members == ZR_NULL) return ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP;
    for (memberIndex = 0u; memberIndex < loop->memberCount; ++memberIndex) {
        TZrExecIrBlockId blockId = members[memberIndex];
        const SZrExecIrBlock *block;
        TZrUInt32 instructionIndex;
        if (blockId == 0u || blockId > function->blockCount)
            return ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP;
        block = &function->blocks[blockId - 1u];
        for (instructionIndex = block->instructionRange.start;
             instructionIndex < block->instructionRange.start +
                                 block->instructionRange.count;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            EZrExecIrOpcode opcode = (EZrExecIrOpcode)instruction->opcode;
            EZrNumericVectorOperation candidateOperation;
            if (instruction->sourceId != 0u && *sourceId == 0u)
                *sourceId = instruction->sourceId;
            if (vectorize_is_terminator(opcode)) continue;
            if (opcode <= ZR_EXEC_IR_OPCODE_INVALID ||
                opcode >= ZR_EXEC_IR_OPCODE_COUNT ||
                (instruction->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u) {
                return ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE;
            }
            if (!vectorize_range_valid(instruction->operandRange,
                                       function->operandCount) ||
                !vectorize_range_valid(instruction->resultRange,
                                       function->resultCount) ||
                !vectorize_range_valid(instruction->memoryIn,
                                       function->memoryTokenCount) ||
                !vectorize_range_valid(instruction->memoryOut,
                                       function->memoryTokenCount) ||
                !vectorize_range_valid(instruction->phiRange,
                                       function->phiIncomingCount) ||
                !vectorize_range_valid(instruction->successorRange,
                                       function->successorCount)) {
                return ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP;
            }
            if (*bodyCount == UINT32_MAX) return ZR_EXEC_IR_VECTORIZER_REASON_COST;
            ++*bodyCount;
            if ((instruction->flags & (ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                                       ZR_EXEC_IR_FLAG_MAY_THROW |
                                       ZR_EXEC_IR_FLAG_MAY_GC |
                                       ZR_EXEC_IR_FLAG_MAY_SUSPEND)) != 0u ||
                instruction->effectIn != 0u || instruction->effectOut != 0u ||
                instruction->deoptId != 0u || instruction->bindingRow != 0u) {
                *hasForbiddenEffect = ZR_TRUE;
            }
            if (instruction->memoryIn.count != 0u ||
                instruction->memoryOut.count != 0u) {
                *hasMemory = ZR_TRUE;
                if (opcode == ZR_EXEC_IR_OPCODE_LOAD) {
                    if (instruction->memoryIn.count != 1u ||
                        instruction->memoryOut.count != 0u)
                        *hasForbiddenEffect = ZR_TRUE;
                } else if (opcode == ZR_EXEC_IR_OPCODE_STORE) {
                    *hasStore = ZR_TRUE;
                    if (instruction->memoryOut.count != 1u ||
                        instruction->memoryIn.count != 0u)
                        *hasForbiddenEffect = ZR_TRUE;
                    if (!options->allowOrderedStores)
                        *hasForbiddenEffect = ZR_TRUE;
                } else {
                    *hasForbiddenEffect = ZR_TRUE;
                }
            }
            if (opcode == ZR_EXEC_IR_OPCODE_PHI ||
                instruction->phiRange.count != 0u) {
                TZrBool inductionPhi = ZR_FALSE;
                if (opcode == ZR_EXEC_IR_OPCODE_PHI &&
                    instruction->resultRange.count == 1u &&
                    function->resultPool != ZR_NULL) {
                    inductionPhi = vectorize_induction_value(
                            info, loop,
                            function->resultPool[instruction->resultRange.start]);
                }
                if (!inductionPhi) *hasDependence = ZR_TRUE;
            }
            if (vectorize_is_arithmetic(opcode, &candidateOperation) &&
                !foundOperation) {
                *operation = candidateOperation;
                foundOperation = ZR_TRUE;
            }
            if (opcode != ZR_EXEC_IR_OPCODE_LOAD &&
                opcode != ZR_EXEC_IR_OPCODE_STORE &&
                opcode != ZR_EXEC_IR_OPCODE_PHI &&
                !vectorize_is_arithmetic(opcode, &candidateOperation)) {
                /* Constants and copies are harmless scaffolding; all other
                 * opcodes are not a portable elementwise body today. */
                if (opcode != ZR_EXEC_IR_OPCODE_CONSTANT &&
                    opcode != ZR_EXEC_IR_OPCODE_COPY &&
                    opcode != ZR_EXEC_IR_OPCODE_MOVE) {
                    return ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE;
                }
            }
        }
    }
    (void)options;
    (void)diagnostic;
    if (*bodyCount == 0u) return ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE;
    if (!foundOperation) {
        *operation = *hasStore ? ZR_EXEC_IR_NUMERIC_VECTOR_STORE
                               : (*hasMemory ? ZR_EXEC_IR_NUMERIC_VECTOR_LOAD
                                             : ZR_EXEC_IR_NUMERIC_VECTOR_INVALID);
    }
    return ZR_EXEC_IR_VECTORIZER_REASON_NONE;
}

static TZrBool vectorize_emit_remark(const SZrExecIrVectorizeOptions *options,
                                     const SZrExecIrVectorizeLoopPlan *row,
                                     TZrUInt64 irHash,
                                     SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOptimizationRemark remark;
    SZrExecIrRemarkSink *sink;
    TZrUInt32 capacity;
    SZrExecIrOptimizationRemark *items;
    if (options == ZR_NULL || options->remarks == ZR_NULL || row == ZR_NULL)
        return ZR_TRUE;
    sink = options->remarks;
    if (sink->count > sink->capacity) {
        vectorize_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                             ZR_NULL, 0u, 0u,
                             ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP);
        return ZR_FALSE;
    }
    if (sink->count == sink->capacity) {
        if (sink->capacity > UINT32_MAX / 2u) {
            vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                 ZR_NULL, 0u, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_COST);
            return ZR_FALSE;
        }
        capacity = sink->capacity == 0u ? 8u : sink->capacity * 2u;
#if SIZE_MAX < UINT32_MAX
        if ((size_t)capacity > SIZE_MAX / sizeof(*items)) {
            vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                 ZR_NULL, 0u, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_COST);
            return ZR_FALSE;
        }
#endif
        items = (SZrExecIrOptimizationRemark *)realloc(
                sink->items, (size_t)capacity * sizeof(*items));
        if (items == ZR_NULL) {
            vectorize_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                 ZR_NULL, 0u, 0u,
                                 ZR_EXEC_IR_VECTORIZER_REASON_COST);
            return ZR_FALSE;
        }
        sink->items = items;
        sink->capacity = capacity;
    }
    memset(&remark, 0, sizeof(remark));
    remark.pass = "vectorize";
    remark.sourceId = row->sourceId;
    remark.outcome = row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR
                             ? ZR_EXEC_IR_REMARK_SUCCESS
                             : ZR_EXEC_IR_REMARK_MISSED;
    remark.reasonCode = (TZrUInt32)row->reason;
    remark.beforeHash = irHash;
    remark.afterHash = irHash;
    sink->items[sink->count++] = remark;
    return ZR_TRUE;
}

static void vectorize_row_fallback(SZrExecIrVectorizeLoopPlan *row,
                                   EZrExecIrVectorizeReason reason,
                                   const SZrExecIrVectorizeOptions *options) {
    row->decision = options->allowScalarFallback
                            ? ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK
                            : ZR_EXEC_IR_VECTORIZER_DECISION_REJECTED;
    row->reason = reason;
    row->vectorLanes = 1u;
    row->vectorIterations = 0u;
    row->tailLanes = 0u;
    row->usesMaskedTail = ZR_FALSE;
    row->usesScalarTail = ZR_FALSE;
    row->needsAliasGuard = ZR_FALSE;
    row->preservesSourceOrder = ZR_TRUE;
}

static EZrExecIrVectorizeReason vectorize_numeric_check(
        SZrExecIrVectorizeLoopPlan *row,
        const SZrExecIrVectorizeOptions *options,
        SZrExecIrDiagnostic *diagnostic) {
    SZrVectorLegalizationRequest request;
    SZrVectorLegalizationResult result;
    if (row == ZR_NULL || options == ZR_NULL) return ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP;
    ZrParser_ExecIr_VectorLegalizationRequestInit(&request);
    request.operation = row->operation;
    request.elementKind = row->elementKind;
    request.length = row->tripCount;
    request.targetVectorLanes = options->targetVectorLanes;
    request.targetCapabilities = options->targetCapabilities;
    request.sourceAlignment = options->sourceAlignment;
    request.requiredAlignment = options->requiredAlignment;
    request.policy = options->numericPolicy;
    request.allowScalarFallback = ZR_TRUE;
    request.preserveSourceOrder = options->preserveSourceOrder;
    if (!ZrParser_ExecIr_LegalizeVector(&request, &result, diagnostic))
        return ZR_EXEC_IR_VECTORIZER_REASON_NUMERIC_POLICY;
    /* Preserve the target/policy identity for backend cache keys even when a
     * scalar fallback is selected. */
    row->contractHash = result.contractHash;
    if (result.decision == ZR_EXEC_IR_NUMERIC_LEGALIZE_SCALAR_FALLBACK) {
        if (result.reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ALIGNMENT)
            return ZR_EXEC_IR_VECTORIZER_REASON_ALIGNMENT;
        if (result.reason == ZR_EXEC_IR_NUMERIC_LEGALIZE_REASON_ORDER_REQUIRED)
            return ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE;
        return ZR_EXEC_IR_VECTORIZER_REASON_NUMERIC_POLICY;
    }
    return ZR_EXEC_IR_VECTORIZER_REASON_NONE;
}

static EZrExecIrVectorizeReason vectorize_one_loop(
        const SZrExecIrFunction *function, const SZrExecIrLoopInfo *info,
        const SZrExecIrLoop *loop, const SZrExecIrVectorizeOptions *options,
        SZrExecIrVectorizeLoopPlan *row, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 bodyCount;
    EZrNumericVectorOperation operation;
    TZrExecIrSourceId sourceId;
    TZrBool hasMemory;
    TZrBool hasStore;
    TZrBool hasForbiddenEffect;
    TZrBool hasDependence;
    EZrExecIrVectorizeReason reason;
    TZrUInt64 vectorCost;
    TZrUInt64 scalarCost;
    TZrUInt64 vectorIterations;
    TZrUInt64 estimatedBytes;
    TZrUInt32 lanes;
    TZrUInt32 tail;
    memset(row, 0, sizeof(*row));
    row->loopId = loop->id;
    row->tripCount = loop->tripCountMax;
    row->elementKind = options->elementKind;
    row->aliasState = options->aliasState;
    row->strideBytes = options->strideBytes;
    row->elementBytes = options->elementBytes;
    row->boundsProven = options->boundsProven;
    row->strideProven = options->strideProven;
    row->dependenceProven = options->dependenceProven;
    row->preservesSourceOrder = options->preserveSourceOrder;
    reason = vectorize_scan_loop(function, info, loop, options, &bodyCount,
                                 &operation, &sourceId, &hasMemory, &hasStore,
                                 &hasForbiddenEffect, &hasDependence,
                                 diagnostic);
    row->sourceId = sourceId;
    row->operation = operation;
    if (reason != ZR_EXEC_IR_VECTORIZER_REASON_NONE) return reason;
    if (options->disableVectorization) return ZR_EXEC_IR_VECTORIZER_REASON_DISABLED;
    if (!loop->reducible || loop->multipleEntry)
        return loop->multipleEntry ? ZR_EXEC_IR_VECTORIZER_REASON_MULTIPLE_ENTRY
                                   : ZR_EXEC_IR_VECTORIZER_REASON_IRREDUCIBLE;
    if (!loop->hasPreheader)
        return ZR_EXEC_IR_VECTORIZER_REASON_NO_PREHEADER;
    if (loop->zeroTrip) return ZR_EXEC_IR_VECTORIZER_REASON_ZERO_TRIP;
    if (!loop->tripCountKnown || loop->tripCountMax == 0u ||
        loop->tripCountMax == ZR_EXEC_IR_LOOP_TRIP_COUNT_UNKNOWN)
        return ZR_EXEC_IR_VECTORIZER_REASON_UNKNOWN_TRIP;
    if (loop->tripCountMax < options->minTripCount)
        return ZR_EXEC_IR_VECTORIZER_REASON_SMALL_TRIP;
    if (hasDependence) return ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE;
    if (!options->dependenceProven)
        return ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE;
    if (hasForbiddenEffect) return ZR_EXEC_IR_VECTORIZER_REASON_EFFECT;
    if (hasMemory) {
        if (!options->boundsProven)
            return ZR_EXEC_IR_VECTORIZER_REASON_BOUNDS_UNKNOWN;
        if (!options->strideProven || options->strideBytes == 0)
            return ZR_EXEC_IR_VECTORIZER_REASON_STRIDE_UNKNOWN;
        if (options->aliasState == ZR_EXEC_IR_VECTORIZER_ALIAS_MAY_ALIAS ||
            options->aliasState == ZR_EXEC_IR_VECTORIZER_ALIAS_MUST_ALIAS)
            return ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNSAFE;
        if (options->aliasState == ZR_EXEC_IR_VECTORIZER_ALIAS_UNKNOWN) {
            if (!options->aliasCheckable || !options->allowRuntimeAliasGuard)
                return ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNKNOWN;
            row->needsAliasGuard = ZR_TRUE;
            row->bridgeCost = 4u;
        }
    }
    if (operation == ZR_EXEC_IR_NUMERIC_VECTOR_INVALID)
        return ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE;
    reason = vectorize_numeric_check(row, options, diagnostic);
    if (reason != ZR_EXEC_IR_VECTORIZER_REASON_NONE) return reason;
    lanes = options->targetVectorLanes;
    if (lanes == 0u || lanes > ZR_EXEC_IR_VECTORIZER_MAX_LANES)
        return ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP;
    vectorIterations = loop->tripCountMax / lanes;
    tail = (TZrUInt32)(loop->tripCountMax % lanes);
    row->vectorLanes = lanes;
    row->vectorIterations = vectorIterations;
    row->tailLanes = tail;
    if (tail != 0u) {
        if (options->allowMaskedTail &&
            (options->targetCapabilities & ZR_EXEC_IR_NUMERIC_TARGET_CAP_MASK) != 0u)
            row->usesMaskedTail = ZR_TRUE;
        else
            row->usesScalarTail = ZR_TRUE;
    }
    if (!vectorize_u64_mul((TZrUInt64)bodyCount, loop->tripCountMax,
                           &scalarCost) ||
        !vectorize_u64_mul((TZrUInt64)bodyCount, vectorIterations,
                           &vectorCost) ||
        !vectorize_u64_add(vectorCost,
                           (TZrUInt64)bodyCount * (TZrUInt64)tail,
                           &vectorCost) ||
        !vectorize_u64_add(vectorCost, row->bridgeCost, &vectorCost))
        return ZR_EXEC_IR_VECTORIZER_REASON_COST;
    row->estimatedScalarCost = scalarCost;
    row->estimatedVectorCost = vectorCost;
    if (!vectorize_u64_mul((TZrUInt64)bodyCount, 8u, &estimatedBytes) ||
        (row->needsAliasGuard &&
         !vectorize_u64_add(estimatedBytes, 16u, &estimatedBytes)) ||
        (tail != 0u &&
         !vectorize_u64_add(estimatedBytes, 8u, &estimatedBytes)) ||
        estimatedBytes > UINT32_MAX)
        return ZR_EXEC_IR_VECTORIZER_REASON_CODE_SIZE;
    row->estimatedCodeBytes = (TZrUInt32)estimatedBytes;
    if (row->bridgeCost > options->maxBridgeCost)
        return ZR_EXEC_IR_VECTORIZER_REASON_BRIDGE_BUDGET;
    if (row->estimatedCodeBytes > options->maxCodeBytes)
        return ZR_EXEC_IR_VECTORIZER_REASON_CODE_SIZE;
    if (vectorCost >= scalarCost)
        return ZR_EXEC_IR_VECTORIZER_REASON_COST;
    return ZR_EXEC_IR_VECTORIZER_REASON_NONE;
}

TZrBool ZrParser_ExecIr_VectorizeLoopsEx(
        const SZrExecIrFunction *function,
        const SZrExecIrLoopInfo *loopInfo,
        const SZrExecIrVectorizeOptions *options,
        SZrExecIrVectorizePlan *plan,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrVectorizeOptions effective;
    SZrExecIrVectorizePlan temporary;
    const SZrExecIrLoopInfo *info = loopInfo;
    SZrExecIrLoopInfo analyzed;
    TZrBool ownsAnalysis = ZR_FALSE;
    TZrUInt32 index;
    TZrUInt64 irHash;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    vectorize_options_effective(options, &effective);
    if (!vectorize_options_valid(&effective, diagnostic) ||
        !vectorize_function_valid(function, diagnostic)) return ZR_FALSE;
    if (plan == ZR_NULL) plan = effective.plan;
    ZrParser_ExecIr_VectorizePlanInit(&temporary);
    irHash = vectorize_function_hash(function);
    temporary.irHash = irHash;
    if (info == ZR_NULL) {
        ZrParser_ExecIr_LoopInfoInit(&analyzed);
        if (!ZrParser_ExecIr_AnalyzeLoops(function, &analyzed, diagnostic)) {
            ZrParser_ExecIr_LoopInfoFree(&analyzed);
            return ZR_FALSE;
        }
        info = &analyzed;
        ownsAnalysis = ZR_TRUE;
    }
    if (!vectorize_loop_info_valid(info, function, diagnostic)) {
        if (ownsAnalysis) ZrParser_ExecIr_LoopInfoFree(&analyzed);
        return ZR_FALSE;
    }
    for (index = 0u; index < info->loopCount; ++index) {
        SZrExecIrVectorizeLoopPlan row;
        EZrExecIrVectorizeReason reason;
        if (index >= effective.maxLoops) {
            memset(&row, 0, sizeof(row));
            row.loopId = info->loops[index].id;
            row.tripCount = info->loops[index].tripCountMax;
            row.elementKind = effective.elementKind;
            row.aliasState = effective.aliasState;
            row.strideBytes = effective.strideBytes;
            row.elementBytes = effective.elementBytes;
            row.boundsProven = effective.boundsProven;
            row.strideProven = effective.strideProven;
            row.dependenceProven = effective.dependenceProven;
            reason = ZR_EXEC_IR_VECTORIZER_REASON_COST;
        } else {
            reason = vectorize_one_loop(function, info, &info->loops[index],
                                        &effective, &row, diagnostic);
        }
        if (reason != ZR_EXEC_IR_VECTORIZER_REASON_NONE)
            vectorize_row_fallback(&row, reason, &effective);
        else {
            row.decision = ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR;
            row.reason = ZR_EXEC_IR_VECTORIZER_REASON_NONE;
        }
        if (!vectorize_plan_append(&temporary, &row, diagnostic) ||
            !vectorize_emit_remark(&effective, &row, irHash, diagnostic)) {
            ZrParser_ExecIr_VectorizePlanFree(&temporary);
            if (ownsAnalysis) ZrParser_ExecIr_LoopInfoFree(&analyzed);
            return ZR_FALSE;
        }
    }
    if (plan != ZR_NULL) {
        ZrParser_ExecIr_VectorizePlanFree(plan);
        *plan = temporary;
        memset(&temporary, 0, sizeof(temporary));
    }
    if (ownsAnalysis) ZrParser_ExecIr_LoopInfoFree(&analyzed);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_VectorizeLoops(
        SZrExecIrFunction *function,
        const SZrExecIrVectorizeOptions *options,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrLoopInfo *loopInfo = options == ZR_NULL ? ZR_NULL
                                                           : options->loopInfo;
    SZrExecIrVectorizePlan *plan = options == ZR_NULL ? ZR_NULL : options->plan;
    return ZrParser_ExecIr_VectorizeLoopsEx(function, loopInfo, options, plan,
                                             diagnostic);
}

const TZrChar *ZrParser_ExecIr_VectorizeReasonName(
        EZrExecIrVectorizeReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_VECTORIZER_REASON_NONE: return "none";
        case ZR_EXEC_IR_VECTORIZER_REASON_DISABLED: return "disabled";
        case ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP: return "invalid-loop";
        case ZR_EXEC_IR_VECTORIZER_REASON_NO_PREHEADER: return "no-preheader";
        case ZR_EXEC_IR_VECTORIZER_REASON_MULTIPLE_ENTRY: return "multiple-entry";
        case ZR_EXEC_IR_VECTORIZER_REASON_IRREDUCIBLE: return "irreducible";
        case ZR_EXEC_IR_VECTORIZER_REASON_ZERO_TRIP: return "zero-trip";
        case ZR_EXEC_IR_VECTORIZER_REASON_UNKNOWN_TRIP: return "unknown-trip";
        case ZR_EXEC_IR_VECTORIZER_REASON_SMALL_TRIP: return "small-trip";
        case ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE: return "dependence";
        case ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNSAFE: return "alias-unsafe";
        case ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNKNOWN: return "alias-unknown";
        case ZR_EXEC_IR_VECTORIZER_REASON_BOUNDS_UNKNOWN: return "bounds-unknown";
        case ZR_EXEC_IR_VECTORIZER_REASON_STRIDE_UNKNOWN: return "stride-unknown";
        case ZR_EXEC_IR_VECTORIZER_REASON_ALIGNMENT: return "alignment";
        case ZR_EXEC_IR_VECTORIZER_REASON_EFFECT: return "effect";
        case ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE: return "unsupported-opcode";
        case ZR_EXEC_IR_VECTORIZER_REASON_NUMERIC_POLICY: return "numeric-policy";
        case ZR_EXEC_IR_VECTORIZER_REASON_TAIL: return "tail";
        case ZR_EXEC_IR_VECTORIZER_REASON_COST: return "cost";
        case ZR_EXEC_IR_VECTORIZER_REASON_CODE_SIZE: return "code-size";
        case ZR_EXEC_IR_VECTORIZER_REASON_BRIDGE_BUDGET: return "bridge-budget";
        case ZR_EXEC_IR_VECTORIZER_REASON_VERSION: return "version";
        case ZR_EXEC_IR_VECTORIZER_REASON_INVALID:
        default: return "invalid";
    }
}

const TZrChar *ZrParser_ExecIr_VectorizeDecisionName(
        EZrExecIrVectorizeDecision decision) {
    switch (decision) {
        case ZR_EXEC_IR_VECTORIZER_DECISION_NONE: return "none";
        case ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR: return "vector";
        case ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK: return "scalar-fallback";
        case ZR_EXEC_IR_VECTORIZER_DECISION_REJECTED: return "rejected";
        case ZR_EXEC_IR_VECTORIZER_DECISION_COUNT:
        default: return "invalid";
    }
}
