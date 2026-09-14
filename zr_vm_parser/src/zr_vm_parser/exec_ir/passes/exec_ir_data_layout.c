#include "zr_vm_parser/exec_ir_aggregate_layout.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define ZR_AGGREGATE_DATA_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_AGGREGATE_DATA_FNV_PRIME UINT64_C(1099511628211)

static void zr_data_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_AGGREGATE_DATA_FNV_PRIME;
    }
}

static void zr_data_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_AGGREGATE_DATA_FNV_PRIME;
    }
}

static void zr_data_diag(SZrExecIrAggregateDiagnostic *diagnostic,
                         EZrExecIrAggregateStatus status,
                         const SZrExecIrAggregateFacts *facts,
                         TZrUInt32 fieldIndex) {
    if (diagnostic == ZR_NULL) return;
    ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    diagnostic->status = status;
    diagnostic->fieldIndex = fieldIndex;
    if (facts != ZR_NULL) {
        diagnostic->sourceId = facts->sourceId;
        diagnostic->instructionId = facts->instructionId;
        diagnostic->execution.functionToken = facts->functionToken;
        diagnostic->execution.blockId = facts->blockId;
        diagnostic->execution.instructionId = facts->instructionId;
        diagnostic->execution.sourceId = facts->sourceId;
    }
}

static TZrBool zr_data_u64_add(TZrUInt64 left, TZrUInt64 right,
                               TZrUInt64 *result) {
    if (result == ZR_NULL || right > UINT64_MAX - left) return ZR_FALSE;
    *result = left + right;
    return ZR_TRUE;
}

static TZrBool zr_data_align_up_u32(TZrUInt32 value, TZrUInt32 alignment,
                                    TZrUInt32 *result) {
    TZrUInt32 remainder;
    TZrUInt32 padding;
    if (result == ZR_NULL || alignment == 0u ||
        (alignment & (alignment - 1u)) != 0u) return ZR_FALSE;
    remainder = value & (alignment - 1u);
    padding = remainder == 0u ? 0u : alignment - remainder;
    if (padding > UINT32_MAX - value) return ZR_FALSE;
    *result = value + padding;
    return ZR_TRUE;
}

static TZrBool zr_data_structural(const SZrExecIrAggregateFacts *facts,
                                  SZrExecIrAggregateDiagnostic *diagnostic) {
    return ZrParser_ExecIr_AggregateFactsValidate(facts, diagnostic);
}

