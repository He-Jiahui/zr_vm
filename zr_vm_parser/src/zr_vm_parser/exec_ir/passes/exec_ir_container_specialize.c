#include "zr_vm_parser/exec_ir_container_specialize.h"

#include <stdint.h>
#include <string.h>

#define ZR_CONTAINER_SPECIALIZE_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_CONTAINER_SPECIALIZE_FNV_PRIME UINT64_C(1099511628211)

static void zr_container_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    if (hash == ZR_NULL) return;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_CONTAINER_SPECIALIZE_FNV_PRIME;
    }
}

static void zr_container_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 index;
    if (hash == ZR_NULL) return;
    for (index = 0u; index < 8u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_CONTAINER_SPECIALIZE_FNV_PRIME;
    }
}

static void zr_container_hash_bool(TZrUInt64 *hash, TZrBool value) {
    zr_container_hash_u32(hash, value != ZR_FALSE ? 1u : 0u);
}

static void zr_container_hash_map(TZrUInt64 *hash,
                                   const SZrCompactMapCandidate *candidate) {
    const SZrCompactMapLayout *layout;
    if (hash == ZR_NULL || candidate == ZR_NULL) return;
    zr_container_hash_u32(hash, candidate->magic);
    zr_container_hash_u32(hash, candidate->schemaVersion);
    zr_container_hash_u64(hash, candidate->hash.seed);
    zr_container_hash_u64(hash, candidate->hash.domain);
    zr_container_hash_u32(hash, candidate->hash.version);
    zr_container_hash_u32(hash, candidate->hash.flags);
    layout = &candidate->layout;
    zr_container_hash_u32(hash, layout->entrySize);
    zr_container_hash_u32(hash, layout->entryAlignment);
    zr_container_hash_u32(hash, layout->bucketCount);
    zr_container_hash_u32(hash, layout->maxLoadNumerator);
    zr_container_hash_u32(hash, layout->maxLoadDenominator);
    zr_container_hash_u32(hash, layout->hashOffset);
    zr_container_hash_u32(hash, layout->keyOffset);
    zr_container_hash_u32(hash, layout->valueOffset);
    zr_container_hash_u32(hash, layout->nextOffset);
    zr_container_hash_u32(hash, (TZrUInt32)layout->tombstonePolicy);
    zr_container_hash_u32(hash, (TZrUInt32)layout->iterationPolicy);
    zr_container_hash_u32(hash, (TZrUInt32)layout->ownershipPolicy);
    zr_container_hash_u32(hash, candidate->flags);
    zr_container_hash_u32(hash, candidate->reserved);
    zr_container_hash_u64(hash, candidate->layoutHash);
    zr_container_hash_u64(hash, candidate->candidateHash);
}

static void zr_container_hash_string_facts(
        TZrUInt64 *hash, const SZrStringStorageFacts *facts) {
    if (hash == ZR_NULL || facts == ZR_NULL) return;
    zr_container_hash_u32(hash, facts->flags);
    zr_container_hash_u32(hash, facts->reserved);
    zr_container_hash_u64(hash, facts->byteLength);
    zr_container_hash_u64(hash, facts->shortStringLimit);
    zr_container_hash_u64(hash, facts->internDomain);
    zr_container_hash_u64(hash, facts->storageGeneration);
    zr_container_hash_u64(hash, facts->declaredEffects);
    zr_container_hash_u64(hash, facts->replacementEffects);
    zr_container_hash_u64(hash, facts->intermediateCount);
    zr_container_hash_u64(hash, facts->maxBuilderBytes);
    zr_container_hash_u64(hash, facts->ropeDepth);
    zr_container_hash_u64(hash, facts->segmentCount);
    zr_container_hash_u64(hash, facts->ropeMaxDepth);
    zr_container_hash_u64(hash, facts->ropeMaxSegments);
    zr_container_hash_u64(hash, facts->measuredCopyBytes);
    zr_container_hash_u64(hash, facts->projectedRopeBytes);
    zr_container_hash_u64(hash, facts->flattenBudgetBytes);
    zr_container_hash_u64(hash, facts->flattenBudgetMicros);
}