static TZrBool zr_data_layout_gate(const SZrExecIrAggregateFacts *facts,
                                   const SZrExecIrDataLayoutCost *cost,
                                   SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt64 totalCost;
    if (!zr_data_structural(facts, diagnostic)) return ZR_FALSE;
    if ((facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_UNION_OR_OVERLAY) != 0u) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->privateClosedWorld || facts->publicLayout || facts->reflectionVisible ||
        facts->ffiVisible || facts->serializationObserved ||
        (facts->flags & (ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_LAYOUT |
                         ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_SLICE |
                         ZR_EXEC_IR_AGGREGATE_FLAG_REFLECTION_VISIBLE |
                         ZR_EXEC_IR_AGGREGATE_FLAG_FFI_VISIBLE |
                         ZR_EXEC_IR_AGGREGATE_FLAG_SERIALIZATION_VISIBLE)) != 0u) {
        if (facts->reflectionVisible ||
            (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_REFLECTION_VISIBLE) != 0u)
            zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_REFLECTION_VISIBLE,
                         facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        else if (facts->ffiVisible ||
                 (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_FFI_VISIBLE) != 0u)
            zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE,
                         facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        else if (facts->serializationObserved ||
                 (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_SERIALIZATION_VISIBLE) != 0u)
            zr_data_diag(diagnostic,
                         ZR_EXEC_IR_AGGREGATE_SERIALIZATION_VISIBLE,
                         facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        else
            zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_PUBLIC_LAYOUT,
                         facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->closedLifetime) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_LIFETIME_OPEN,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->noUnknownEscape ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_UNKNOWN_ESCAPE) != 0u) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->noAddressObservation ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_ADDRESS_OBSERVED) != 0u) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->reconstructibleIdentity) {
        zr_data_diag(diagnostic,
                     ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_IDENTITY_OBSERVED) != 0u &&
        facts->identityToken == 0u) {
        zr_data_diag(diagnostic,
                     ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_ALIAS_OBSERVED) != 0u &&
        !facts->aliasProven) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (cost == ZR_NULL) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_PROFILE_MISSING,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!cost->profileAvailable || cost->elementCount == 0u ||
        cost->loopTripCount == 0u) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_PROFILE_MISSING,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (cost->evidence != ZR_EXEC_IR_AGGREGATE_EVIDENCE_ESTIMATED &&
        cost->evidence != ZR_EXEC_IR_AGGREGATE_EVIDENCE_MEASURED) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (cost->evidence == ZR_EXEC_IR_AGGREGATE_EVIDENCE_MEASURED &&
        (cost->measuredCacheMissesBefore ==
             ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT ||
         cost->measuredCacheMissesAfter ==
             ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT)) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_PROFILE_MISSING,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (cost->estimatedBridgeCost > cost->estimatedLocalityBenefit) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_BRIDGE_TOO_EXPENSIVE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = cost->estimatedLocalityBenefit;
            diagnostic->actualHash = cost->estimatedBridgeCost;
        }
        return ZR_FALSE;
    }
    totalCost = cost->estimatedBridgeCost;
    if (!zr_data_u64_add(totalCost, cost->estimatedMoveBytes, &totalCost)) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_COST_UNPROFITABLE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (totalCost >= cost->estimatedLocalityBenefit) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_COST_UNPROFITABLE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[index];
        if ((field->flags & (ZR_EXEC_IR_AGGREGATE_FIELD_ADDRESS_OBSERVED |
                             ZR_EXEC_IR_AGGREGATE_FIELD_NATIVE_RETAINED |
                             ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE |
                             ZR_EXEC_IR_AGGREGATE_FIELD_OVERLAY |
                             ZR_EXEC_IR_AGGREGATE_FIELD_BORROWED_ACROSS_SUSPEND)) != 0u) {
            EZrExecIrAggregateStatus status =
                (field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE) != 0u
                    ? ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE
                    : ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_OVERLAY) != 0u
                           ? ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY
                           : ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED);
            zr_data_diag(diagnostic, status, facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED) != 0u &&
            (!field->aliasProven || !facts->aliasProven ||
             field->aliasClass == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ALIAS ||
             field->aliasLocation.unknownWrite || field->aliasLocation.escaped ||
             !field->aliasLocation.hasStableBase ||
             (field->aliasLocation.generation != 0u &&
              field->aliasLocation.generation != facts->generation))) {
            zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
                         facts, index);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void ZrParser_ExecIr_DataLayoutCostInit(SZrExecIrDataLayoutCost *cost) {
    if (cost == ZR_NULL) return;
    memset(cost, 0, sizeof(*cost));
    cost->measuredCacheMissesBefore = ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT;
    cost->measuredCacheMissesAfter = ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT;
    cost->evidence = ZR_EXEC_IR_AGGREGATE_EVIDENCE_ESTIMATED;
}

void ZrParser_ExecIr_DataLayoutCandidateInit(
        SZrExecIrDataLayoutCandidate *candidate) {
    TZrUInt32 index;
    if (candidate == ZR_NULL) return;
    memset(candidate, 0, sizeof(*candidate));
    candidate->magic = ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC;
    candidate->schemaVersion = ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION;
    candidate->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS;
    candidate->fallbackReason = ZR_EXEC_IR_AGGREGATE_OK;
    candidate->measuredCacheMissesBefore = ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT;
    candidate->measuredCacheMissesAfter = ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT;
    candidate->evidence = ZR_EXEC_IR_AGGREGATE_EVIDENCE_ESTIMATED;
    for (index = 0u; index < ZR_EXEC_IR_AGGREGATE_MAX_FIELDS; ++index)
        candidate->fieldOrder[index] = ZR_EXEC_IR_AGGREGATE_INVALID_INDEX;
}

TZrBool ZrParser_ExecIr_DataLayoutCanUseSoA(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCost *cost,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    return zr_data_layout_gate(facts, cost, diagnostic);
}