static void zr_container_hash_string_candidate(
        TZrUInt64 *hash, const SZrStringStorageCandidate *candidate) {
    if (hash == ZR_NULL || candidate == ZR_NULL) return;
    zr_container_hash_u32(hash, candidate->magic);
    zr_container_hash_u32(hash, candidate->schemaVersion);
    zr_container_hash_u32(hash, (TZrUInt32)candidate->strategy);
    zr_container_hash_u32(hash, candidate->flags);
    zr_container_hash_u64(hash, candidate->byteLength);
    zr_container_hash_u64(hash, candidate->shortStringLimit);
    zr_container_hash_u64(hash, candidate->internDomain);
    zr_container_hash_u64(hash, candidate->ropeDepth);
    zr_container_hash_u64(hash, candidate->segmentCount);
    zr_container_hash_u64(hash, candidate->ropeMaxDepth);
    zr_container_hash_u64(hash, candidate->ropeMaxSegments);
    zr_container_hash_u64(hash, candidate->measuredCopyBytes);
    zr_container_hash_u64(hash, candidate->projectedRopeBytes);
    zr_container_hash_u64(hash, candidate->flattenBudgetBytes);
    zr_container_hash_u64(hash, candidate->flattenBudgetMicros);
    zr_container_hash_u64(hash, candidate->declaredEffects);
    zr_container_hash_u64(hash, candidate->replacementEffects);
    zr_container_hash_u64(hash, candidate->intermediateCount);
    zr_container_hash_u64(hash, candidate->maxBuilderBytes);
    zr_container_hash_u64(hash, candidate->candidateHash);
}

static void zr_container_diag_execution_clear(SZrExecIrDiagnostic *execution) {
    if (execution != ZR_NULL) {
        memset(execution, 0, sizeof(*execution));
        execution->code = ZR_EXECUTION_DIAGNOSTIC_NONE;
    }
}

void ZrParser_ExecIr_ContainerSpecializationDiagnosticInit(
        SZrContainerSpecializationDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->status = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK;
    diagnostic->kind = ZR_EXEC_IR_CONTAINER_KIND_NONE;
    zr_container_diag_execution_clear(&diagnostic->execution);
    ZrCore_ContainerStorage_DiagnosticClear(&diagnostic->storage);
}

const TZrChar *ZrParser_ExecIr_ContainerSpecializationStatusName(
        EZrExecIrContainerSpecializationStatus status) {
    switch (status) {
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK: return "ok";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_MISMATCH:
            return "schema-mismatch";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SEALED:
            return "function-sealed";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_INVALID:
            return "function-invalid";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SCOPE_MISMATCH:
            return "function-scope-mismatch";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERATION_MISMATCH:
            return "generation-mismatch";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_IR_HASH_MISMATCH:
            return "ir-hash-mismatch";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT:
            return "unknown-layout";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_CONTRACT_INVALID:
            return "map-contract-invalid";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_HASH_UNSTABLE:
            return "map-hash-unstable";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_EQUALITY_UNSTABLE:
            return "map-equality-unstable";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_OWNERSHIP_UNSAFE:
            return "map-ownership-unsafe";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_ITERATION_UNSAFE:
            return "map-iteration-unsafe";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_CONTRACT_INVALID:
            return "string-contract-invalid";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_IDENTITY_OBSERVED:
            return "string-identity-observed";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_ESCAPE:
            return "string-escape";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_EFFECT_MISMATCH:
            return "string-effect-mismatch";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_BUDGET:
            return "string-budget";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERIC_FALLBACK:
            return "generic-fallback";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH:
            return "plan-hash-mismatch";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNSUPPORTED:
            return "unsupported";
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STATUS_COUNT:
        default:
            return "unknown";
    }
}

static EZrExecutionDiagnosticCode zr_container_execution_code(
        EZrExecIrContainerSpecializationStatus status) {
    switch (status) {
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK:
            return ZR_EXECUTION_DIAGNOSTIC_NONE;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_INVALID_ARGUMENT:
            return ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SEALED:
            return ZR_EXEC_IR_DIAGNOSTIC_SEALED;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERATION_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_IR_HASH_MISMATCH:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_CONTRACT_INVALID:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_CONTRACT_INVALID:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_EFFECT_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH;
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SCOPE_MISMATCH:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_HASH_UNSTABLE:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_EQUALITY_UNSTABLE:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_OWNERSHIP_UNSAFE:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_ITERATION_UNSAFE:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_IDENTITY_OBSERVED:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_ESCAPE:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_BUDGET:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERIC_FALLBACK:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNSUPPORTED:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_INVALID:
        case ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STATUS_COUNT:
        default:
            return ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    }
}

static EZrExecIrContainerSpecializationStatus zr_container_map_status(
        EZrContainerStorageDiagnosticCode code) {
    switch (code) {
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH:
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_DOMAIN:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_HASH_UNSTABLE;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_EQUALITY_UNSTABLE;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_OWNERSHIP_UNSAFE;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION:
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_ITERATION_UNSAFE;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK;
        default:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_CONTRACT_INVALID;
    }
}

static EZrExecIrContainerSpecializationStatus zr_container_string_status(
        EZrContainerStorageDiagnosticCode code) {
    switch (code) {
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_IDENTITY_OBSERVED:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_IDENTITY_OBSERVED;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_ESCAPE;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_EFFECT_MISMATCH;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET:
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT:
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED:
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_DEPTH:
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_SEGMENTS:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_BUDGET;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_EFFECT_MISMATCH;
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK;
        default:
            return ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_CONTRACT_INVALID;
    }
}

static void zr_container_diag_set(
        SZrContainerSpecializationDiagnostic *diagnostic,
        EZrExecIrContainerSpecializationStatus status,
        EZrExecIrContainerKind kind,
        const SZrContainerSpecializationFacts *facts,
        const SZrExecIrFunction *function,
        const SZrContainerStorageDiagnostic *storage) {
    if (diagnostic == ZR_NULL) return;
    diagnostic->status = status;
    diagnostic->kind = kind;
    if (storage != ZR_NULL) diagnostic->storage = *storage;
    diagnostic->execution.code = zr_container_execution_code(status);
    diagnostic->execution.functionToken = function != ZR_NULL
                                               ? function->functionToken
                                               : (facts != ZR_NULL ? facts->functionToken : 0u);
    diagnostic->execution.blockId = facts != ZR_NULL ? facts->blockId : 0u;
    diagnostic->execution.instructionId = facts != ZR_NULL ? facts->instructionId : 0u;
    diagnostic->execution.sourceId = facts != ZR_NULL ? facts->sourceId : 0u;
    if (status == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERATION_MISMATCH &&
        facts != ZR_NULL && function != ZR_NULL) {
        diagnostic->execution.expectedVersion =
                (TZrUInt32)function->contract.generation;
        diagnostic->execution.actualVersion = (TZrUInt32)facts->generation;
    }
    if (storage != ZR_NULL) {
        diagnostic->execution.expectedHash = storage->expected;
        diagnostic->execution.actualHash = storage->actual;
    }
}