static void zr_data_sorted_order(const SZrExecIrAggregateFacts *facts,
                                 TZrUInt32 *order) {
    TZrUInt32 index;
    for (index = 0u; index < facts->fieldCount; ++index) order[index] = index;
    for (index = 1u; index < facts->fieldCount; ++index) {
        TZrUInt32 value = order[index];
        TZrUInt32 cursor = index;
        while (cursor != 0u) {
            TZrUInt32 previous = order[cursor - 1u];
            const SZrExecIrAggregateFieldFact *left = &facts->fields[previous];
            const SZrExecIrAggregateFieldFact *right = &facts->fields[value];
            if (left->loopUseCount > right->loopUseCount ||
                (left->loopUseCount == right->loopUseCount &&
                 left->logicalIndex < right->logicalIndex)) break;
            order[cursor] = previous;
            --cursor;
        }
        order[cursor] = value;
    }
}

static TZrUInt64 zr_data_physical_hash(const SZrExecIrAggregateFacts *facts,
                                       const TZrUInt32 *order,
                                       EZrExecIrAggregateStrategy strategy,
                                       TZrUInt32 bridgeVersion) {
    TZrUInt64 hash = ZR_AGGREGATE_DATA_FNV_OFFSET;
    TZrUInt32 index;
    zr_data_hash_u32(&hash, (TZrUInt32)strategy);
    zr_data_hash_u32(&hash, bridgeVersion);
    zr_data_hash_u64(&hash, facts->logicalLayoutHash);
    {
        TZrUInt32 runningOffset = 0u;
        for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[order[index]];
        TZrUInt32 physicalOffset = field->logicalOffset;
        if (strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA) {
            if (!zr_data_align_up_u32(runningOffset, field->byteAlign,
                                      &physicalOffset) ||
                field->byteSize > UINT32_MAX - physicalOffset)
                return 1u;
            runningOffset = physicalOffset + field->byteSize;
        }
        zr_data_hash_u32(&hash, field->logicalIndex);
        zr_data_hash_u32(&hash, field->byteSize);
        zr_data_hash_u32(&hash, field->byteAlign);
        zr_data_hash_u32(&hash, index);
        zr_data_hash_u32(&hash, physicalOffset);
        }
    }
    return hash == 0u ? 1u : hash;
}

TZrUInt64 ZrParser_ExecIr_DataLayoutCandidateHash(
        const SZrExecIrDataLayoutCandidate *candidate) {
    TZrUInt64 hash = ZR_AGGREGATE_DATA_FNV_OFFSET;
    TZrUInt32 index;
    if (candidate == ZR_NULL) return 0u;
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->strategy);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->fallbackReason);
    zr_data_hash_u32(&hash, candidate->logicalTypeToken);
    zr_data_hash_u32(&hash, candidate->logicalLayoutId);
    zr_data_hash_u64(&hash, candidate->logicalLayoutHash);
    zr_data_hash_u64(&hash, candidate->physicalLayoutHash);
    zr_data_hash_u64(&hash, candidate->generation);
    zr_data_hash_u32(&hash, candidate->bridgeVersion);
    zr_data_hash_u32(&hash, candidate->fieldCount);
    for (index = 0u; index < candidate->fieldCount &&
                      index < ZR_EXEC_IR_AGGREGATE_MAX_FIELDS; ++index) {
        zr_data_hash_u32(&hash, candidate->fieldOrder[index]);
        zr_data_hash_u32(&hash, candidate->columnStride[index]);
        zr_data_hash_u32(&hash, candidate->columnOffset[index]);
    }
    zr_data_hash_u64(&hash, candidate->estimatedMoveBytes);
    zr_data_hash_u64(&hash, candidate->estimatedBridgeCost);
    zr_data_hash_u64(&hash, candidate->estimatedLocalityBenefit);
    zr_data_hash_u64(&hash, candidate->measuredCacheMissesBefore);
    zr_data_hash_u64(&hash, candidate->measuredCacheMissesAfter);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->evidence);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->closedLifetime);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->ownershipMigrated);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->rootsMigrated);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->dropOrderPreserved);
    zr_data_hash_u32(&hash, (TZrUInt32)candidate->requiresMaterialization);
    return hash == 0u ? 1u : hash;
}