static TZrBool zr_container_function_storage_valid(
        const SZrExecIrFunction *function) {
    if (function == ZR_NULL ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        function->phiCount > function->phiCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        function->gcMapCount > function->gcMapCapacity ||
        function->gcRootCount > function->gcRootCapacity ||
        function->deoptStateCount > function->deoptStateCapacity ||
        function->deoptValueCount > function->deoptValueCapacity ||
        function->sourceMapCount > function->sourceMapCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->memoryTokenCount != 0u && function->memoryTokenPool == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->gcRootCount != 0u && function->gcRoots == ZR_NULL) ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL) ||
        (function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (function->gcMapCount != 0u && function->gcMap == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 zr_container_function_hash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZR_CONTAINER_SPECIALIZE_FNV_OFFSET;
    TZrUInt64 aggregateHash = ZrCore_ExecIr_DeoptAggregateHash(function);
    TZrUInt32 index;
    if (aggregateHash == 0u || !zr_container_function_storage_valid(function)) return 0u;
    zr_container_hash_u64(&hash, aggregateHash);
    zr_container_hash_u32(&hash, function->id);
    zr_container_hash_u32(&hash, function->functionToken);
    zr_container_hash_u64(&hash, function->signatureHash);
    zr_container_hash_u64(&hash, function->contract.generation);
    zr_container_hash_u64(&hash, function->contract.layoutHash);
    zr_container_hash_u64(&hash, function->contract.moduleHash);
    zr_container_hash_u32(&hash, function->entryBlockId);
    zr_container_hash_u32(&hash, function->valueCount);
    zr_container_hash_u32(&hash, function->instructionCount);
    zr_container_hash_u32(&hash, function->blockCount);
    zr_container_hash_u32(&hash, function->operandCount);
    zr_container_hash_u32(&hash, function->resultCount);
    zr_container_hash_u32(&hash, function->memoryTokenCount);
    zr_container_hash_u32(&hash, function->phiCount);
    zr_container_hash_u32(&hash, function->phiIncomingCount);
    zr_container_hash_u32(&hash, function->predecessorCount);
    zr_container_hash_u32(&hash, function->successorCount);
    zr_container_hash_u32(&hash, function->gcRootCount);
    zr_container_hash_u32(&hash, function->deoptStateCount);
    zr_container_hash_u32(&hash, function->deoptValueCount);
    zr_container_hash_u32(&hash, function->sourceMapCount);
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        zr_container_hash_u32(&hash, value->id);
        zr_container_hash_u32(&hash, value->definition);
        zr_container_hash_u32(&hash, value->typeToken);
        zr_container_hash_u32(&hash, (TZrUInt32)value->ownership);
        zr_container_hash_u32(&hash, (TZrUInt32)value->nullability);
        zr_container_hash_u32(&hash, value->flags);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        zr_container_hash_u32(&hash, instruction->opcode);
        zr_container_hash_u32(&hash, instruction->flags);
        zr_container_hash_u32(&hash, instruction->typeToken);
        zr_container_hash_u32(&hash, instruction->matchTypeToken);
        zr_container_hash_u32(&hash, instruction->layoutId);
        zr_container_hash_u32(&hash, instruction->effectIn);
        zr_container_hash_u32(&hash, instruction->effectOut);
        zr_container_hash_u32(&hash, instruction->sourceId);
        zr_container_hash_u32(&hash, instruction->deoptId);
        zr_container_hash_u32(&hash, instruction->bindingRow);
        zr_container_hash_u32(&hash, instruction->operands.offset);
        zr_container_hash_u32(&hash, instruction->operands.count);
        zr_container_hash_u32(&hash, instruction->results.offset);
        zr_container_hash_u32(&hash, instruction->results.count);
        zr_container_hash_u32(&hash, instruction->phiRange.offset);
        zr_container_hash_u32(&hash, instruction->phiRange.count);
        zr_container_hash_u32(&hash, instruction->successorRange.offset);
        zr_container_hash_u32(&hash, instruction->successorRange.count);
        zr_container_hash_u32(&hash, instruction->memoryIn.offset);
        zr_container_hash_u32(&hash, instruction->memoryIn.count);
        zr_container_hash_u32(&hash, instruction->memoryOut.offset);
        zr_container_hash_u32(&hash, instruction->memoryOut.count);
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        zr_container_hash_u32(&hash, block->id);
        zr_container_hash_u32(&hash, block->flags);
        zr_container_hash_u32(&hash, block->terminatorInstructionId);
        zr_container_hash_u32(&hash, block->instructions.offset);
        zr_container_hash_u32(&hash, block->instructions.count);
        zr_container_hash_u32(&hash, block->predecessors.offset);
        zr_container_hash_u32(&hash, block->predecessors.count);
        zr_container_hash_u32(&hash, block->successors.offset);
        zr_container_hash_u32(&hash, block->successors.count);
        zr_container_hash_u32(&hash, block->phis.offset);
        zr_container_hash_u32(&hash, block->phis.count);
    }
    for (index = 0u; index < function->operandCount; ++index)
        zr_container_hash_u32(&hash, function->operands[index]);
    for (index = 0u; index < function->resultCount; ++index)
        zr_container_hash_u32(&hash, function->results[index]);
    for (index = 0u; index < function->memoryTokenCount; ++index)
        zr_container_hash_u32(&hash, function->memoryTokenPool[index]);
    for (index = 0u; index < function->predecessorCount; ++index)
        zr_container_hash_u32(&hash, function->predecessors[index]);
    for (index = 0u; index < function->successorCount; ++index)
        zr_container_hash_u32(&hash, function->successors[index]);
    zr_container_hash_bool(&hash, function->sealed);
    return hash == 0u ? UINT64_C(1) : hash;
}

void ZrParser_ExecIr_ContainerSpecializationFactsInit(
        SZrContainerSpecializationFacts *facts) {
    if (facts == ZR_NULL) return;
    memset(facts, 0, sizeof(*facts));
    facts->magic = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAGIC;
    facts->schemaVersion = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_VERSION;
    facts->allowGenericFallback = ZR_TRUE;
    ZrCore_CompactMapCandidate_Init(&facts->mapCandidate);
    ZrCore_StringStorageFacts_Init(&facts->stringFacts);
}

static TZrBool zr_container_facts_envelope_valid(
        const SZrContainerSpecializationFacts *facts,
        SZrContainerSpecializationDiagnostic *diagnostic) {
    if (facts == ZR_NULL) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_INVALID_ARGUMENT,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, ZR_NULL, ZR_NULL);
        return ZR_FALSE;
    }
    if (facts->magic != ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAGIC ||
        facts->schemaVersion != ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_VERSION) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_MISMATCH,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, ZR_NULL, ZR_NULL);
        return ZR_FALSE;
    }
    if ((facts->flags & ~ZR_EXEC_IR_CONTAINER_FACT_KNOWN_MASK) != 0u ||
        facts->reservedBool != ZR_FALSE) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNSUPPORTED,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, ZR_NULL, ZR_NULL);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ContainerSpecializationFactsValidate(
        const SZrContainerSpecializationFacts *facts,
        SZrContainerSpecializationDiagnostic *diagnostic) {
    SZrContainerStorageDiagnostic storage;
    EZrExecIrContainerSpecializationStatus status;
    ZrParser_ExecIr_ContainerSpecializationDiagnosticInit(diagnostic);
    if (!zr_container_facts_envelope_valid(facts, diagnostic)) return ZR_FALSE;
    if (facts->mapKnown) {
        ZrCore_ContainerStorage_DiagnosticClear(&storage);
        if (!ZrCore_CompactMapCandidate_Validate(&facts->mapCandidate, &storage)) {
            status = zr_container_map_status(storage.code);
            zr_container_diag_set(diagnostic, status, ZR_EXEC_IR_CONTAINER_KIND_MAP,
                                  facts, ZR_NULL, &storage);
            return ZR_FALSE;
        }
    }
    if (facts->stringKnown) {
        ZrCore_ContainerStorage_DiagnosticClear(&storage);
        if (!ZrCore_StringStorage_Validate(&facts->stringFacts, &storage)) {
            status = zr_container_string_status(storage.code);
            zr_container_diag_set(diagnostic,
                                  status, ZR_EXEC_IR_CONTAINER_KIND_STRING,
                                  facts, ZR_NULL, &storage);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrUInt64 ZrParser_ExecIr_ContainerSpecializationFactsHash(
        const SZrContainerSpecializationFacts *facts) {
    TZrUInt64 hash = ZR_CONTAINER_SPECIALIZE_FNV_OFFSET;
    if (facts == ZR_NULL) return 0u;
    zr_container_hash_u32(&hash, facts->magic);
    zr_container_hash_u32(&hash, facts->schemaVersion);
    zr_container_hash_u32(&hash, facts->flags);
    zr_container_hash_u32(&hash, facts->functionToken);
    zr_container_hash_u32(&hash, facts->blockId);
    zr_container_hash_u32(&hash, facts->instructionId);
    zr_container_hash_u32(&hash, facts->sourceId);
    zr_container_hash_u64(&hash, facts->generation);
    zr_container_hash_u64(&hash, facts->irHash);
    zr_container_hash_bool(&hash, facts->mapKnown);
    zr_container_hash_bool(&hash, facts->stringKnown);
    zr_container_hash_bool(&hash, facts->allowGenericFallback);
    if (facts->mapKnown) zr_container_hash_map(&hash, &facts->mapCandidate);
    if (facts->stringKnown)
        zr_container_hash_string_facts(&hash, &facts->stringFacts);
    return hash == 0u ? UINT64_C(1) : hash;
}

void ZrParser_ExecIr_ContainerSpecializationPlanInit(
        SZrContainerSpecializationPlan *plan) {
    if (plan == ZR_NULL) return;
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAGIC;
    plan->schemaVersion = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_VERSION;
    plan->status = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK;
    plan->fallbackReason = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK;
    plan->mapStrategy = ZR_EXEC_IR_CONTAINER_MAP_GENERIC;
    plan->stringStrategy = ZR_STRING_STORAGE_STRATEGY_GENERIC;
    plan->preservesGenericEquality = ZR_TRUE;
    plan->irUnchanged = ZR_TRUE;
}

static void zr_container_plan_set_fallback(
        SZrContainerSpecializationPlan *plan,
        EZrExecIrContainerSpecializationStatus status,
        EZrExecIrContainerKind kind,
        const SZrContainerSpecializationFacts *facts,
        const SZrExecIrFunction *function,
        const SZrContainerStorageDiagnostic *storage,
        SZrContainerSpecializationDiagnostic *diagnostic) {
    if (plan == ZR_NULL) return;
    if (plan->fallbackReason == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK)
        plan->fallbackReason = status;
    if (diagnostic != ZR_NULL &&
        diagnostic->status == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK)
        zr_container_diag_set(diagnostic, status, kind, facts, function, storage);
}

static TZrBool zr_container_check_function(
        const SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        SZrContainerSpecializationDiagnostic *diagnostic) {
    TZrUInt64 currentHash;
    if (!zr_container_function_storage_valid(function)) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_INVALID,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (function->sealed) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SEALED,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (facts != ZR_NULL && facts->functionToken != 0u &&
        facts->functionToken != function->functionToken) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SCOPE_MISMATCH,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (facts != ZR_NULL && facts->generation != 0u &&
        facts->generation != function->contract.generation) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERATION_MISMATCH,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (facts != ZR_NULL && facts->irHash != 0u) {
        currentHash = zr_container_function_hash(function);
        if (currentHash == 0u || currentHash != facts->irHash) {
            if (diagnostic != ZR_NULL) {
                zr_container_diag_set(diagnostic,
                                      ZR_EXEC_IR_CONTAINER_SPECIALIZATION_IR_HASH_MISMATCH,
                                      ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function,
                                      ZR_NULL);
                diagnostic->execution.expectedHash = currentHash;
                diagnostic->execution.actualHash = facts->irHash;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_container_allow_generic(
        const SZrContainerSpecializationFacts *facts) {
    return (TZrBool)(facts != ZR_NULL &&
                     facts->allowGenericFallback != ZR_FALSE);
}

static TZrBool zr_container_required_map(
        const SZrContainerSpecializationFacts *facts) {
    return (TZrBool)(facts != ZR_NULL &&
                     (facts->flags & ZR_EXEC_IR_CONTAINER_FACT_REQUIRE_MAP) != 0u);
}

static TZrBool zr_container_required_string(
        const SZrContainerSpecializationFacts *facts) {
    return (TZrBool)(facts != ZR_NULL &&
                     (facts->flags & ZR_EXEC_IR_CONTAINER_FACT_REQUIRE_STRING) != 0u);
}

TZrBool ZrParser_ExecIr_BuildContainerSpecialization(
        const SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        SZrContainerSpecializationPlan *plan,
        SZrContainerSpecializationDiagnostic *diagnostic) {
    SZrContainerStorageDiagnostic storage;
    EZrExecIrContainerSpecializationStatus status;
    EZrStringStorageStrategy stringStrategy;
    TZrBool genericAllowed;

    ZrParser_ExecIr_ContainerSpecializationDiagnosticInit(diagnostic);
    if (plan == ZR_NULL || function == ZR_NULL || facts == ZR_NULL) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_INVALID_ARGUMENT,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_ContainerSpecializationPlanInit(plan);
    if (!zr_container_facts_envelope_valid(facts, diagnostic) ||
        !zr_container_check_function(function, facts, diagnostic)) {
        return ZR_FALSE;
    }
    genericAllowed = zr_container_allow_generic(facts);
    plan->functionToken = function->functionToken;
    plan->blockId = facts->blockId;
    plan->instructionId = facts->instructionId;
    plan->sourceId = facts->sourceId;
    plan->generation = function->contract.generation;
    plan->beforeHash = zr_container_function_hash(function);
    plan->afterHash = plan->beforeHash;
    plan->mapKnown = facts->mapKnown;
    plan->stringKnown = facts->stringKnown;

    if (facts->mapKnown) {
        ZrCore_ContainerStorage_DiagnosticClear(&storage);
        if (ZrCore_CompactMapCandidate_Validate(&facts->mapCandidate, &storage)) {
            plan->mapCandidate = facts->mapCandidate;
            plan->mapStrategy = ZR_EXEC_IR_CONTAINER_MAP_COMPACT;
            plan->cacheHash = (TZrBool)(
                    (facts->mapCandidate.flags & ZR_COMPACT_MAP_FLAG_CACHE_HASH) != 0u &&
                    ZrCore_CompactMap_CanCacheHash(&facts->mapCandidate.hash));
        } else {
            status = zr_container_map_status(storage.code);
            zr_container_plan_set_fallback(plan, status,
                                           ZR_EXEC_IR_CONTAINER_KIND_MAP, facts,
                                           function, &storage, diagnostic);
            if (!genericAllowed || zr_container_required_map(facts)) return ZR_FALSE;
        }
    } else {
        status = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT;
        zr_container_plan_set_fallback(plan, status, ZR_EXEC_IR_CONTAINER_KIND_MAP,
                                       facts, function, ZR_NULL, diagnostic);
        if (!genericAllowed || zr_container_required_map(facts)) return ZR_FALSE;
    }

    if (facts->stringKnown) {
        ZrCore_ContainerStorage_DiagnosticClear(&storage);
        if (!ZrCore_StringStorage_Validate(&facts->stringFacts, &storage)) {
            status = zr_container_string_status(storage.code);
            zr_container_plan_set_fallback(plan, status,
                                           ZR_EXEC_IR_CONTAINER_KIND_STRING, facts,
                                           function, &storage, diagnostic);
            if (!genericAllowed || zr_container_required_string(facts)) return ZR_FALSE;
        } else {
            ZrCore_ContainerStorage_DiagnosticClear(&storage);
            stringStrategy = ZrCore_StringStorage_SelectStrategy(
                    &facts->stringFacts, &storage);
            if (stringStrategy == ZR_STRING_STORAGE_STRATEGY_GENERIC) {
                status = zr_container_string_status(storage.code);
                if (status == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK)
                    status = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERIC_FALLBACK;
                zr_container_plan_set_fallback(plan, status,
                                               ZR_EXEC_IR_CONTAINER_KIND_STRING, facts,
                                               function, &storage, diagnostic);
                if (!genericAllowed || zr_container_required_string(facts))
                    return ZR_FALSE;
            } else if (ZrCore_StringStorage_BuildCandidate(
                               &facts->stringFacts, &plan->stringCandidate,
                               &storage)) {
                plan->stringStrategy = stringStrategy;
            } else {
                status = zr_container_string_status(storage.code);
                zr_container_plan_set_fallback(plan, status,
                                               ZR_EXEC_IR_CONTAINER_KIND_STRING, facts,
                                               function, &storage, diagnostic);
                if (!genericAllowed || zr_container_required_string(facts))
                    return ZR_FALSE;
            }
        }
    } else {
        status = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT;
        zr_container_plan_set_fallback(plan, status,
                                       ZR_EXEC_IR_CONTAINER_KIND_STRING, facts,
                                       function, ZR_NULL, diagnostic);
        if (!genericAllowed || zr_container_required_string(facts)) return ZR_FALSE;
    }

    plan->specialized = (TZrBool)(plan->mapStrategy != ZR_EXEC_IR_CONTAINER_MAP_GENERIC ||
                                  plan->stringStrategy != ZR_STRING_STORAGE_STRATEGY_GENERIC);
    plan->status = plan->fallbackReason == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK
                       ? ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK
                       : plan->fallbackReason;
    plan->planHash = ZrParser_ExecIr_ContainerSpecializationPlanHash(plan);
    return ZR_TRUE;
}

TZrUInt64 ZrParser_ExecIr_ContainerSpecializationPlanHash(
        const SZrContainerSpecializationPlan *plan) {
    TZrUInt64 hash = ZR_CONTAINER_SPECIALIZE_FNV_OFFSET;
    if (plan == ZR_NULL) return 0u;
    zr_container_hash_u32(&hash, plan->magic);
    zr_container_hash_u32(&hash, plan->schemaVersion);
    zr_container_hash_u32(&hash, (TZrUInt32)plan->status);
    zr_container_hash_u32(&hash, (TZrUInt32)plan->fallbackReason);
    zr_container_hash_u32(&hash, plan->functionToken);
    zr_container_hash_u32(&hash, plan->blockId);
    zr_container_hash_u32(&hash, plan->instructionId);
    zr_container_hash_u32(&hash, plan->sourceId);
    zr_container_hash_u64(&hash, plan->generation);
    zr_container_hash_u64(&hash, plan->beforeHash);
    zr_container_hash_u64(&hash, plan->afterHash);
    zr_container_hash_u32(&hash, (TZrUInt32)plan->mapStrategy);
    zr_container_hash_u32(&hash, (TZrUInt32)plan->stringStrategy);
    zr_container_hash_bool(&hash, plan->mapKnown);
    zr_container_hash_bool(&hash, plan->stringKnown);
    zr_container_hash_bool(&hash, plan->cacheHash);
    zr_container_hash_bool(&hash, plan->preservesGenericEquality);
    zr_container_hash_bool(&hash, plan->irUnchanged);
    zr_container_hash_bool(&hash, plan->specialized);
    zr_container_hash_map(&hash, &plan->mapCandidate);
    zr_container_hash_string_candidate(&hash, &plan->stringCandidate);
    return hash == 0u ? UINT64_C(1) : hash;
}

TZrBool ZrParser_ExecIr_ValidateContainerSpecializationPlan(
        const SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        const SZrContainerSpecializationPlan *plan,
        SZrContainerSpecializationDiagnostic *diagnostic) {
    SZrContainerStorageDiagnostic storage;
    SZrContainerSpecializationPlan expectedPlan;
    SZrContainerSpecializationDiagnostic expectedDiagnostic;
    TZrUInt64 expectedHash;
    ZrParser_ExecIr_ContainerSpecializationDiagnosticInit(diagnostic);
    if (plan == ZR_NULL || function == ZR_NULL || facts == ZR_NULL) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_INVALID_ARGUMENT,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (!zr_container_facts_envelope_valid(facts, diagnostic) ||
        !zr_container_check_function(function, facts, diagnostic)) return ZR_FALSE;
    if (plan->magic != ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAGIC ||
        plan->schemaVersion != ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_VERSION ||
        plan->preservesGenericEquality == ZR_FALSE || plan->irUnchanged == ZR_FALSE) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_MISMATCH,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (plan->mapStrategy > ZR_EXEC_IR_CONTAINER_MAP_COMPACT ||
        plan->stringStrategy > ZR_STRING_STORAGE_STRATEGY_ROPE) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNSUPPORTED,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        return ZR_FALSE;
    }
    if (plan->mapStrategy == ZR_EXEC_IR_CONTAINER_MAP_COMPACT) {
        ZrCore_ContainerStorage_DiagnosticClear(&storage);
        if (!ZrCore_CompactMapCandidate_Validate(&plan->mapCandidate, &storage)) {
            zr_container_diag_set(diagnostic,
                                  zr_container_map_status(storage.code),
                                  ZR_EXEC_IR_CONTAINER_KIND_MAP, facts, function,
                                  &storage);
            return ZR_FALSE;
        }
        if (plan->cacheHash != ZR_FALSE &&
            !ZrCore_CompactMap_CanCacheHash(&plan->mapCandidate.hash)) {
            zr_container_diag_set(diagnostic,
                                  ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_HASH_UNSTABLE,
                                  ZR_EXEC_IR_CONTAINER_KIND_MAP, facts, function,
                                  &storage);
            return ZR_FALSE;
        }
    }
    if (plan->stringStrategy != ZR_STRING_STORAGE_STRATEGY_GENERIC) {
        ZrCore_ContainerStorage_DiagnosticClear(&storage);
        if (!ZrCore_StringStorage_ValidateCandidate(&plan->stringCandidate,
                                                    &storage)) {
            zr_container_diag_set(diagnostic,
                                  zr_container_string_status(storage.code),
                                  ZR_EXEC_IR_CONTAINER_KIND_STRING, facts, function,
                                  &storage);
            return ZR_FALSE;
        }
        if (plan->stringCandidate.strategy != plan->stringStrategy) {
            zr_container_diag_set(diagnostic,
                                  ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH,
                                  ZR_EXEC_IR_CONTAINER_KIND_STRING, facts, function,
                                  ZR_NULL);
            return ZR_FALSE;
        }
    }
    /* Rebuild from the current function/facts and compare the complete value
     * hash.  This catches a plan copied across a changed generation, layout,
     * candidate, or fallback reason even when an attacker recomputes only a
     * nested core candidate hash. */
    if (!ZrParser_ExecIr_BuildContainerSpecialization(
                function, facts, &expectedPlan, &expectedDiagnostic)) {
        if (diagnostic != ZR_NULL) *diagnostic = expectedDiagnostic;
        return ZR_FALSE;
    }
    if (expectedPlan.planHash != plan->planHash ||
        expectedPlan.beforeHash != plan->beforeHash ||
        expectedPlan.afterHash != plan->afterHash) {
        zr_container_diag_set(diagnostic,
                              ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH,
                              ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
        if (diagnostic != ZR_NULL) {
            diagnostic->execution.expectedHash = expectedPlan.planHash;
            diagnostic->execution.actualHash = plan->planHash;
        }
        return ZR_FALSE;
    }
    expectedHash = ZrParser_ExecIr_ContainerSpecializationPlanHash(plan);
    if (plan->planHash == 0u || plan->planHash != expectedHash) {
        if (diagnostic != ZR_NULL) {
            zr_container_diag_set(diagnostic,
                                  ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH,
                                  ZR_EXEC_IR_CONTAINER_KIND_NONE, facts, function, ZR_NULL);
            diagnostic->execution.expectedHash = expectedHash;
            diagnostic->execution.actualHash = plan->planHash;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SpecializeContainers(
        SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        SZrExecIrDiagnostic *diagnostic) {
    SZrContainerSpecializationPlan plan;
    SZrContainerSpecializationDiagnostic detail;
    TZrBool result;
    result = ZrParser_ExecIr_BuildContainerSpecialization(
            function, facts, &plan, &detail);
    if (diagnostic != ZR_NULL) *diagnostic = detail.execution;
    if (!result) return ZR_FALSE;
    /* There is intentionally no container opcode to rewrite in this ExecIR
     * schema.  Report whether a non-generic candidate was admitted; the
     * generic path remains semantically available in either case. */
    return plan.specialized;
}