TZrBool ZrParser_ExecIr_DataLayoutBuildCandidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCost *cost,
        SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 order[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    SZrExecIrAggregateDiagnostic gateDiagnostic;
    TZrBool useSoA;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (facts == ZR_NULL || candidate == ZR_NULL) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_data_structural(facts, diagnostic)) return ZR_FALSE;
    ZrParser_ExecIr_DataLayoutCandidateInit(candidate);
    candidate->logicalTypeToken = facts->logicalTypeToken;
    candidate->logicalLayoutId = facts->logicalLayoutId;
    candidate->logicalLayoutHash = facts->logicalLayoutHash;
    candidate->generation = facts->generation;
    candidate->bridgeVersion = facts->bridgeVersion;
    candidate->fieldCount = facts->fieldCount;
    candidate->ownershipMigrated = ZR_TRUE;
    candidate->rootsMigrated = ZR_TRUE;
    candidate->dropOrderPreserved = ZR_TRUE;
    zr_data_sorted_order(facts, order);
    useSoA = zr_data_layout_gate(facts, cost, &gateDiagnostic);
    if (useSoA) {
        candidate->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA;
        candidate->closedLifetime = ZR_TRUE;
        candidate->fallbackReason = ZR_EXEC_IR_AGGREGATE_OK;
    } else {
        candidate->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS;
        candidate->closedLifetime = facts->closedLifetime;
        candidate->fallbackReason = gateDiagnostic.status;
        if (diagnostic != ZR_NULL) *diagnostic = gateDiagnostic;
    }
    candidate->requiresMaterialization =
        (TZrBool)((facts->flags & (ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY |
                                   ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY |
                                   ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY)) != 0u);
    {
        TZrUInt32 runningOffset = 0u;
        for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[order[index]];
        candidate->fieldOrder[index] = order[index];
        candidate->columnOffset[index] = field->logicalOffset;
        if (candidate->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA) {
            if (!zr_data_align_up_u32(runningOffset, field->byteAlign,
                                      &candidate->columnOffset[index]) ||
                field->byteSize > UINT32_MAX - candidate->columnOffset[index]) {
                zr_data_diag(diagnostic,
                             ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                             facts, order[index]);
                return ZR_FALSE;
            }
            runningOffset = candidate->columnOffset[index] + field->byteSize;
        }
        candidate->columnStride[index] = field->byteSize;
        }
    }
    candidate->physicalLayoutHash =
        zr_data_physical_hash(facts, order, candidate->strategy,
                              candidate->bridgeVersion);
    if (cost != ZR_NULL) {
        candidate->estimatedMoveBytes = cost->estimatedMoveBytes;
        candidate->estimatedBridgeCost = cost->estimatedBridgeCost;
        candidate->estimatedLocalityBenefit = cost->estimatedLocalityBenefit;
        candidate->measuredCacheMissesBefore = cost->measuredCacheMissesBefore;
        candidate->measuredCacheMissesAfter = cost->measuredCacheMissesAfter;
        candidate->evidence = cost->evidence;
    }
    candidate->planHash = ZrParser_ExecIr_DataLayoutCandidateHash(candidate);
    return ZR_TRUE;
}

static TZrBool zr_data_candidate_order_valid(
        const SZrExecIrDataLayoutCandidate *candidate) {
    TZrUInt32 index;
    TZrUInt32 seen[ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS * 2u];
    memset(seen, 0, sizeof(seen));
    for (index = 0u; index < candidate->fieldCount; ++index) {
        TZrUInt32 value = candidate->fieldOrder[index];
        TZrUInt32 word;
        TZrUInt32 bit;
        if (value >= candidate->fieldCount) return ZR_FALSE;
        word = value / 32u;
        bit = value % 32u;
        if ((seen[word] & ((TZrUInt32)1u << bit)) != 0u) return ZR_FALSE;
        seen[word] |= (TZrUInt32)1u << bit;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_DataLayoutCandidateValidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt64 expectedHash;
    TZrUInt64 expectedPhysicalHash;
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (facts == ZR_NULL || candidate == ZR_NULL) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_data_structural(facts, diagnostic)) return ZR_FALSE;
    if (candidate->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        candidate->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        (candidate->strategy != ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS &&
         candidate->strategy != ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA)) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (candidate->fieldCount != facts->fieldCount ||
        candidate->logicalTypeToken != facts->logicalTypeToken ||
        candidate->logicalLayoutId != facts->logicalLayoutId ||
        candidate->logicalLayoutHash != facts->logicalLayoutHash ||
        candidate->generation != facts->generation ||
        candidate->bridgeVersion != facts->bridgeVersion) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_GENERATION_STALE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_data_candidate_order_valid(candidate)) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    expectedPhysicalHash = zr_data_physical_hash(
            facts, candidate->fieldOrder, candidate->strategy,
            candidate->bridgeVersion);
    if (candidate->physicalLayoutHash != expectedPhysicalHash) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expectedPhysicalHash;
            diagnostic->actualHash = candidate->physicalLayoutHash;
        }
        return ZR_FALSE;
    }
    if (!candidate->ownershipMigrated || !candidate->rootsMigrated ||
        !candidate->dropOrderPreserved) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    {
        TZrBool expectedMaterialization =
            (TZrBool)((facts->flags & (ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY |
                                       ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY |
                                       ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY)) != 0u);
        if (candidate->requiresMaterialization != expectedMaterialization) {
            zr_data_diag(diagnostic,
                         ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                         facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
            return ZR_FALSE;
        }
    }
    if (candidate->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA &&
        (!facts->closedLifetime || !facts->noUnknownEscape ||
         !facts->noAddressObservation)) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_LIFETIME_OPEN,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    for (index = 0u; index < candidate->fieldCount; ++index) {
        TZrUInt32 fieldIndex = candidate->fieldOrder[index];
        if (candidate->columnStride[index] == 0u ||
            candidate->columnStride[index] != facts->fields[fieldIndex].byteSize) {
            zr_data_diag(diagnostic,
                         ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                         facts, fieldIndex);
            return ZR_FALSE;
        }
        if (candidate->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS &&
            candidate->columnOffset[index] != facts->fields[fieldIndex].logicalOffset) {
            zr_data_diag(diagnostic,
                         ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                         facts, fieldIndex);
            return ZR_FALSE;
        }
        if (candidate->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA) {
            TZrUInt32 expectedOffset;
            TZrUInt32 runningOffset = 0u;
            TZrUInt32 cursor;
            for (cursor = 0u; cursor < index; ++cursor) {
                const SZrExecIrAggregateFieldFact *prior =
                    &facts->fields[candidate->fieldOrder[cursor]];
                if (!zr_data_align_up_u32(runningOffset, prior->byteAlign,
                                          &expectedOffset) ||
                    prior->byteSize > UINT32_MAX - expectedOffset) {
                    zr_data_diag(diagnostic,
                                 ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                                 facts, fieldIndex);
                    return ZR_FALSE;
                }
                runningOffset = expectedOffset + prior->byteSize;
            }
            if (!zr_data_align_up_u32(runningOffset,
                                      facts->fields[fieldIndex].byteAlign,
                                      &expectedOffset) ||
                candidate->columnOffset[index] != expectedOffset) {
                zr_data_diag(diagnostic,
                             ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                             facts, fieldIndex);
                return ZR_FALSE;
            }
        }
    }
    expectedHash = ZrParser_ExecIr_DataLayoutCandidateHash(candidate);
    if (candidate->planHash != expectedHash) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expectedHash;
            diagnostic->actualHash = candidate->planHash;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 zr_plan_hash(const SZrExecIrLayoutTransformPlan *plan) {
    TZrUInt64 hash = ZR_AGGREGATE_DATA_FNV_OFFSET;
    zr_data_hash_u32(&hash, (TZrUInt32)plan->strategy);
    zr_data_hash_u32(&hash, (TZrUInt32)plan->fallbackReason);
    zr_data_hash_u32(&hash, plan->logicalTypeToken);
    zr_data_hash_u32(&hash, plan->logicalLayoutId);
    zr_data_hash_u64(&hash, plan->logicalLayoutHash);
    zr_data_hash_u64(&hash, plan->physicalLayoutHash);
    zr_data_hash_u64(&hash, plan->generation);
    zr_data_hash_u32(&hash, plan->bridgeVersion);
    zr_data_hash_u32(&hash, plan->fieldCount);
    zr_data_hash_u32(&hash, plan->flags);
    zr_data_hash_u32(&hash, (TZrUInt32)plan->requiresMaterialization);
    return hash == 0u ? 1u : hash;
}

TZrBool ZrParser_ExecIr_PlanAggregateLayout(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCost *cost,
        SZrExecIrLayoutTransformPlan *plan,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    SZrExecIrAggregateDiagnostic local;
    SZrExecIrSroaCandidate sroa;
    SZrExecIrDataLayoutCandidate data;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (facts == ZR_NULL || plan == ZR_NULL) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_data_structural(facts, diagnostic)) return ZR_FALSE;
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC;
    plan->schemaVersion = ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION;
    plan->logicalTypeToken = facts->logicalTypeToken;
    plan->logicalLayoutId = facts->logicalLayoutId;
    plan->logicalLayoutHash = facts->logicalLayoutHash;
    plan->generation = facts->generation;
    plan->bridgeVersion = facts->bridgeVersion;
    plan->fieldCount = facts->fieldCount;
    plan->ownershipMigrated = ZR_TRUE;
    plan->rootsMigrated = ZR_TRUE;
    plan->dropOrderPreserved = ZR_TRUE;
    if (ZrParser_ExecIr_SroaCanScalarize(facts, &local)) {
        if (cost != ZR_NULL &&
            ZrParser_ExecIr_DataLayoutCanUseSoA(facts, cost, &local) &&
            ZrParser_ExecIr_DataLayoutBuildCandidate(facts, cost, &data, &local)) {
            plan->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA;
            plan->physicalLayoutHash = data.physicalLayoutHash;
            plan->requiresMaterialization =
                (TZrBool)((facts->flags & (ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY |
                                            ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY |
                                            ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY)) != 0u);
            plan->flags = facts->flags;
        } else if (ZrParser_ExecIr_SroaBuildCandidate(facts, &sroa, &local)) {
            plan->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA;
            plan->physicalLayoutHash = facts->logicalLayoutHash;
            plan->requiresMaterialization = ZR_FALSE;
            plan->flags = facts->flags;
        } else {
            plan->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC;
            plan->fallbackReason = local.status;
        }
    } else {
        plan->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC;
        plan->fallbackReason = local.status;
        if (diagnostic != ZR_NULL) *diagnostic = local;
    }
    plan->planHash = zr_plan_hash(plan);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ApplyAggregateLayout(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrLayoutTransformPlan *plan,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt64 expectedHash;
    SZrExecIrAggregateDiagnostic local;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (facts == ZR_NULL || plan == ZR_NULL) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_data_structural(facts, diagnostic)) return ZR_FALSE;
    if (plan->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        plan->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        plan->fieldCount != facts->fieldCount ||
        plan->logicalTypeToken != facts->logicalTypeToken ||
        plan->logicalLayoutId != facts->logicalLayoutId ||
        plan->logicalLayoutHash != facts->logicalLayoutHash ||
        plan->generation != facts->generation ||
        plan->bridgeVersion != facts->bridgeVersion) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_GENERATION_STALE,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    expectedHash = zr_plan_hash(plan);
    if (plan->planHash != expectedHash) {
        zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH,
                     facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expectedHash;
            diagnostic->actualHash = plan->planHash;
        }
        return ZR_FALSE;
    }
    if (plan->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC ||
        plan->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS) {
        return ZR_TRUE;
    }
    if (plan->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA) {
        SZrExecIrSroaCandidate candidate;
        if (!ZrParser_ExecIr_SroaBuildCandidate(facts, &candidate, &local)) {
            if (diagnostic != ZR_NULL) *diagnostic = local;
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (plan->strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA) {
        if (!facts->closedLifetime || !facts->noUnknownEscape ||
            !facts->noAddressObservation || !facts->reconstructibleIdentity) {
            zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                         facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    zr_data_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNSUPPORTED,
                 facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
    return ZR_FALSE;
}
